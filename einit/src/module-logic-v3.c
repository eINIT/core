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
#include <einit-modules/ipc.h>

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

void mod_get_and_apply () {
 int pthread_errno;

 if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 fprintf (stderr, "enable: %s\n", set2str(' ', current.enable));
 fprintf (stderr, "disable: %s\n", set2str(' ', current.disable));
 fprintf (stderr, "reset: %s\n", set2str(' ', current.reset));
 fprintf (stderr, "reload: %s\n", set2str(' ', current.reload));
 fprintf (stderr, "zap: %s\n", set2str(' ', current.zap));
 fprintf (stderr, "critical: %s\n", set2str(' ', current.critical));

 if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_unlock() failed.");
 }
}

void cross_taskblock (struct module_taskblock *source, struct module_taskblock *target) {
 if (source->enable) {
  char **tmp = (char **)setcombine ((void **)target->enable, (void **)source->enable, SET_TYPE_STRING);
  if (target->enable) free (target->enable);
  target->enable = tmp;

  tmp = (char **)setslice ((void **)target->disable, (void **)source->enable, SET_TYPE_STRING);
  if (target->disable) free (target->disable);
  target->disable = tmp;
 }
 if (source->reset) {
  char **tmp = (char **)setcombine ((void **)target->reset, (void **)source->reset, SET_TYPE_STRING);
  if (target->reset) free (target->reset);
  target->reset = tmp;
 }
 if (source->reload) {
  char **tmp = (char **)setcombine ((void **)target->reload, (void **)source->reload, SET_TYPE_STRING);
  if (target->reload) free (target->reload);
  target->reload = tmp;
 }
 if (source->critical) {
  char **tmp = (char **)setcombine ((void **)target->critical, (void **)source->critical, SET_TYPE_STRING);
  if (target->critical) free (target->critical);
  target->critical = tmp;
 }

 if (source->disable) {
  char **tmp = (char **)setcombine ((void **)target->disable, (void **)source->disable, SET_TYPE_STRING);
  if (target->disable) free (target->disable);
  target->disable = tmp;

  tmp = (char **)setslice ((void **)target->enable, (void **)source->disable, SET_TYPE_STRING);
  if (target->enable) free (target->enable);
  target->enable = tmp;

  tmp = (char **)setslice ((void **)target->critical, (void **)source->disable, SET_TYPE_STRING);
  if (target->critical) free (target->critical);
  target->critical = tmp;
 }

 if (source->zap) {
  char **tmp = (char **)setcombine ((void **)target->zap, (void **)source->zap, SET_TYPE_STRING);
  if (target->zap) free (target->zap);
  target->zap = tmp;

  tmp = (char **)setslice ((void **)target->enable, (void **)source->zap, SET_TYPE_STRING);
  if (target->enable) free (target->enable);
  target->enable = tmp;

  tmp = (char **)setslice ((void **)target->critical, (void **)source->zap, SET_TYPE_STRING);
  if (target->critical) free (target->critical);
  target->critical = tmp;
 }
}

struct mloadplan *mod_plan (struct mloadplan *plan, char **atoms, unsigned int task, struct cfgnode *mode) {
 int pthread_errno;

 char disable_all_but_feedback = 0, disable_all = 0;

 if (!plan) {
  plan = emalloc (sizeof (struct mloadplan));
  memset (plan, 0, sizeof (struct mloadplan));
 }

 if (mode) {
  char **base = NULL;
  uint32_t xi = 0;
  char **enable   = str2set (':', cfg_getstring ("enable/services", mode));
  char **disable  = str2set (':', cfg_getstring ("disable/services", mode));
  char **reset    = str2set (':', cfg_getstring ("reset/services", mode));
  char **reload   = str2set (':', cfg_getstring ("reload/services", mode));
  char **zap      = str2set (':', cfg_getstring ("zap/services", mode));
  char **critical = str2set (':', cfg_getstring ("enable/critical", mode));

  if (!enable)
   enable  = str2set (':', cfg_getstring ("enable/mod", mode));
  if (!disable)
   disable = str2set (':', cfg_getstring ("disable/mod", mode));
  if (!reset)
   reset   = str2set (':', cfg_getstring ("reset/mod", mode));

  if (mode->arbattrs) for (; mode->arbattrs[xi]; xi+=2) {
   if (!strcmp(mode->arbattrs[xi], "base")) {
    base = str2set (':', mode->arbattrs[xi+1]);
   }
  }

  if (base) {
   int y = 0;
   struct cfgnode *cno;
   while (base[y]) {
    if (!inset ((void **)plan->used_modes, (void *)base[y], SET_TYPE_STRING)) {
     cno = cfg_findnode (base[y], EI_NODETYPE_MODE, NULL);
     if (cno) {
      plan = mod_plan (plan, NULL, 0, cno);
     }
    }

    y++;
   }

   free (base);
  }

  if (enable) {
   char **tmp = (char **)setcombine ((void **)plan->changes.enable, (void **)enable, SET_TYPE_STRING);
   if (plan->changes.enable) free (plan->changes.enable);
   plan->changes.enable = tmp;
  }
  if (disable) {
   char **tmp = (char **)setcombine ((void **)plan->changes.disable, (void **)disable, SET_TYPE_STRING);
   if (plan->changes.disable) free (plan->changes.disable);
   plan->changes.disable = tmp;
  }
  if (reset) {
   char **tmp = (char **)setcombine ((void **)plan->changes.reset, (void **)reset, SET_TYPE_STRING);
   if (plan->changes.reset) free (plan->changes.reset);
   plan->changes.reset = tmp;
  }
  if (reload) {
   char **tmp = (char **)setcombine ((void **)plan->changes.reload, (void **)reload, SET_TYPE_STRING);
   if (plan->changes.reload) free (plan->changes.reload);
   plan->changes.reload = tmp;
  }
  if (zap) {
   char **tmp = (char **)setcombine ((void **)plan->changes.zap, (void **)zap, SET_TYPE_STRING);
   if (plan->changes.zap) free (plan->changes.zap);
   plan->changes.zap = tmp;
  }
  if (critical) {
   char **tmp = (char **)setcombine ((void **)plan->changes.critical, (void **)critical, SET_TYPE_STRING);
   if (plan->changes.critical) free (plan->changes.critical);
   plan->changes.critical = tmp;
  }

  if (mode->id) {
   plan->used_modes = (char **)setadd ((void **)plan->used_modes, mode->id, SET_TYPE_STRING);
  }

  plan->mode = mode;
 } else {
  if (task & MOD_ENABLE) {
   char **tmp = (char **)setcombine ((void **)plan->changes.enable, (void **)atoms, SET_TYPE_STRING);
   if (plan->changes.enable) free (plan->changes.enable);
   plan->changes.enable = tmp;
  }
  if (task & MOD_DISABLE) {
   char **tmp = (char **)setcombine ((void **)plan->changes.disable, (void **)atoms, SET_TYPE_STRING);
   if (plan->changes.disable) free (plan->changes.disable);
   plan->changes.disable = tmp;
  }
  if (task & MOD_RESET) {
   char **tmp = (char **)setcombine ((void **)plan->changes.reset, (void **)atoms, SET_TYPE_STRING);
   if (plan->changes.reset) free (plan->changes.reset);
   plan->changes.reset = tmp;
  }
  if (task & MOD_RELOAD) {
   char **tmp = (char **)setcombine ((void **)plan->changes.reload, (void **)atoms, SET_TYPE_STRING);
   if (plan->changes.reload) free (plan->changes.reload);
   plan->changes.reload = tmp;
  }
  if (task & MOD_ZAP) {
   char **tmp = (char **)setcombine ((void **)plan->changes.zap, (void **)atoms, SET_TYPE_STRING);
   if (plan->changes.zap) free (plan->changes.zap);
   plan->changes.zap = tmp;
  }
 }


 disable_all = inset ((void **)plan->changes.disable, (void *)"all", SET_TYPE_STRING);
 disable_all_but_feedback = inset ((void **)plan->changes.disable, (void *)"all-but-feedback", SET_TYPE_STRING);

 if (disable_all || disable_all_but_feedback) {
  if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
   bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_lock() failed.");
  }

  struct stree *cur = module_logics_service_list;
  while (cur) {
   if (cur->value) {
    struct lmodule **lm = (struct lmodule **)cur->value;
    ssize_t i = 0;
    char tbe = 0;

    if ((pthread_errno = pthread_mutex_lock (&ml_tb_target_state_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    tbe = inset ((void **)target_state.enable, (void *)cur->key, SET_TYPE_STRING);

    if ((pthread_errno = pthread_mutex_unlock (&ml_tb_target_state_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_unlock() failed.");
    }

    for (; lm[i]; i++) {
     if ((lm[i]->status & STATUS_ENABLED) || tbe) {
      if (!disable_all_but_feedback || (!(lm[i]->module->mode & EINIT_MOD_FEEDBACK))) {
       plan->changes.disable = (char **)setadd ((void **)plan->changes.disable, (void *)cur->key, SET_TYPE_STRING);

       break;
      }
     }
    }
   }

   cur = streenext (cur);
  }

  if (disable_all)
   plan->changes.disable = strsetdel (plan->changes.disable, "all");
  if (disable_all_but_feedback)
   plan->changes.disable = strsetdel (plan->changes.disable, "all-but-feedback");

  if ((pthread_errno = pthread_mutex_unlock (&ml_service_list_mutex))) {
   bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_unlock() failed.");
  }
 }

 return plan;
}

unsigned int mod_plan_commit (struct mloadplan *plan) {
 int pthread_errno;
 struct einit_event *fb = evinit (EVE_FEEDBACK_PLAN_STATUS);

 if (!plan) return 0;

// do some extra work if the plan was derived from a mode
 if (plan->mode) {
  char *cmdt;
  cmode = plan->mode;

  if ((cmdt = cfg_getstring ("before-switch/emit-event", cmode))) {
   struct einit_event ee = evstaticinit (event_string_to_code(cmdt));
   event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
   evstaticdestroy (ee);
  }

  if ((cmdt = cfg_getstring ("before-switch/ipc", cmode))) {
   char **cmdts = str2set (';', cmdt);
   uint32_t in = 0;

   if (cmdts) {
    for (; cmdts[in]; in++)
     ipc_process(cmdts[in], stderr);

    free (cmdts);
   }
  }
 }

 fb->task = MOD_SCHEDULER_PLAN_COMMIT_START;
 fb->para = (void *)plan;
 status_update (fb);

 if ((pthread_errno = pthread_mutex_lock (&ml_tb_target_state_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 cross_taskblock (&(plan->changes), &target_state);

 if ((pthread_errno = pthread_mutex_unlock (&ml_tb_target_state_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 cross_taskblock (&target_state, &current);

 if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 mod_get_and_apply ();

 fb->task = MOD_SCHEDULER_PLAN_COMMIT_FINISH;
 status_update (fb);

// do some more extra work if the plan was derived from a mode
 if (plan->mode) {
  char *cmdt;
  amode = plan->mode;

  if (amode->id) {
   struct einit_event eema = evstaticinit (EVE_PLAN_UPDATE);
   eema.string = estrdup(amode->id);
   eema.para   = (void *)amode;
   event_emit (&eema, EINIT_EVENT_FLAG_BROADCAST);
   free (eema.string);
   evstaticdestroy (eema);
  }

  if ((cmdt = cfg_getstring ("after-switch/emit-event", amode))) {
   struct einit_event ee = evstaticinit (event_string_to_code(cmdt));
   event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
   evstaticdestroy (ee);
  }

  if ((cmdt = cfg_getstring ("after-switch/ipc", amode))) {
   char **cmdts = str2set (';', cmdt);
   uint32_t in = 0;

   if (cmdts) {
    for (; cmdts[in]; in++) {
     ipc_process(cmdts[in], stderr);
    }
    free (cmdts);
   }
  }
 }

 evdestroy (fb);

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
