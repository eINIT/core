/*
 *  module-logic-v3.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/02/2007.
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
#include <unistd.h>
#include <stdio.h>
#include <einit/config.h>
#include <einit/module-logic.h>
#include <einit/tree.h>
#include <pthread.h>
#include <string.h>
#include <einit/bitch.h>

struct module_taskblock
  current = { NULL, NULL, NULL, NULL, NULL, NULL },
  target_state = { NULL, NULL, NULL, NULL, NULL, NULL };

struct stree *module_logics_service_list = NULL; // value is a (struct lmodule **)
struct stree *module_logics_mode_data = NULL;

struct lmodule *mlist;

pthread_mutex_t
  ml_tb_current_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_tb_target_state_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_service_list_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_mode_data_mutex = PTHREAD_MUTEX_INITIALIZER;

struct mloadplan *mod_plan (struct mloadplan *plan, char **atoms, unsigned int task, struct cfgnode *mode) {
 int pthread_errno;

 if (!plan) {
  plan = emalloc (sizeof (struct mloadplan));
  memset (plan, 0, sizeof (struct mloadplan));
 }

 if (mode) {
  plan->mode = mode;

 } else {
  if (ev->task & MOD_ENABLE) {
   char **tmp = (char **)setcombine (plan->changes->enable, atoms, SET_TYPE_STRING);
   if (plan->changes->enable) free (plan->changes->enable);
   plan->changes->enable = tmp;
  }
  if (ev->task & MOD_DISABLE) {
   char **tmp = (char **)setcombine (plan->changes->disable, atoms, SET_TYPE_STRING);
   if (plan->changes->disable) free (plan->changes->disable);
   plan->changes->disable = tmp;
  }
  if (ev->task & MOD_RESET) {
   char **tmp = (char **)setcombine (plan->changes->reset, atoms, SET_TYPE_STRING);
   if (plan->changes->reset) free (plan->changes->reset);
   plan->changes->reset = tmp;
  }
  if (ev->task & MOD_RELOAD) {
   char **tmp = (char **)setcombine (plan->changes->reload, atoms, SET_TYPE_STRING);
   if (plan->changes->reload) free (plan->changes->reload);
   plan->changes->reload = tmp;
  }
  if (ev->task & MOD_ZAP) {
   char **tmp = (char **)setcombine (plan->changes->zap, atoms, SET_TYPE_STRING);
   if (plan->changes->zap) free (plan->changes->zap);
   plan->changes->zap = tmp;
  }
 }

 if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_service_list_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_unlock() failed.");
 }
 return plan;
}

unsigned int mod_plan_commit (struct mloadplan *plan) {
 if (!plan) return 0;

 if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 if (plan->changes->enable) {
  char **tmp = (char **)setcombine (target_state.enable, plan->changes->enable, SET_TYPE_STRING);
  if (target_state.enable) free (target_state.enable);
  target_state.enable = tmp;
 }
 if (plan->changes->disable) {
  char **tmp = (char **)setcombine (target_state.disable, plan->changes->disable, SET_TYPE_STRING);
  if (target_state.disable) free (target_state.disable);
  target_state.disable = tmp;
 }
 if (plan->changes->reset) {
  char **tmp = (char **)setcombine (target_state.reset, plan->changes->reset, SET_TYPE_STRING);
  if (target_state.reset) free (target_state.reset);
  target_state.reset = tmp;
 }
 if (plan->changes->reload) {
  char **tmp = (char **)setcombine (target_state.reload, plan->changes->reload, SET_TYPE_STRING);
  if (target_state.reload) free (target_state.reload);
  target_state.reload = tmp;
 }
 if (plan->changes->zap) {
  char **tmp = (char **)setcombine (target_state.zap, plan->changes->zap, SET_TYPE_STRING);
  if (target_state.zap) free (target_state.zap);
  target_state.zap = tmp;
 }
 if (plan->changes->critical) {
  char **tmp = (char **)setcombine (target_state.critical, plan->changes->critical, SET_TYPE_STRING);
  if (target_state.critical) free (target_state.critical);
  target_state.critical = tmp;
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 if (plan->mode) return 0; // always return "OK" if it's based on a mode

 return 0;
}

int mod_plan_free (struct mloadplan *plan) {
 if (plan) free (plan);
 return 0;
}

double get_plan_progress (struct mloadplan *plan) {
 return 0.0;
}

void module_logic_einit_event_handler(struct einit_event *ev) {
 int pthread_errno;

 if (ev->type == EVE_MODULE_LIST_UPDATE) {
/* update list with services */
  struct stree *new_service_list = NULL;
  struct lmodule *cur = mlist;

  if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_lock() failed.");
  }

  while (cur) {
   if (cur->si && cur->si->provides) {
    ssize_t i = 0;

    for (; cur->si->provides[i]; i++) {
     struct stree *slnode = streefind (new_service_list, cur->si->provides[i], TREE_FIND_FIRST);
     struct lnode **curval = (struct lnode **) (slnode ? slnode->value : NULL);

     curval = (struct lnode **)setadd ((void **)curval, cur, SET_NOALLOC);

     if (slnode) {
      slnode->value = curval;
      slnode->luggage = curval;
     } else {
      new_service_list = streeadd (new_service_list, cur->si->provides[i], (void *)curval, SET_NOALLOC, (void *)curval);
     }
    }
   }
   cur = cur->next;
  }

  if (module_logics_service_list) streefree (module_logics_service_list);
  module_logics_service_list = new_service_list;

  if ((pthread_errno = pthread_mutex_unlock (&ml_service_list_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_unlock() failed.");
  }
 } else if ((ev->type == EVE_SERVICE_UPDATE) && (!(ev->status & STATUS_WORKING))) {
/* something's done now, update our lists */
  if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_lock() failed.");
  }

  if (ev->task & MOD_ENABLE) {
   if (ev->set) {
    uint32_t x = 0;
    for (; ev->set[x]; x++) {
     current.enable = strsetdel (current.enable, (char *)ev->set[x]);
    }
   }
  }
  if (ev->task & MOD_DISABLE) {
   if (ev->set) {
    uint32_t x = 0;
    for (; ev->set[x]; x++) {
     current.disable = strsetdel (current.disable, (char *)ev->set[x]);
    }
   }
  }
  if (ev->task & MOD_RESET) {
   if (ev->set) {
    uint32_t x = 0;
    for (; ev->set[x]; x++) {
     current.reset = strsetdel (current.reset, (char *)ev->set[x]);
    }
   }
  }
  if (ev->task & MOD_RELOAD) {
   if (ev->set) {
    uint32_t x = 0;
    for (; ev->set[x]; x++) {
     current.reload = strsetdel (current.reload, (char *)ev->set[x]);
    }
   }
  }
  if (ev->task & MOD_ZAP) {
   if (ev->set) {
    uint32_t x = 0;
    for (; ev->set[x]; x++) {
     current.zap = strsetdel (current.zap, (char *)ev->set[x]);
    }
   }
  }

  if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_unlock() failed.");
  }
 }
}
