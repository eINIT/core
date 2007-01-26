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
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <einit-modules/exec.h>

#include <sys/types.h>
#include <sys/stat.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char *provides[] = {"feedback-visual", "feedback-graphical", NULL};
const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
 .name      = "visual/fbsplash-based feedback module",
 .rid       = "einit-feedback-visual-fbsplash",
 .si        = {
  .provides = provides,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

struct planref {
 struct mloadplan *plan;
 time_t startedat;
 uint32_t max_changes;
 uint32_t min_changes;
};

struct lmodule *self_l = NULL;

void einit_event_handler(struct einit_event *);
void feedback_event_handler(struct einit_event *);

char *splash_functions = "/sbin/splash-functions.sh";
char *scriptlet_action = NULL;

struct planref **plans = NULL;
pthread_mutex_t plansmutex = PTHREAD_MUTEX_INITIALIZER;

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getstring("configuration-feedback-visual-fbsplash-splash-functions", NULL)) {
   fputs (" * configuration variable \"configuration-feedback-visual-fbsplash-splash-functions\" not found : fbsplash support disabled.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-visual-fbsplash-scriptlets", NULL)) {
   fputs (" * configuration variable \"configuration-feedback-visual-fbsplash-scriptlets\" not found : fbsplash support disabled.\n", (FILE *)ev->para);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int configure (struct lmodule *this) {
 exec_configure (this);
 char *s = NULL;

 struct cfgnode *node;

 if (s = cfg_getstring("configuration-feedback-visual-fbsplash-splash-functions", NULL))
  splash_functions = s;

 self_l = this;
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *this) {
 exec_cleanup(this);
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int enable (void *pa, struct einit_event *status) {
 char *s = NULL;

 struct stat st;
 if (stat (splash_functions, &st)) return STATUS_FAIL;

 if (s = cfg_getstring("configuration-feedback-visual-fbsplash-scriptlets/action", NULL)) {
  scriptlet_action = s;
 } else
  return STATUS_FAIL;

// remember: pexec(command, variables, uid, gid, user, group, local_environment, status)

  if (s = cfg_getstring("configuration-feedback-visual-fbsplash-scriptlets/init", NULL)) {
   char *vars[] = { "splash-functions-file", splash_functions, "new-mode", "default", NULL },
        *subs = apply_variables (s, vars);
   int ret = STATUS_FAIL;

   if (subs) {
    ret = pexec(subs, NULL, 0, 0, NULL, NULL, NULL, status);

    free (subs);
   }

   if (ret & STATUS_OK) {
    event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
    event_listen (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
   }

   return ret;
  }

  return STATUS_FAIL;
}

int disable (void *pa, struct einit_event *status) {
 char *s = NULL;

 pthread_mutex_lock (&self_l->imutex);
 event_ignore (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);

 if (s = cfg_getstring("configuration-feedback-visual-fbsplash-scriptlets/quit", NULL)) {
  char *vars[] = { "splash-functions-file", splash_functions, "new-mode", "default", NULL },
  *subs = apply_variables (s, vars);
  int ret = STATUS_FAIL;

  if (subs) {
   ret = pexec(subs, NULL, 0, 0, NULL, NULL, NULL, status);

   free (subs);
  }

  if (ret & STATUS_OK) {
   event_ignore (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
   event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
  }

  pthread_mutex_unlock (&self_l->imutex);

  return ret;
 }

 pthread_mutex_unlock (&self_l->imutex);
 return STATUS_FAIL;
}

void feedback_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&self_l->imutex);

 if (ev->type == EVE_FEEDBACK_PLAN_STATUS) {
  int i = 0;
  struct planref plan, *cul = NULL;
  uint32_t startedat = 0;
  switch (ev->task) {
   case MOD_SCHEDULER_PLAN_COMMIT_START:
    pthread_mutex_lock (&plansmutex);
    plan.plan = (struct mloadplan *)ev->para;
    plan.startedat = time (NULL);
    plan.max_changes = 0;
    plan.min_changes = 0;
    plans = (struct planref **)setadd ((void **)plans, (void *)&plan, sizeof (struct planref));
    pthread_mutex_unlock (&plansmutex);
    break;
   case MOD_SCHEDULER_PLAN_COMMIT_FINISH:
    if (!plans) break;
    pthread_mutex_lock (&plansmutex);
    for (; plans[i]; i++)
     if (plans[i]->plan == (struct mloadplan *)ev->para) {
     cul = plans[i];
     startedat = plans[i]->startedat;
     break;
     }
     if (cul)
      plans = (struct planref **)setdel ((void **)plans, (void *)cul);
     pthread_mutex_unlock (&plansmutex);
  }
 }

 pthread_mutex_unlock (&self_l->imutex);
 return;
}

void einit_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&self_l->imutex);

 if (ev->type == EVE_SERVICE_UPDATE) {
  uint32_t u = 0;
  if (ev->set) for (u = 0; ev->set[u]; u++) {
   if (plans) {
    uint32_t i = 0;
    char  *nmode = "unknown",
         **all_services = NULL,
         **active_services = NULL,
         *splash_command[] = { "splash",
                               (ev->task & MOD_ENABLE) ?
                                 ((ev->status == STATUS_WORKING) ? "svc_start" : ((ev->status & STATUS_OK) ? "svc_started" : "svc_stopped")) :
                                 ((ev->status == STATUS_WORKING) ? "svc_stop" : ((ev->status & STATUS_OK) ? "svc_stopped" : "svc_started")),
                               ev->set[u], NULL };

    pthread_mutex_lock (&plansmutex);

    for (; plans[i]; i++) {
     if (plans[i]->plan) {
      if (plans[i]->plan) {
       if (plans[i]->plan->mode && plans[i]->plan->mode->id) nmode = plans[i]->plan->mode->id;

       if (plans[i]->plan->services) {
        struct stree *ha = plans[i]->plan->services;
        while (ha) {
         all_services = (char **)setadd ((void **)all_services, (void *)ha->key, SET_TYPE_STRING);

         if (((struct mloadplan_node *)(ha->value))->changed == 2) {
          active_services = (char **)setadd ((void **)active_services, (void *)ha->key, SET_TYPE_STRING);
         }

         ha = streenext(ha);
        }
       }
      }
     }
    }

    if (all_services && splash_command) {
     char *as = set2str (' ', all_services),
          *acs = set2str (' ', active_services),
          *cm = set2str (' ', splash_command),
          *var[] = { "splash-functions-file", splash_functions,
                     "new-mode", nmode,
                     "all-services", as ? as : "none",
                     "active-services", acs ? acs : "none",
                     "action", cm ? cm : "/bin/false",
                     NULL },
          *s = apply_variables(scriptlet_action, var);


     if (s) pexec(s, NULL, 0, 0, NULL, NULL, NULL, NULL);
     if (as) free (as);
     if (acs) free (acs);
     if (cm) free (cm);
    }

    if (all_services) free (all_services);
    if (active_services) free (active_services);

    pthread_mutex_unlock (&plansmutex);
   }
  }
 }

 pthread_mutex_unlock (&self_l->imutex);
 return;
}
