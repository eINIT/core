/*
 *  einit-feedback-visual-libnotify.c
 *  einit
 *
 *  Created by Ryan Hope on 2/11/2008.
 *  Copyright 2008 Ryan Hope. All rights reserved.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be
	  used to endorse or promote products derived from this software without
	  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <einit/bitch.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_feedback_visual_libnotify_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_feedback_visual_libnotify_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_feedback,
 .name      = "libnotify feedback",
 .rid       = "einit-feedback-visual-libnotify",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_feedback_visual_libnotify_configure
};

module_register(einit_feedback_visual_libnotify_self);

#endif

pthread_t libnotify_thread;
char einit_feedback_visual_libnotify_worker_thread_running = 0,
 einit_feedback_visual_libnotify_worker_thread_keep_running = 1;


char **libnotify_commandQ = NULL;

pthread_mutex_t
  libnotify_commandQ_mutex = PTHREAD_MUTEX_INITIALIZER,
  libnotify_commandQ_cond_mutex = PTHREAD_MUTEX_INITIALIZER,
  libnotify_mode_switches_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t libnotify_commandQ_cond = PTHREAD_COND_INITIALIZER;

int einit_feedback_visual_libnotify_disable ();
int einit_feedback_visual_libnotify_enable ();

int libnotify_mode_switches = 0;

void libnotify_queue_command (const char *command) {
 emutex_lock (&libnotify_commandQ_mutex);
 libnotify_commandQ = set_str_add (libnotify_commandQ, (char *)command);
 emutex_unlock (&libnotify_commandQ_mutex);

 pthread_cond_broadcast (&libnotify_commandQ_cond);
}

void einit_feedback_visual_libnotify_power_event_handler(struct einit_event *ev) {
 notice (4, "disabling feedack (libnotify)");
 einit_feedback_visual_libnotify_disable();
}

void einit_feedback_visual_libnotify_boot_event_handler_boot_devices_available (struct einit_event *ev) {
 einit_feedback_visual_libnotify_enable();
}

void einit_feedback_visual_libnotify_einit_event_handler_mode_switching (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 if (ev->para && ((struct cfgnode *)ev->para)->id) {
  esprintf (tmp, BUFFERSIZE, "MSG eINIT now switching to mode %s", ((struct cfgnode *)ev->para)->id);
  libnotify_queue_command(tmp);
 }

 emutex_lock (&libnotify_mode_switches_mutex);
 libnotify_mode_switches++;
 if (libnotify_mode_switches == 1) {
  emutex_unlock (&libnotify_mode_switches_mutex);
  libnotify_queue_command("PROGRESS 0");
 } else
  emutex_unlock (&libnotify_mode_switches_mutex);
}

void einit_feedback_visual_libnotify_einit_event_handler_mode_switch_done (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 emutex_lock (&libnotify_mode_switches_mutex);
 libnotify_mode_switches--;
 emutex_unlock (&libnotify_mode_switches_mutex);

 if (ev->para && ((struct cfgnode *)ev->para)->id) {
  esprintf (tmp, BUFFERSIZE, "MSG mode %s now in effect.", ((struct cfgnode *)ev->para)->id);
  libnotify_queue_command(tmp);
 }

 emutex_lock (&libnotify_mode_switches_mutex);
 if (!libnotify_mode_switches) {
  emutex_unlock (&libnotify_mode_switches_mutex);
  libnotify_queue_command("PROGRESS 100");
  libnotify_queue_command("QUIT");
 } else
  emutex_unlock (&libnotify_mode_switches_mutex);
}

void einit_feedback_visual_libnotify_einit_event_handler_service_update (struct einit_event *ev) {
}

void einit_feedback_visual_libnotify_feedback_switch_progress_handler (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "PROGRESS %i", ev->integer);
 libnotify_queue_command(tmp);
}

void *einit_feedback_visual_libnotify_worker_thread (void *irr) {
 einit_feedback_visual_libnotify_worker_thread_running = 1;

 while (einit_feedback_visual_libnotify_worker_thread_keep_running) {
  while (libnotify_commandQ) {
   char *command = NULL;

   emutex_lock (&libnotify_commandQ_mutex);
   if (libnotify_commandQ) {
    if ((command = libnotify_commandQ[0])) {
     void *it = command;
     command = estrdup (command);

     libnotify_commandQ = (char **)setdel ((void **)libnotify_commandQ, it);
    }
   }
   emutex_unlock (&libnotify_commandQ_mutex);

  }

  emutex_lock (&libnotify_commandQ_cond_mutex);
  pthread_cond_wait (&libnotify_commandQ_cond, &libnotify_commandQ_cond_mutex);
  emutex_unlock (&libnotify_commandQ_cond_mutex);
 }

 return NULL;
}

int einit_feedback_visual_libnotify_enable () {
 char *tmp = NULL;

 einit_feedback_visual_libnotify_worker_thread_keep_running = 1;
 ethread_create (&libnotify_thread, NULL, einit_feedback_visual_libnotify_worker_thread, NULL);

 return status_ok;
}

int einit_feedback_visual_libnotify_disable () {
 einit_feedback_visual_libnotify_worker_thread_keep_running = 0;
 pthread_cond_broadcast (&libnotify_commandQ_cond);

 return status_ok;
}

int einit_feedback_visual_libnotify_cleanup (struct lmodule *tm) {
 exec_cleanup(irr);

 event_ignore (einit_boot_devices_available, einit_feedback_visual_libnotify_boot_event_handler_boot_devices_available);
 event_ignore (einit_power_down_scheduled, einit_feedback_visual_libnotify_power_event_handler);
 event_ignore (einit_power_reset_scheduled, einit_feedback_visual_libnotify_power_event_handler);
 event_ignore (einit_core_mode_switching, einit_feedback_visual_libnotify_einit_event_handler_mode_switching);
 event_ignore (einit_core_mode_switch_done, einit_feedback_visual_libnotify_einit_event_handler_mode_switch_done);
 event_ignore (einit_core_service_update, einit_feedback_visual_libnotify_einit_event_handler_service_update);
 event_ignore (einit_feedback_switch_progress, einit_feedback_visual_libnotify_feedback_switch_progress_handler);

 return 0;
}

int einit_feedback_visual_libnotify_configure (struct lmodule *tm) {
 module_init (tm);
 exec_configure(irr);

 char *tmp;

 if (!(tmp = cfg_getstring ("configuration-feedback-libnotify", NULL)) || (strcmp (tmp, "libnotify") )) {
  return status_configure_failed | status_not_in_use;
 }

 tm->cleanup = einit_feedback_visual_libnotify_cleanup;

 event_listen (einit_boot_devices_available, einit_feedback_visual_libnotify_boot_event_handler_boot_devices_available);
 event_listen (einit_power_down_scheduled, einit_feedback_visual_libnotify_power_event_handler);
 event_listen (einit_power_reset_scheduled, einit_feedback_visual_libnotify_power_event_handler);
 event_listen (einit_core_mode_switching, einit_feedback_visual_libnotify_einit_event_handler_mode_switching);
 event_listen (einit_core_mode_switch_done, einit_feedback_visual_libnotify_einit_event_handler_mode_switch_done);
 event_listen (einit_core_service_update, einit_feedback_visual_libnotify_einit_event_handler_service_update);
 event_listen (einit_feedback_switch_progress, einit_feedback_visual_libnotify_feedback_switch_progress_handler);

 return 0;
}
