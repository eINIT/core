/*
 *  einit-feedback-visual-fbsplash.c
 *  einit
 *
 *  Created by Magnus Deininger on 18/01/2006.
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

#define _MODULE

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

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int _einit_feedback_visual_fbsplash_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)

char *_einit_feedback_visual_fbsplash_provides[] = {"feedback-visual", "feedback-graphical", NULL};
char *_einit_feedback_visual_fbsplash_requires[] = {"mount/system", "splashd", NULL};
char *_einit_feedback_visual_fbsplash_before[]   = {"mount/critical", NULL};
const struct smodule _einit_feedback_visual_fbsplash_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
 .name      = "visual/fbsplash-based feedback module",
 .rid       = "einit-feedback-visual-fbsplash",
 .si        = {
  .provides = _einit_feedback_visual_fbsplash_provides,
  .requires = _einit_feedback_visual_fbsplash_requires,
  .after    = NULL,
  .before   = _einit_feedback_visual_fbsplash_before
 },
 .configure = _einit_feedback_visual_fbsplash_configure
};

module_register(_einit_feedback_visual_fbsplash_self);

#endif

pthread_t fbsplash_thread;
char _einit_feedback_visual_fbsplash_worker_thread_running = 0,
     _einit_feedback_visual_fbsplash_worker_thread_keep_running = 1;

char **fbsplash_commandQ = NULL;

char *fbsplash_fifo = "/lib/splash/cache/.splash";

pthread_mutex_t
 fbsplash_commandQ_mutex = PTHREAD_MUTEX_INITIALIZER,
 fbsplash_commandQ_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fbsplash_commandQ_cond = PTHREAD_COND_INITIALIZER;

void fbsplash_queue_comand (const char *command) {
 emutex_lock (&fbsplash_commandQ_mutex);
 fbsplash_commandQ = (char **)setadd ((void **)fbsplash_commandQ, command, SET_TYPE_STRING);
 emutex_unlock (&fbsplash_commandQ_mutex);

 pthread_cond_broadcast (&fbsplash_commandQ_cond);
}

void _einit_feedback_visual_fbsplash_einit_event_handler(struct einit_event *ev) {
 if (ev->type == EVE_SWITCHING_MODE) {
  char tmp[BUFFERSIZE];

  fbsplash_queue_comand("set mode silent");
  fbsplash_queue_comand("progress 0");
  if (ev->para && ((struct cfgnode *)ev->para)->id) {
   esprintf (tmp, BUFFERSIZE, "set message eINIT now switching to mode %s", ((struct cfgnode *)ev->para)->id);
   fbsplash_queue_comand(tmp);
  }

  fbsplash_queue_comand("repaint");
 }
 if (ev->type == EVE_MODE_SWITCHED) {
  char tmp[BUFFERSIZE];

  if (ev->para && ((struct cfgnode *)ev->para)->id) {
   esprintf (tmp, BUFFERSIZE, "set message mode %s now in effect.", ((struct cfgnode *)ev->para)->id);
   fbsplash_queue_comand(tmp);
  }
  fbsplash_queue_comand("progress 65535");
  fbsplash_queue_comand("repaint");
//  fbsplash_queue_comand("set mode verbose");
 }
 if ((ev->type == EVE_SERVICE_UPDATE) && ev->set) {
  char tmp[BUFFERSIZE];
  uint32_t i = 0;

  if (ev->status & STATUS_WORKING) {
   if (ev->task & MOD_ENABLE) {
    for (; ev->set[i]; i++) {
     esprintf (tmp, BUFFERSIZE, "update_svc %s svc_start", (char *)ev->set[i]);
     fbsplash_queue_comand(tmp);
    }
   } else if (ev->task & MOD_DISABLE) {
    for (; ev->set[i]; i++) {
     esprintf (tmp, BUFFERSIZE, "update_svc %s svc_stop", (char *)ev->set[i]);
     fbsplash_queue_comand(tmp);
    }
   }
  } else {
   if (ev->task & MOD_ENABLE) {
    if (ev->status & STATUS_FAIL) {
     for (; ev->set[i]; i++) {
      esprintf (tmp, BUFFERSIZE, "update_svc %s svc_start_failed", (char *)ev->set[i]);
      fbsplash_queue_comand(tmp);
     }
    } else {
     for (; ev->set[i]; i++) {
      esprintf (tmp, BUFFERSIZE, "update_svc %s svc_started", (char *)ev->set[i]);
      fbsplash_queue_comand(tmp);
     }
    }
   } else if (ev->task & MOD_DISABLE) {
    if (ev->status & STATUS_FAIL) {
     for (; ev->set[i]; i++) {
      esprintf (tmp, BUFFERSIZE, "update_svc %s svc_stop_failed", (char *)ev->set[i]);
      fbsplash_queue_comand(tmp);
     }
    } else {
     for (; ev->set[i]; i++) {
      esprintf (tmp, BUFFERSIZE, "update_svc %s svc_stopped", (char *)ev->set[i]);
      fbsplash_queue_comand(tmp);
     }
    }
   }
  }

// get_plan_progress(NULL): overall progress, 0.0-1.0
  esprintf (tmp, BUFFERSIZE, "progress %i", (int)(get_plan_progress (NULL) * 65535));
  fbsplash_queue_comand(tmp);

  fbsplash_queue_comand("repaint");
 }
}

void *_einit_feedback_visual_fbsplash_worker_thread (void *irr) {
 _einit_feedback_visual_fbsplash_worker_thread_running = 1;

 while (_einit_feedback_visual_fbsplash_worker_thread_keep_running) {
  while (fbsplash_commandQ) {
   char *command = NULL;

   emutex_lock (&fbsplash_commandQ_mutex);
   if (fbsplash_commandQ) {
    if ((command = fbsplash_commandQ[0])) {
     void *it = command;
     command = estrdup (command);

     fbsplash_commandQ = (char **)setdel ((void **)fbsplash_commandQ, it);
    }
   }
   emutex_unlock (&fbsplash_commandQ_mutex);

   if (command) {
//    notice (1, command);
    FILE *fifo = efopen (fbsplash_fifo, "w");
    if (fifo) {
     eprintf (fifo, "%s\n", command);
     fclose (fifo);
    }

    free (command);
   }
  }

  pthread_cond_wait (&fbsplash_commandQ_cond, &fbsplash_commandQ_cond_mutex);
 }

 return NULL;
}

int _einit_feedback_visual_fbsplash_enable (void *pa, struct einit_event *status) {
 char *tmp = NULL, *fbtheme = NULL, *fbmode = "silent";
 char freetheme = 0, freemode = 0;

 _einit_feedback_visual_fbsplash_worker_thread_keep_running = 1;
 ethread_create (&fbsplash_thread, NULL, _einit_feedback_visual_fbsplash_worker_thread, NULL);

 if (einit_initial_environment) {
/* check for kernel params */
  uint32_t i = 0;
  for (; einit_initial_environment[i]; i++) {
   if (strstr (einit_initial_environment[i], "splash=")) {
    char *params = estrdup(einit_initial_environment[i] + 7);

    if (params) {
     char **p = str2set (',', params);
     if (p) {
      for (i = 0; p[i]; i++) {
       char *sep = strchr (p[i], ':');
       if (sep) {
        *sep = 0;
        sep++;
        if (strmatch (p[i], "theme")) {
         fbtheme = estrdup (sep);
         freetheme = 1;
        }
       } else {
        fbmode = estrdup(p[i]);
        freemode = 1;
       }
      }
      free (p);
     }

     free (params);
    }

    break;
   }
  }
 }

 if (fbtheme || (fbtheme = cfg_getstring ("configuration-feedback-visual-fbsplash-theme", NULL))) {
  char tmpx[BUFFERSIZE];

  esprintf (tmpx, BUFFERSIZE, "set theme %s", fbtheme);
  fbsplash_queue_comand(tmpx);

  if (freetheme) free (fbtheme);
//  notice (1, tmpx);
 }
 if ((tmp = cfg_getstring ("configuration-feedback-visual-fbsplash-daemon-ttys/silent", NULL))) {
  char tmpx[BUFFERSIZE];

  esprintf (tmpx, BUFFERSIZE, "set tty silent %s", tmp);
  fbsplash_queue_comand(tmpx);
 }
 if ((tmp = cfg_getstring ("configuration-feedback-visual-fbsplash-daemon-ttys/verbose", NULL))) {
  char tmpx[BUFFERSIZE];

  esprintf (tmpx, BUFFERSIZE, "set tty verbose %s", tmp);
  fbsplash_queue_comand(tmpx);
 }
 if ((tmp = cfg_getstring ("configuration-feedback-visual-fbsplash-daemon-fifo", NULL))) {
  fbsplash_fifo = tmp;
 }

 if (fbmode) {
  char tmpx[BUFFERSIZE];

  esprintf (tmpx, BUFFERSIZE, "set mode %s", fbmode);
  fbsplash_queue_comand(tmpx);

  if (freemode) free (fbmode);
//  notice (1, tmpx);
 }

 fbsplash_queue_comand("progress 0");
 fbsplash_queue_comand("repaint");

 event_listen (EVENT_SUBSYSTEM_EINIT, _einit_feedback_visual_fbsplash_einit_event_handler);

 return STATUS_OK;
}

int _einit_feedback_visual_fbsplash_disable (void *pa, struct einit_event *status) {
 _einit_feedback_visual_fbsplash_worker_thread_keep_running = 0;
 pthread_cond_broadcast (&fbsplash_commandQ_cond);

 event_ignore (EVENT_SUBSYSTEM_EINIT, _einit_feedback_visual_fbsplash_einit_event_handler);

 return STATUS_OK;
}

int _einit_feedback_visual_fbsplash_cleanup (struct lmodule *tm) {
 return 0;
}

int _einit_feedback_visual_fbsplash_configure (struct lmodule *tm) {
 module_init (tm);
 module_logic_configure(tm);

 tm->cleanup = _einit_feedback_visual_fbsplash_cleanup;
 tm->enable = _einit_feedback_visual_fbsplash_enable;
 tm->disable = _einit_feedback_visual_fbsplash_disable;

 return 0;
}
