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
struct stree *module_logics_group_data = NULL;

struct lmodule *mlist;

char **unresolved_services = NULL;
char **broken_services = NULL;

pthread_mutex_t
  ml_tb_current_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_tb_target_state_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_service_list_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_group_data_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_unresolved_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ma_task {
 struct stree *st;
 int task;
};

struct group_data {
 char **members;
 uint32_t options;
};

#define MOD_PLAN_GROUP_SEQ_ANY     0x00000001
#define MOD_PLAN_GROUP_SEQ_ALL     0x00000002
#define MOD_PLAN_GROUP_SEQ_ANY_IOP 0x00000004
#define MOD_PLAN_GROUP_SEQ_MOST    0x00000008

void mod_update_group (struct lmodule *lm);
char mod_isbroken (char *service);

char mod_done (struct lmodule *module, int task) {
 int pthread_errno;

 if (!module) return 0;

 mod_update_group (module);

 if ((task & MOD_ENABLE) && (module->status & STATUS_ENABLED)) {
  if (module->si && module->si->provides) {
   uint32_t x = 0;
   for (; module->si->provides[x]; x++) {
    if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    current.enable = strsetdel (current.enable, module->si->provides[x]);

    if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }
  }

  return 1;
 }

 if ((task & MOD_DISABLE) && (module->status & STATUS_DISABLED)) {
  if (module->si && module->si->provides) {
   uint32_t x = 0;
   for (; module->si->provides[x]; x++) {
    if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    current.disable = strsetdel (current.disable, module->si->provides[x]);

    if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }
  }

  return 1;
 }

 if ((task & MOD_RESET) && (module->status & STATUS_OK)) {
  if (module->si && module->si->provides) {
   uint32_t x = 0;
   for (; module->si->provides[x]; x++) {
    if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    current.reset = strsetdel (current.reset, module->si->provides[x]);

    if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }
  }

  return 1;
 }

 if ((task & MOD_RELOAD) && (module->status & STATUS_OK)) {
  if (module->si && module->si->provides) {
   uint32_t x = 0;
   for (; module->si->provides[x]; x++) {
    if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    current.reload = strsetdel (current.reload, module->si->provides[x]);

    if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }
  }

  return 1;
 }

 if ((task & MOD_ZAP) && (module->status & STATUS_OK)) {
  if (module->si && module->si->provides) {
   uint32_t x = 0;
   for (; module->si->provides[x]; x++) {
    if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    current.zap = strsetdel (current.zap, module->si->provides[x]);

    if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_done()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }
  }

  return 1;
 }

 return 0;
}

struct group_data *mod_group_get_data (char *group) {
 struct group_data *ret = NULL;
 int pthread_errno = 0;

 if ((pthread_errno = pthread_mutex_lock (&ml_group_data_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_group_get_data()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 struct stree *cur = streefind (module_logics_group_data, group, TREE_FIND_FIRST);
 if (cur) { ret = (struct group_data *)cur->value; }
 else {
  char *tnodeid = emalloc (strlen (group)+17);
  struct cfgnode *gnode = NULL;

  memcpy (tnodeid, "services-alias-", 16);
  strcat (tnodeid, group);

  ret = ecalloc (1, sizeof (struct group_data));

  if ((gnode = cfg_getnode (tnodeid, NULL)) && gnode->arbattrs) {
   ssize_t r = 0;

   for (r = 0; gnode->arbattrs[r]; r+=2) {
    if (!strcmp (gnode->arbattrs[r], "group")) {
     ret->members = str2set (':', gnode->arbattrs[r+1]);
    } else if (!strcmp (gnode->arbattrs[r], "seq")) {
     if (!strcmp (gnode->arbattrs[r+1], "any"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ANY;
     else if (!strcmp (gnode->arbattrs[r+1], "all"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ALL;
     else if (!strcmp (gnode->arbattrs[r+1], "any-iop"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ANY_IOP;
     else if (!strcmp (gnode->arbattrs[r+1], "most"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_MOST;
    }
   }
  }
  free (tnodeid);

  if (!ret->members || !ret->options) {
   free (ret);
   ret = NULL;
  } else {
   module_logics_group_data = streeadd (module_logics_group_data, group, (void *)ret, SET_NOALLOC, (void *)ret);
  }
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_group_data_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_group_get_data()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 return ret;
}

void mod_update_group (struct lmodule *lmx) {
 int pthread_errno;
 char retval = 0;

// if (!lm || !lm->si || !lm->si->provides) return;

 if ((pthread_errno = pthread_mutex_lock (&ml_group_data_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_update_group()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 struct stree *cur = module_logics_group_data;

 while (cur) {
  struct group_data *gd = (struct group_data *)cur->value;

  if (gd && gd->members) {
   ssize_t x = 0, mem = setcount ((void **)gd->members), failed = 0, on = 0;
   struct lmodule **providers = NULL;

   for (; gd->members[x]; x++) {
    if (mod_isbroken (gd->members[x])) {
     failed++;
    } else {
	 struct stree *serv = NULL;

     if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
      bitch2(BITCH_EPTHREADS, "mod_update_group()", pthread_errno, "pthread_mutex_lock() failed.");
     }

     if ((serv = streefind(module_logics_service_list, gd->members[x], TREE_FIND_FIRST))) {
	  struct lmodule **lm = (struct lmodule **)serv->value;

      if (lm) {
       ssize_t y = 0;

       for (; lm[y]; y++) {
        if (lm[y]->status & STATUS_ENABLED) {
         providers = (struct lmodule **)setadd ((void **)providers, (void *)lm[y], SET_NOALLOC);
         on++;

		 break;
        }
       }
      }
     }
    }

    if ((pthread_errno = pthread_mutex_unlock (&ml_service_list_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_update_group()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }

   if (gd->options & (MOD_PLAN_GROUP_SEQ_ANY | MOD_PLAN_GROUP_SEQ_ANY_IOP)) {
    if (on > 0) {
     service_usage_query_group (SERVICE_SET_GROUP_PROVIDERS, (struct lmodule *)providers, cur->key);
    }
   } else if (gd->options & MOD_PLAN_GROUP_SEQ_MOST) {
    if ((on + failed) == mem) {
     service_usage_query_group (SERVICE_SET_GROUP_PROVIDERS, (struct lmodule *)providers, cur->key);
    }
   } else if (gd->options & MOD_PLAN_GROUP_SEQ_ALL) {
    if (on == mem) {
     service_usage_query_group (SERVICE_SET_GROUP_PROVIDERS, (struct lmodule *)providers, cur->key);
    }
   }

   if (failed == mem) {
    if ((pthread_errno = pthread_mutex_lock (&ml_unresolved_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    fprintf (stderr, " >> broken service (all group members failed): %s\n", cur->key);

    broken_services = (char **)setadd ((void **)broken_services, (void *)cur->key, SET_TYPE_STRING);

    if ((pthread_errno = pthread_mutex_unlock (&ml_unresolved_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }

   if (providers) free (providers);
  }

  cur = streenext (cur);
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_group_data_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_update_group()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 return retval;
}

char mod_disable_users (struct lmodule *module) {
 int pthread_errno = 0;

 if (!service_usage_query(SERVICE_NOT_IN_USE, module, NULL)) {
  ssize_t i = 0;
  char **need = NULL;
  char **t = service_usage_query_cr (SERVICE_GET_SERVICES_THAT_USE, module, NULL);

  if (t) {
   for (; t[i]; i++) {
    if (mod_isbroken (t[i])) {
	 if (need) free (need);
	 return 0;
    } else {
     need = (char **)setadd ((void **)need, t[i], SET_TYPE_STRING);
	}
   }

   if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_disable_users()", pthread_errno, "pthread_mutex_lock() failed.");
   }

   char **tmp = (char **)setcombine ((void **)current.disable, (void **)need, SET_TYPE_STRING);
   if (current.disable) free (current.disable);
   current.disable = tmp;

   if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_disable_users()", pthread_errno, "pthread_mutex_unlock() failed.");
   }
  }

  return 1;
 }

 return 0;
}

char mod_enable_requirements (struct lmodule *module) {
 int pthread_errno = 0;

 if (!service_usage_query(SERVICE_REQUIREMENTS_MET, module, NULL)) {
  if (module->si && module->si->requires) {
   ssize_t i = 0;
   char **need = NULL;

   for (; module->si->requires[i]; i++) {
    if (mod_isbroken (module->si->requires[i])) {
	 if (need) free (need);
	 return 0;
    } else if (!service_usage_query (SERVICE_IS_PROVIDED, NULL, module->si->requires[i])) {
     need = (char **)setadd ((void **)need, module->si->requires[i], SET_TYPE_STRING);
	}
   }

   if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_enable_requirements()", pthread_errno, "pthread_mutex_lock() failed.");
   }

   char **tmp = (char **)setcombine ((void **)current.enable, (void **)need, SET_TYPE_STRING);
   if (current.enable) free (current.enable);
   current.enable = tmp;

   if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_enable_requirements()", pthread_errno, "pthread_mutex_unlock() failed.");
   }
  }

  return 1;
 }

 return 0;
}

char mod_isbroken (char *service) {
 int retval = 0, pthread_errno;

 if ((pthread_errno = pthread_mutex_lock (&ml_unresolved_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_isbroken()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 retval = inset ((void **)broken_services, (void *)service, SET_TYPE_STRING);

 if ((pthread_errno = pthread_mutex_unlock (&ml_unresolved_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_isbroken()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 return retval;
}

void mod_apply (struct ma_task *task) {
 if (task && task->st) {
  struct stree *des = task->st;
  struct lmodule **lm = (struct lmodule **)des->value;

  if (lm) {
   int pthread_errno;
   struct lmodule *first = lm[0];

   do {
    struct lmodule *current = lm[0];

    if ((task->task & MOD_DISABLE) && ((lm[0]->status & STATUS_DISABLED) || (lm[0]->status == STATUS_IDLE)))
	 goto skip_module;

    if (task->task & MOD_ENABLE) if (mod_enable_requirements (current)) {
     free (task);
     return;
    }
    if (task->task & MOD_DISABLE) if (mod_disable_users (current)) {
     free (task);
     return;
    }

    int retval = mod (task->task, current);

    if (task->task & MOD_ENABLE) {
     if (retval & STATUS_OK) {
      free (task);
      return;
     }
    }

    skip_module:
/* next module */
    if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    lm = (struct lmodule **)setdel ((void **)lm, current);
    lm = (struct lmodule **)setadd ((void **)lm, current, SET_NOALLOC);
	des->value = (struct lmodule **)lm;
	des->luggage = (struct lmodule **)lm;

    if ((pthread_errno = pthread_mutex_unlock (&ml_service_list_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   } while (lm[0] != first);
/* if we tried to enable something and end up here, it means we did a complete
   round-trip and nothing worked */

   if ((pthread_errno = pthread_mutex_lock (&ml_unresolved_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_lock() failed.");
   }

   switch (task->task) {
    case MOD_ENABLE: current.enable = strsetdel(current.enable, des->key); break;
    case MOD_DISABLE: current.disable = strsetdel(current.disable, des->key); break;
    case MOD_RESET: current.reset = strsetdel(current.reset, des->key); break;
    case MOD_RELOAD: current.reload = strsetdel(current.reload, des->key); break;
    case MOD_ZAP: current.zap = strsetdel(current.zap, des->key); break;
   }

   if ((pthread_errno = pthread_mutex_unlock (&ml_unresolved_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_unlock() failed.");
   }

   if (task->task & (MOD_ENABLE | MOD_RESET | MOD_RELOAD)) {
    if ((pthread_errno = pthread_mutex_lock (&ml_unresolved_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_lock() failed.");
    }

    fprintf (stderr, " >> broken service: %s\n", des->key);

    broken_services = (char **)setadd ((void **)broken_services, (void *)des->key, SET_TYPE_STRING);

    if ((pthread_errno = pthread_mutex_unlock (&ml_unresolved_mutex))) {
     bitch2(BITCH_EPTHREADS, "mod_apply()", pthread_errno, "pthread_mutex_unlock() failed.");
    }
   }
  }

  free (task);
  return;
 }
}

void mod_get_and_apply_recurse (int task) {
 int pthread_errno;
 ssize_t x = 0, nservices;
 char **services = NULL;
 char **do_later = NULL;
 pthread_t **subthreads = NULL;
 pthread_t th;

 if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 switch (task) {
  case MOD_ENABLE: services = (char **)setdup((void **)current.enable, SET_TYPE_STRING); break;
  case MOD_DISABLE: services = (char **)setdup((void **)current.disable, SET_TYPE_STRING); break;
  case MOD_RESET: services = (char **)setdup((void **)current.reset, SET_TYPE_STRING); break;
  case MOD_RELOAD: services = (char **)setdup((void **)current.reload, SET_TYPE_STRING); break;
  case MOD_ZAP: services = (char **)setdup((void **)current.zap, SET_TYPE_STRING); break;
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 if (!services) return;
 nservices = setcount ((void **)services);

 fprintf (stderr, " >> %i (%s)\n", task, set2str (' ', services));

 for (; services[x]; x++) {
  if (mod_isbroken (services[x])) {
   if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_lock() failed.");
   }

   switch (task) {
    case MOD_ENABLE: current.enable = strsetdel(current.enable, services[x]); break;
    case MOD_DISABLE: current.disable = strsetdel(current.disable, services[x]); break;
    case MOD_RESET: current.reset = strsetdel(current.reset, services[x]); break;
    case MOD_RELOAD: current.reload = strsetdel(current.reload, services[x]); break;
    case MOD_ZAP: current.zap = strsetdel(current.zap, services[x]); break;
   }

   if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_unlock() failed.");
   }
  } else {
   if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_lock() failed.");
   }

   struct stree *des = streefind (module_logics_service_list, services[x], TREE_FIND_FIRST);
   struct lmodule **lm = des ? (struct lmodule **)des->value : NULL;

   if ((pthread_errno = pthread_mutex_unlock (&ml_service_list_mutex))) {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_unlock() failed.");
   }

   if (!des) {
    struct group_data *gd = mod_group_get_data(services[x]);
    if (gd && gd->members) {
     fprintf (stderr, " >> %s is actually a group!\n", services[x]);

     if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
      bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_lock() failed.");
     }

     if (task & MOD_ENABLE) {
      if (gd->options & (MOD_PLAN_GROUP_SEQ_ANY | MOD_PLAN_GROUP_SEQ_ANY_IOP)) {
	   ssize_t r = 0;

       for (; gd->members[r]; r++) {
        if (!mod_isbroken (gd->members[r])) {
         current.enable = (char **)setadd ((void **)current.enable, (void *)gd->members[r], SET_TYPE_STRING);
        }
       }
      } else if (gd->options & (MOD_PLAN_GROUP_SEQ_ALL | MOD_PLAN_GROUP_SEQ_MOST)) {
       char **tmp = (char **)setcombine ((void **)current.enable, (void **)gd->members, SET_TYPE_STRING);
       tmp = strsetdel(tmp, services[x]);
       if (current.enable) free (current.enable);
       current.enable = tmp;
      }
     }
     if (task & MOD_DISABLE) {
      char **tmp = (char **)setcombine ((void **)current.disable, (void **)gd->members, SET_TYPE_STRING);
      tmp = strsetdel(tmp, services[x]);
      if (current.disable) free (current.disable);
      current.disable = tmp;
     }
     if (task & MOD_RESET) {
      char **tmp = (char **)setcombine ((void **)current.reset, (void **)gd->members, SET_TYPE_STRING);
      tmp = strsetdel(tmp, services[x]);
      if (current.reset) free (current.reset);
      current.reset = tmp;
     }
     if (task & MOD_RELOAD) {
      char **tmp = (char **)setcombine ((void **)current.reload, (void **)gd->members, SET_TYPE_STRING);
      tmp = strsetdel(tmp, services[x]);
      if (current.reload) free (current.reload);
      current.reload = tmp;
     }
     if (task & MOD_ZAP) {
      char **tmp = (char **)setcombine ((void **)current.zap, (void **)gd->members, SET_TYPE_STRING);
      tmp = strsetdel(tmp, services[x]);
      if (current.zap) free (current.zap);
      current.zap = tmp;
     }

     if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
      bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_unlock() failed.");
     }
    } else {
     if ((pthread_errno = pthread_mutex_lock (&ml_unresolved_mutex))) {
      bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_lock() failed.");
     }

     unresolved_services = (char **)setadd ((void **)unresolved_services, (void *)services[x], SET_TYPE_STRING);

     if ((pthread_errno = pthread_mutex_unlock (&ml_unresolved_mutex))) {
      bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_mutex_unlock() failed.");
     }
    }
   } else if (lm) {
    char isdone = 0;
    ssize_t i = 0;

/* always do resets/reloads/zaps */
    if ((task & MOD_ENABLE) || (task & MOD_DISABLE)) {
     for (; lm[i]; i++) {
      isdone |= mod_done(lm[i], task);
     }
    }

    if (!isdone) {
     struct ma_task *subtask_data = ecalloc (1, sizeof (struct ma_task));
     subtask_data->st = des;
     subtask_data->task = task;

     if ((nservices - x) == 1) {
      mod_apply (subtask_data);
     } else {
      if (!(pthread_errno = pthread_create (&th, NULL, (void *(*)(void *))mod_apply, (void *)subtask_data))) {
       subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
      } else {
       bitch2(BITCH_EPTHREADS, "mod_get_and_apply()", pthread_errno, "pthread_create() failed.");
       mod_apply (subtask_data);
      }
     }
    }
   }
  }
 }

 if (subthreads) {
  uint32_t u;
  uintptr_t ret;

  for (u = 0; subthreads[u]; u++) {
   if ((pthread_errno = pthread_join (*(subthreads[u]), (void *)&ret))) {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply_recurse()", pthread_errno, "pthread_join() failed.");
   }
  }
  free (subthreads); subthreads = NULL;
 }

 free (services);
}

void mod_get_and_apply () {
 int pthread_errno = 0;

 while (current.zap || current.disable || current.reset || current.reload || current.enable) {
  pthread_t **subthreads = NULL;
  pthread_t th;

  if (current.zap) {
   if (!(pthread_errno = pthread_create (&th, NULL, (void *(*)(void *))mod_get_and_apply_recurse, (void *)MOD_ZAP))) {
    subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
   } else {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply()", pthread_errno, "pthread_create() failed.");
    mod_get_and_apply_recurse (MOD_ZAP);
   }
  }

  if (current.disable) {
   if (!(pthread_errno = pthread_create (&th, NULL, (void *(*)(void *))mod_get_and_apply_recurse, (void *)MOD_DISABLE))) {
    subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
   } else {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply()", pthread_errno, "pthread_create() failed.");
    mod_get_and_apply_recurse (MOD_DISABLE);
   }
  }

  if (current.reset) {
   if (!(pthread_errno = pthread_create (&th, NULL, (void *(*)(void *))mod_get_and_apply_recurse, (void *)MOD_RESET))) {
    subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
   } else {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply()", pthread_errno, "pthread_create() failed.");
    mod_get_and_apply_recurse (MOD_RESET);
   }
  }

  if (current.reload) {
   if (!(pthread_errno = pthread_create (&th, NULL, (void *(*)(void *))mod_get_and_apply_recurse, (void *)MOD_RELOAD))) {
    subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
   } else {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply()", pthread_errno, "pthread_create() failed.");
    mod_get_and_apply_recurse (MOD_RELOAD);
   }
  }

  if (current.enable) {
   if (!(pthread_errno = pthread_create (&th, NULL, (void *(*)(void *))mod_get_and_apply_recurse, (void *)MOD_ENABLE))) {
    subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
   } else {
    bitch2(BITCH_EPTHREADS, "mod_get_and_apply()", pthread_errno, "pthread_create() failed.");
    mod_get_and_apply_recurse (MOD_ENABLE);
   }
  }

  if (subthreads) {
   uint32_t u;
   uintptr_t ret;

   for (u = 0; subthreads[u]; u++) {
    if ((pthread_errno = pthread_join (*(subthreads[u]), (void *)&ret))) {
     bitch2(BITCH_EPTHREADS, "mod_get_and_apply()", pthread_errno, "pthread_join() failed.");
    }
   }
   free (subthreads); subthreads = NULL;
  }
 }

 if ((pthread_errno = pthread_mutex_lock (&ml_unresolved_mutex))) {
  bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 if (broken_services) {
  free (broken_services);
  broken_services = NULL;
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_unresolved_mutex))) {
  bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_unlock() failed.");
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
  struct stree *cur;
  ssize_t i = 0;

  if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
   bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_lock() failed.");
  }

  if ((pthread_errno = pthread_mutex_lock (&ml_tb_target_state_mutex))) {
   bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_lock() failed.");
  }

  char **tmpx = (char **)setcombine ((void **)plan->changes.enable, (void **)target_state.enable, SET_TYPE_STRING);

  if ((pthread_errno = pthread_mutex_unlock (&ml_tb_target_state_mutex))) {
   bitch2(BITCH_EPTHREADS, "mod_plan()", pthread_errno, "pthread_mutex_unlock() failed.");
  }

  char **tmp = (char **)setcombine ((void **)tmpx, (void **)plan->changes.disable, SET_TYPE_STRING);  

  free (tmpx);
  tmpx = (char **)setdup((void **)tmp, SET_TYPE_STRING);

  if (tmpx) {
   for (; tmpx[i]; i++) {
	if ((cur = streefind (module_logics_service_list, tmpx[i], TREE_FIND_FIRST))) {
     struct lmodule **lm = (struct lmodule **)cur->value;
     if (lm) {
	  ssize_t y = 0;
	  for (; lm[y]; y++) {
       if (disable_all_but_feedback && (lm[y]->module->mode & EINIT_MOD_FEEDBACK)) {
        tmp = strsetdel (tmp, cur->key);

        break;
       }
	  }
	 }
    } else if (!service_usage_query (SERVICE_IS_PROVIDED, NULL, tmpx[i])) {
     tmp = strsetdel (tmp, tmpx[i]);
    }
   }

   free (tmpx);
  }

  if (plan->changes.disable)
   free (plan->changes.disable);

  plan->changes.disable = tmp;

/*  cur = module_logics_service_list;

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
  }*/

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

 if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 cross_taskblock (&target_state, &current);

 uint32_t i = 0;

 if (current.enable) {
  char **tmp = NULL;
  for (; current.enable[i]; i++) {
   if (!service_usage_query (SERVICE_IS_PROVIDED, NULL, current.enable[i])) {
    tmp = (char **)setadd ((void **)tmp, (void *)current.enable[i], SET_TYPE_STRING);
   }
  }
  free (current.enable);
  current.enable = tmp;
 }
 if (current.disable) {
  char **tmp = NULL;
  for (; current.disable[i]; i++) {
   if (service_usage_query (SERVICE_IS_PROVIDED, NULL, current.disable[i])) {
    tmp = (char **)setadd ((void **)tmp, (void *)current.disable[i], SET_TYPE_STRING);
   }
  }
  free (current.disable);
  current.disable = tmp;
 }
 if (current.zap) {
  char **tmp = NULL;
  for (; current.zap[i]; i++) {
   if (service_usage_query (SERVICE_IS_PROVIDED, NULL, current.zap[i])) {
    tmp = (char **)setadd ((void **)tmp, (void *)current.zap[i], SET_TYPE_STRING);
   }
  }
  free (current.zap);
  current.zap = tmp;
 }
 if (current.reset) {
  char **tmp = NULL;
  for (; current.reset[i]; i++) {
   if (service_usage_query (SERVICE_IS_PROVIDED, NULL, current.reset[i])) {
    tmp = (char **)setadd ((void **)tmp, (void *)current.reset[i], SET_TYPE_STRING);
   }
  }
  free (current.reset);
  current.reset = tmp;
 }
 if (current.reload) {
  char **tmp = NULL;
  for (; current.reload[i]; i++) {
   if (service_usage_query (SERVICE_IS_PROVIDED, NULL, current.reload[i])) {
    tmp = (char **)setadd ((void **)tmp, (void *)current.reload[i], SET_TYPE_STRING);
   }
  }
  free (current.reload);
  current.reload = tmp;
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_tb_target_state_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_plan_commit()", pthread_errno, "pthread_mutex_unlock() failed.");
 }

 mod_get_and_apply ();

 fb->task = MOD_SCHEDULER_PLAN_COMMIT_FINISH;
 status_update (fb);

// fputs (">> DONE WITH THAT SWITCH <<", stderr);

// do some more extra work if the plan was derived from a mode
 if (plan->mode) {
  char *cmdt;
  cmode = plan->mode;
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
 if (plan) {
  if (plan->changes.enable) free (plan->changes.enable);
  if (plan->changes.disable) free (plan->changes.disable);
  if (plan->changes.reset) free (plan->changes.reset);
  if (plan->changes.reload) free (plan->changes.reload);
  if (plan->changes.critical) free (plan->changes.critical);
  if (plan->changes.zap) free (plan->changes.zap);

  if (plan->used_modes) free (plan->used_modes);

  free (plan);
 }
 return 0;
}

double get_plan_progress (struct mloadplan *plan) {
 return 0.0;
}

void mod_sort_service_list_items_by_preference() {
 int pthread_errno = 0;
 struct stree *cur;

 if ((pthread_errno = pthread_mutex_lock (&ml_service_list_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_sort_service_list_items_by_preference()", pthread_errno, "pthread_mutex_lock() failed.");
 }

 cur = module_logics_service_list;

 while (cur) {
  struct lmodule **lm = (struct lmodule **)cur->value;

  if (lm) {
/* order modules that should be enabled according to the user's preference */
    uint32_t mpx, mpy, mpz = 0;
    char *pnode = NULL, **preference = NULL;

/* first make sure all modules marked as "deprecated" are last */
    for (mpx = 0; lm[mpx]; mpx++); mpx--;
    for (mpy = 0; mpy < mpx; mpy++) {
     if (lm[mpy]->module && (lm[mpy]->module->options & EINIT_MOD_DEPRECATED)) {
      struct lmodule *t = lm[mpx];
      lm[mpx] = lm[mpy];
      lm[mpy] = t;
      mpx--;
     }
    }

/* now to the sorting bit... */
    pnode = emalloc (strlen (cur->key)+18);
    pnode[0] = 0;
    strcat (pnode, "services-prefer-");
    strcat (pnode, cur->key);
    if ((preference = str2set (':', cfg_getstring (pnode, NULL)))) {
     for (mpx = 0; preference[mpx]; mpx++) {
      for (mpy = 0; lm[mpy]; mpy++) {
       if (lm[mpy]->module && lm[mpy]->module->rid && !strcmp(lm[mpy]->module->rid, preference[mpx])) {
        struct lmodule *tm = lm[mpy];

        lm[mpy] = lm[mpz];
        lm[mpz] = tm;

        mpz++;
       }
      }
     }
     free (preference);
    }

    free (pnode);
  }

  cur = streenext(cur);
 }

 if ((pthread_errno = pthread_mutex_unlock (&ml_service_list_mutex))) {
  bitch2(BITCH_EPTHREADS, "mod_sort_service_list_items_by_preference()", pthread_errno, "pthread_mutex_unlock() failed.");
 }
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

  mod_sort_service_list_items_by_preference();

  if ((pthread_errno = pthread_mutex_lock (&ml_unresolved_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_lock() failed.");
  }

  if (unresolved_services) {
   free (unresolved_services);
   unresolved_services = NULL;
  }
  if (broken_services) {
   free (broken_services);
   broken_services = NULL;
  }

  if ((pthread_errno = pthread_mutex_unlock (&ml_unresolved_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_unlock() failed.");
  }

  if ((pthread_errno = pthread_mutex_lock (&ml_group_data_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_lock() failed.");
  }

  if (module_logics_group_data) {
   streefree (module_logics_group_data);
   module_logics_group_data = NULL;
  }

  if ((pthread_errno = pthread_mutex_unlock (&ml_group_data_mutex))) {
   bitch2(BITCH_EPTHREADS, "module_logic_einit_event_handler()", pthread_errno, "pthread_mutex_unlock() failed.");
  }
 } else if ((ev->type == EVE_SERVICE_UPDATE) && (!(ev->status & STATUS_WORKING))) {
/* something's done now, update our lists */

  mod_done ((struct lmodule *)ev->para, ev->task);
 }
}

void module_logic_ipc_event_handler (struct einit_event *ev) {
 if (ev->set && ev->set[0] && ev->set[1] && ev->para) {
  if (!strcmp (ev->set[0], "list") && !strcmp (ev->set[1], "control-blocks")) {
   int pthread_errno = 0;
   if ((pthread_errno = pthread_mutex_lock (&ml_tb_target_state_mutex))) {
    bitch2(BITCH_EPTHREADS, "module_logic_ipc_event_handler()", pthread_errno, "pthread_mutex_lock() failed.");
   }

   if (target_state.enable) {
    char *r = set2str (' ', target_state.enable);
	if (r) {
     fprintf ((FILE *)ev->para, "target_state.enable = { %s }\n", r);
     free (r);
	}
   }
   if (target_state.disable) {
    char *r = set2str (' ', target_state.disable);
	if (r) {
     fprintf ((FILE *)ev->para, "target_state.disable = { %s }\n", r);
     free (r);
	}
   }
   if (target_state.reset) {
    char *r = set2str (' ', target_state.reset);
	if (r) {
     fprintf ((FILE *)ev->para, "target_state.reset = { %s }\n", r);
     free (r);
	}
   }
   if (target_state.reload) {
    char *r = set2str (' ', target_state.reload);
	if (r) {
     fprintf ((FILE *)ev->para, "target_state.reload = { %s }\n", r);
     free (r);
	}
   }
   if (target_state.zap) {
    char *r = set2str (' ', target_state.zap);
	if (r) {
     fprintf ((FILE *)ev->para, "target_state.zap = { %s }\n", r);
     free (r);
	}
   }
   if (target_state.critical) {
    char *r = set2str (' ', target_state.critical);
	if (r) {
     fprintf ((FILE *)ev->para, "target_state.critical = { %s }\n", r);
     free (r);
	}
   }

   if ((pthread_errno = pthread_mutex_unlock (&ml_tb_target_state_mutex))) {
    bitch2(BITCH_EPTHREADS, "module_logic_ipc_event_handler()", pthread_errno, "pthread_mutex_unlock() failed.");
   }

   if ((pthread_errno = pthread_mutex_lock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "module_logic_ipc_event_handler()", pthread_errno, "pthread_mutex_lock() failed.");
   }

   if (current.enable) {
    char *r = set2str (' ', current.enable);
	if (r) {
     fprintf ((FILE *)ev->para, "current.enable = { %s }\n", r);
     free (r);
	}
   }
   if (current.disable) {
    char *r = set2str (' ', current.disable);
	if (r) {
     fprintf ((FILE *)ev->para, "current.disable = { %s }\n", r);
     free (r);
	}
   }
   if (current.reset) {
    char *r = set2str (' ', current.reset);
	if (r) {
     fprintf ((FILE *)ev->para, "current.reset = { %s }\n", r);
     free (r);
	}
   }
   if (current.reload) {
    char *r = set2str (' ', current.reload);
	if (r) {
     fprintf ((FILE *)ev->para, "current.reload = { %s }\n", r);
     free (r);
	}
   }
   if (current.zap) {
    char *r = set2str (' ', current.zap);
	if (r) {
     fprintf ((FILE *)ev->para, "current.zap = { %s }\n", r);
     free (r);
	}
   }
   if (current.critical) {
    char *r = set2str (' ', current.critical);
	if (r) {
     fprintf ((FILE *)ev->para, "current.critical = { %s }\n", r);
     free (r);
	}
   }

   if ((pthread_errno = pthread_mutex_unlock (&ml_tb_current_mutex))) {
    bitch2(BITCH_EPTHREADS, "module_logic_ipc_event_handler()", pthread_errno, "pthread_mutex_unlock() failed.");
   }

   ev->flag = 1;
  }
 }
}
