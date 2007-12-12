/*
 *  einit-feedback-visual-psplash.c
 *  einit
 *
 *  Created by Magnus Deininger on 12/12/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, 2007, Magnus Deininger
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
#include <einit/module-logic.h>
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

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_feedback_visual_psplash_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_feedback_visual_psplash_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_feedback,
 .name      = "visual/{psplash|usplash|exquisite}-based feedback module",
 .rid       = "einit-feedback-visual-psplash",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_feedback_visual_psplash_configure
};

module_register(einit_feedback_visual_psplash_self);

#endif

enum splash_type {
 sp_psplash,
 sp_usplash,
 sp_exquisite,
 sp_disabled
};

pthread_t psplash_thread;
char einit_feedback_visual_psplash_worker_thread_running = 0,
 einit_feedback_visual_psplash_worker_thread_keep_running = 1;


char **psplash_commandQ = NULL;
char *psplash_fifo = "/dev/psplash_fifo";

pthread_mutex_t
  psplash_commandQ_mutex = PTHREAD_MUTEX_INITIALIZER,
  psplash_commandQ_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t psplash_commandQ_cond = PTHREAD_COND_INITIALIZER;

int einit_feedback_visual_psplash_disable ();
int einit_feedback_visual_psplash_enable ();

int psplash_mode_switches = 0;
enum splash_type psplash_type = sp_disabled;

void psplash_queue_comand (const char *command) {
 if (psplash_type != sp_disabled) {
  emutex_lock (&psplash_commandQ_mutex);
  psplash_commandQ = (char **)setadd ((void **)psplash_commandQ, command, SET_TYPE_STRING);
  emutex_unlock (&psplash_commandQ_mutex);

  pthread_cond_broadcast (&psplash_commandQ_cond);
 }
}

void einit_feedback_visual_psplash_power_event_handler(struct einit_event *ev) {
 notice (4, "disabling feedack (psplash)");
 einit_feedback_visual_psplash_disable();
}

void einit_feedback_visual_psplash_boot_event_handler_boot_devices_available (struct einit_event *ev) {
 einit_feedback_visual_psplash_enable();
}

void einit_feedback_visual_psplash_einit_event_handler_mode_switching (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 if (ev->para && ((struct cfgnode *)ev->para)->id) {
  esprintf (tmp, BUFFERSIZE, "MSG eINIT now switching to mode %s", ((struct cfgnode *)ev->para)->id);
  psplash_queue_comand(tmp);
 }

 if (!psplash_mode_switches) {
  psplash_queue_comand("PROGRESS 0");
 }

 psplash_mode_switches++;
}

void einit_feedback_visual_psplash_einit_event_handler_mode_switch_done (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 psplash_mode_switches--;

 if (ev->para && ((struct cfgnode *)ev->para)->id) {
  esprintf (tmp, BUFFERSIZE, "MSG mode %s now in effect.", ((struct cfgnode *)ev->para)->id);
  psplash_queue_comand(tmp);
 }

 if (!psplash_mode_switches) {
  psplash_queue_comand("PROGRESS 100");
  psplash_queue_comand("QUIT");
 }
}

void einit_feedback_visual_psplash_einit_event_handler_service_update (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "PROGRESS %i", (int)(get_plan_progress (NULL) * 100));
 psplash_queue_comand(tmp);
}

void *einit_feedback_visual_psplash_worker_thread (void *irr) {
 einit_feedback_visual_psplash_worker_thread_running = 1;

 while (einit_feedback_visual_psplash_worker_thread_keep_running) {
  while (psplash_commandQ) {
   char *command = NULL;

   emutex_lock (&psplash_commandQ_mutex);
   if (psplash_commandQ) {
    if ((command = psplash_commandQ[0])) {
     void *it = command;
     command = estrdup (command);

     psplash_commandQ = (char **)setdel ((void **)psplash_commandQ, it);
    }
   }
   emutex_unlock (&psplash_commandQ_mutex);

   if (command) {
    int fd = open (psplash_fifo, O_WRONLY | O_NONBLOCK);
    if (fd != -1) {
     write (fd, command, strlen(command) +1); /* \0 terminates commands in psplash */
     eclose (fd);
    }

    efree (command);
   }
  }

  emutex_lock (&psplash_commandQ_cond_mutex);
  pthread_cond_wait (&psplash_commandQ_cond, &psplash_commandQ_cond_mutex);
  emutex_unlock (&psplash_commandQ_cond_mutex);
 }

 return NULL;
}

int einit_feedback_visual_psplash_enable () {
 char *tmp = NULL;

 einit_feedback_visual_psplash_worker_thread_keep_running = 1;
 ethread_create (&psplash_thread, NULL, einit_feedback_visual_psplash_worker_thread, NULL);

 if ((tmp = cfg_getstring ("configuration-feedback-psplash", NULL))) {
  if (strmatch (tmp, "psplash")) {
   psplash_type = sp_psplash;
  } else if (strmatch (tmp, "usplash")) {
   psplash_type = sp_usplash;
  } else if (strmatch (tmp, "exquisite")) {
   psplash_type = sp_exquisite;
  } else
   psplash_type = sp_disabled;
 }

 switch (psplash_type) {
  case sp_psplash:
   if ((tmp = cfg_getstring ("configuration-feedback-visual-psplash-fifo", NULL))) {
    psplash_fifo = tmp;
   }

   if ((tmp = cfg_getstring ("configuration-feedback-visual-psplash-run", NULL))) {
    system (tmp);
   } else {
    psplash_type = sp_disabled;
   }
   break;

  case sp_usplash:
   if ((tmp = cfg_getstring ("configuration-feedback-visual-usplash-fifo", NULL))) {
    psplash_fifo = tmp;
   }

   if ((tmp = cfg_getstring ("configuration-feedback-visual-usplash-run", NULL))) {
    system (tmp);
   } else {
    psplash_type = sp_disabled;
   }
   break;

  case sp_exquisite:
   if ((tmp = cfg_getstring ("configuration-feedback-visual-exquisite-fifo", NULL))) {
    psplash_fifo = tmp;
   }

   if ((tmp = cfg_getstring ("configuration-feedback-visual-exquisite-run", NULL))) {
    system (tmp);
   } else {
    psplash_type = sp_disabled;
   }
   break;

  default:
   psplash_type = sp_disabled;
   break;
 }

 psplash_queue_comand("PROGRESS 0");

 return status_ok;
}

int einit_feedback_visual_psplash_disable () {
 einit_feedback_visual_psplash_worker_thread_keep_running = 0;
 pthread_cond_broadcast (&psplash_commandQ_cond);

 return status_ok;
}

int einit_feedback_visual_psplash_cleanup (struct lmodule *tm) {
 event_ignore (einit_boot_devices_available, einit_feedback_visual_psplash_boot_event_handler_boot_devices_available);
 event_ignore (einit_power_down_scheduled, einit_feedback_visual_psplash_power_event_handler);
 event_ignore (einit_power_reset_scheduled, einit_feedback_visual_psplash_power_event_handler);
 event_ignore (einit_core_mode_switching, einit_feedback_visual_psplash_einit_event_handler_mode_switching);
 event_ignore (einit_core_mode_switch_done, einit_feedback_visual_psplash_einit_event_handler_mode_switch_done);
 event_ignore (einit_core_service_update, einit_feedback_visual_psplash_einit_event_handler_service_update);

 return 0;
}

int einit_feedback_visual_psplash_configure (struct lmodule *tm) {
 module_init (tm);
 module_logic_configure(tm);

 tm->cleanup = einit_feedback_visual_psplash_cleanup;

 event_listen (einit_boot_devices_available, einit_feedback_visual_psplash_boot_event_handler_boot_devices_available);
 event_listen (einit_power_down_scheduled, einit_feedback_visual_psplash_power_event_handler);
 event_listen (einit_power_reset_scheduled, einit_feedback_visual_psplash_power_event_handler);
 event_listen (einit_core_mode_switching, einit_feedback_visual_psplash_einit_event_handler_mode_switching);
 event_listen (einit_core_mode_switch_done, einit_feedback_visual_psplash_einit_event_handler_mode_switch_done);
 event_listen (einit_core_service_update, einit_feedback_visual_psplash_einit_event_handler_service_update);

 return 0;
}
