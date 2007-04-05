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
#include <einit/module.h>
#include <einit/tree.h>
#include <pthread.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit-modules/ipc.h>
#include <einit-modules/configuration.h>

void module_logic_ipc_event_handler (struct einit_event *);
void module_logic_einit_event_handler (struct einit_event *);
double __mod_get_plan_progress_f (struct mloadplan *);

int _einit_module_logic_v3_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)
const struct smodule _einit_module_logic_v3_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Module Logic Core (V3)",
 .rid       = "module-logic-v3",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _einit_module_logic_v3_configure
};

module_register(_einit_module_logic_v3_self);

#endif

int _einit_module_logic_v3_cleanup (struct lmodule *this) {
 function_unregister ("module-logic-get-plan-progress", 1, __mod_get_plan_progress_f);

 event_ignore (EVENT_SUBSYSTEM_EINIT, module_logic_einit_event_handler);
 event_ignore (EVENT_SUBSYSTEM_IPC, module_logic_ipc_event_handler);

 return 0;
}

int _einit_module_logic_v3_configure (struct lmodule *this) {
 module_init(this);

 thismodule->cleanup = _einit_module_logic_v3_cleanup;

 event_listen (EVENT_SUBSYSTEM_IPC, module_logic_ipc_event_handler);
 event_listen (EVENT_SUBSYSTEM_EINIT, module_logic_einit_event_handler);

 function_register ("module-logic-get-plan-progress", 1, __mod_get_plan_progress_f);

 return 0;
}

struct module_taskblock
  current = { NULL, NULL, NULL },
  working = { NULL, NULL, NULL },
  target_state = { NULL, NULL, NULL };

struct stree *module_logics_service_list = NULL; // value is a (struct lmodule **)
struct stree *module_logics_group_data = NULL;
struct stree *ml_service_status_worker_threads = NULL;
struct stree *module_logics_chain_examine = NULL; // value is a (char **)

struct lmodule *mlist;

char **unresolved_services = NULL;
char **broken_services = NULL;

pthread_mutex_t
  ml_tb_current_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_tb_target_state_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_service_list_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_group_data_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_unresolved_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_mutex_apply_loop = PTHREAD_MUTEX_INITIALIZER,
  ml_chain_examine = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t
  ml_cond_apply_loop = PTHREAD_COND_INITIALIZER;

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

#define MARK_BROKEN                0x01
#define MARK_UNRESOLVED            0x02

char initdone = 0;

void mod_update_group (struct lmodule *lm);
char mod_isbroken (char *service);
char mod_mark (char *service, char task);

/* examine service status and (maybe) change dependant services */
void mod_examine (char *service);

char mod_done (struct lmodule *module, int task) {
 if (!module) return 0;

 mod_update_group (module);

 if ((task & MOD_ENABLE) && (module->status & STATUS_ENABLED)) {
  if (module->si && module->si->provides) {
   uint32_t x = 0;
   for (; module->si->provides[x]; x++) {
    emutex_lock(&ml_tb_current_mutex);

    current.enable = strsetdel (current.enable, module->si->provides[x]);

    emutex_unlock(&ml_tb_current_mutex);
   }
  }

  return 1;
 }

 if ((task & MOD_DISABLE) && (module->status & STATUS_DISABLED)) {
  if (module->si && module->si->provides) {
   uint32_t x = 0;
   for (; module->si->provides[x]; x++) {
    emutex_lock(&ml_tb_current_mutex);

    current.disable = strsetdel (current.disable, module->si->provides[x]);

    emutex_unlock(&ml_tb_current_mutex);
   }
  }

  return 1;
 }

 return 0;
}

struct group_data *mod_group_get_data (char *group) {
 struct group_data *ret = NULL;

 emutex_lock (&ml_group_data_mutex);

 struct stree *cur = module_logics_group_data ? streefind (module_logics_group_data, group, TREE_FIND_FIRST) : NULL;
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
    if (strmatch (gnode->arbattrs[r], "group")) {
     ret->members = str2set (':', gnode->arbattrs[r+1]);
    } else if (strmatch (gnode->arbattrs[r], "seq")) {
     if (strmatch (gnode->arbattrs[r+1], "any"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ANY;
     else if (strmatch (gnode->arbattrs[r+1], "all"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ALL;
     else if (strmatch (gnode->arbattrs[r+1], "any-iop"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ANY_IOP;
     else if (strmatch (gnode->arbattrs[r+1], "most"))
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

 emutex_unlock (&ml_group_data_mutex);

 return ret;
}

void mod_update_group (struct lmodule *lmx) {
 emutex_lock (&ml_group_data_mutex);

 struct stree *cur = module_logics_group_data;

 while (cur) {
  struct group_data *gd = (struct group_data *)cur->value;

  if (gd && gd->members) {
   ssize_t x = 0, mem = setcount ((const void **)gd->members), failed = 0, on = 0;
   struct lmodule **providers = NULL;
   char group_failed = 0;

   for (; gd->members[x]; x++) {
    if (mod_isbroken (gd->members[x])) {
     failed++;
    } else {
     struct stree *serv = NULL;

     emutex_lock (&ml_service_list_mutex);

     if (module_logics_service_list && (serv = streefind(module_logics_service_list, gd->members[x], TREE_FIND_FIRST))) {
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

    emutex_unlock (&ml_service_list_mutex);
   }

   if (providers) {
    if (gd->options & (MOD_PLAN_GROUP_SEQ_ANY | MOD_PLAN_GROUP_SEQ_ANY_IOP)) {
     if (on > 0) {
      service_usage_query_group (SERVICE_SET_GROUP_PROVIDERS, (struct lmodule *)providers, cur->key);
     } else if (failed >= mem) {
      group_failed = 1;
     }
    } else if (gd->options & MOD_PLAN_GROUP_SEQ_MOST) {
     if (on && ((on + failed) >= mem)) {
      service_usage_query_group (SERVICE_SET_GROUP_PROVIDERS, (struct lmodule *)providers, cur->key);
     } else if (failed >= mem) {
      group_failed = 1;
     }
    } else if (gd->options & MOD_PLAN_GROUP_SEQ_ALL) {
     if (on >= mem) {
      service_usage_query_group (SERVICE_SET_GROUP_PROVIDERS, (struct lmodule *)providers, cur->key);
     } else if (failed) {
      group_failed = 1;
     }
    } else {
     notice (2, "marking group %s broken (bad group type)", cur->key);

     mod_mark (cur->key, MARK_BROKEN);
    }
   }

   if (group_failed) {
    notice (2, "marking group %s broken (group requirements failed)", cur->key);

    mod_mark (cur->key, MARK_BROKEN);
   }

   if (providers) free (providers);
  }

  cur = streenext (cur);
 }

 emutex_unlock (&ml_group_data_mutex);
}

char mod_disable_users (struct lmodule *module) {
 if (!service_usage_query(SERVICE_NOT_IN_USE, module, NULL)) {
  ssize_t i = 0;
  char **need = NULL;
  char **t = service_usage_query_cr (SERVICE_GET_SERVICES_THAT_USE, module, NULL);
  char retval = 1;

  if (t) {
   for (; t[i]; i++) {
    if (mod_isbroken (t[i])) {
     if (need) free (need);
      return 0;
    } else {
     emutex_lock (&ml_tb_current_mutex);

     if (!inset ((const void **)current.disable, (void *)t[i], SET_TYPE_STRING)) {
      retval = 2;
      need = (char **)setadd ((void **)need, t[i], SET_TYPE_STRING);
     }

     emutex_unlock (&ml_tb_current_mutex);
    }
   }

   if (retval == 2) {
    emutex_lock (&ml_tb_current_mutex);

    char **tmp = (char **)setcombine ((const void **)current.disable, (const void **)need, SET_TYPE_STRING);
    if (current.disable) free (current.disable);
    current.disable = tmp;

    emutex_unlock (&ml_tb_current_mutex);
   }
  }

  return retval;
 }

 return 0;
}

char mod_enable_requirements (struct lmodule *module) {
 if (!service_usage_query(SERVICE_REQUIREMENTS_MET, module, NULL)) {
  char retval = 1;
  if (module->si && module->si->requires) {
   ssize_t i = 0;
   char **need = NULL;

   for (; module->si->requires[i]; i++) {
    if (mod_isbroken (module->si->requires[i])) {
     if (need) free (need);
     return 0;
    } else if (!service_usage_query (SERVICE_IS_PROVIDED, NULL, module->si->requires[i])) {
     emutex_lock (&ml_tb_current_mutex);

     if (!inset ((const void **)current.enable, (void *)module->si->requires[i], SET_TYPE_STRING)) {
      retval = 2;
      need = (char **)setadd ((void **)need, module->si->requires[i], SET_TYPE_STRING);
     }

     emutex_unlock (&ml_tb_current_mutex);
    }
   }

   if (retval == 2) {
    emutex_lock (&ml_tb_current_mutex);

    char **tmp = (char **)setcombine ((const void **)current.enable, (const void **)need, SET_TYPE_STRING);
    if (current.enable) free (current.enable);
    current.enable = tmp;

    emutex_unlock (&ml_tb_current_mutex);
   }
  }

  return retval;
 }

 return 0;
}

char mod_isbroken (char *service) {
 int retval = 0;

 emutex_lock (&ml_unresolved_mutex);

 retval = inset ((const void **)broken_services, (void *)service, SET_TYPE_STRING) ||
          inset ((const void **)unresolved_services, (void *)service, SET_TYPE_STRING);

 emutex_unlock (&ml_unresolved_mutex);

 return retval;
}

char mod_mark (char *service, char task) {
 char retval = 0;

 emutex_lock (&ml_unresolved_mutex);

 if ((task & MARK_BROKEN) && !inset ((const void **)broken_services, (void *)service, SET_TYPE_STRING)) {
  broken_services = (char **)setadd ((void **)broken_services, (void *)service, SET_TYPE_STRING);
 }
 if ((task & MARK_UNRESOLVED) && !inset ((const void **)unresolved_services, (void *)service, SET_TYPE_STRING)) {
  unresolved_services = (char **)setadd ((void **)unresolved_services, (void *)service, SET_TYPE_STRING);
 }

 emutex_unlock (&ml_unresolved_mutex);

 return retval;
}

void mod_apply (struct ma_task *task) {
 if (task && task->st) {
  struct stree *des = task->st;
  struct lmodule **lm = (struct lmodule **)des->value;

  if (lm && lm[0]) {
   struct lmodule *first = lm[0];
   int any_ok = 0;

   do {
    struct lmodule *current = lm[0];

    if ((task->task & MOD_DISABLE) && ((lm[0]->status & STATUS_DISABLED) || (lm[0]->status == STATUS_IDLE))) {
     goto skip_module;
    }
    if ((task->task & MOD_ENABLE) && (lm[0]->status & STATUS_ENABLED)) {
     any_ok = 1;
     goto skip_module;
    }

    if (task->task & MOD_ENABLE) if (mod_enable_requirements (current)) {
     free (task);
     return;
    }
    if (task->task & MOD_DISABLE) if (mod_disable_users (current)) {
     free (task);
     return;
    }

//    int retval = 
      mod (task->task, current, NULL);

/* check module status or return value to find out if it's appropriate for the task */
    if (((task->task & MOD_DISABLE) && ((lm[0]->status & STATUS_DISABLED) || (lm[0]->status == STATUS_IDLE))) ||
        ((task->task & MOD_ENABLE) && (lm[0]->status & STATUS_ENABLED))) {
     any_ok = 1;
    }

    if (any_ok && (task->task & MOD_ENABLE)) {
     free (task);
     return;
    }

    skip_module:
/* next module */
    emutex_lock (&ml_service_list_mutex);

/* make sure there's not been a different thread that did what we want to do */
    if ((lm[0] == current) && lm[1]) {
     ssize_t rx = 1;

     notice (10, "service %s: done with module %s, rotating the list", des->key, (current->module && current->module->rid ? current->module->rid : "unknown"));

     for (; lm[rx]; rx++) {
      lm[rx-1] = lm[rx];
     }

     lm[rx-1] = current;
    }

    emutex_unlock (&ml_service_list_mutex);
   } while (lm[0] != first);
/* if we tried to enable something and end up here, it means we did a complete
   round-trip and nothing worked */

   emutex_lock (&ml_tb_current_mutex);

   switch (task->task) {
    case MOD_ENABLE: current.enable = strsetdel(current.enable, des->key); break;
    case MOD_DISABLE: current.disable = strsetdel(current.disable, des->key); break;
   }

   emutex_unlock (&ml_tb_current_mutex);

   if (any_ok) {
    free (task);
    return;
   }

/* mark service broken if stuff went completely wrong */
   notice (2, "ran out of options for service %s, marking as broken", des->key);

   mod_mark (des->key, MARK_BROKEN);
  }

  free (task);
  return;
 }
}

void mod_get_and_apply_recurse (int task) {
 ssize_t x = 0, nservices;
 char **services = NULL;
 char **now = NULL;
 struct stree *defer = NULL;
 pthread_t **subthreads = NULL;
 pthread_t th;
 char dm = 2, repeat = 0;

 do {
 repeat = 0;

 while (dm & 2) {
  dm = 0;

  if (now) { free (now); now = NULL; }
  if (defer) { streefree (defer); defer = NULL; }
  if (services) { free (services); services = NULL; }

  emutex_lock (&ml_tb_current_mutex);

  switch (task) {
   case MOD_ENABLE: services = (char **)setdup((const void **)current.enable, SET_TYPE_STRING); break;
   case MOD_DISABLE: services = (char **)setdup((const void **)current.disable, SET_TYPE_STRING); break;
  }

  emutex_unlock (&ml_tb_current_mutex);

  if (!services) return;
  nservices = setcount ((const void **)services);

  for (x = 0; services[x]; x++) {
   char is_provided = service_usage_query (SERVICE_IS_PROVIDED, NULL, services[x]);
   char skip = 0;

   if ((task & MOD_ENABLE) && is_provided) {
    emutex_lock (&ml_tb_current_mutex);
    current.enable = strsetdel(current.enable, services[x]);
    emutex_unlock (&ml_tb_current_mutex);

    skip = 1;
   }
   if ((task & MOD_DISABLE) && !is_provided) {
    emutex_lock (&ml_tb_current_mutex);
    current.disable = strsetdel(current.disable, services[x]);
    emutex_unlock (&ml_tb_current_mutex);

    skip = 1;
   }

   if (skip) continue;

   if (mod_isbroken (services[x])) {
    emutex_lock (&ml_tb_current_mutex);

    switch (task) {
     case MOD_ENABLE: current.enable = strsetdel(current.enable, services[x]); break;
     case MOD_DISABLE: current.disable = strsetdel(current.disable, services[x]); break;
    }

    emutex_unlock (&ml_tb_current_mutex);
   } else {
    emutex_lock (&ml_service_list_mutex);

    struct stree *des = module_logics_service_list ? streefind (module_logics_service_list, services[x], TREE_FIND_FIRST) : NULL;
    struct lmodule **lm = des ? (struct lmodule **)des->value : NULL;

    emutex_unlock (&ml_service_list_mutex);

    if (!des) {
     struct group_data *gd = mod_group_get_data(services[x]);
     if (gd && gd->members) {

      emutex_lock (&ml_tb_current_mutex);

      if (task & MOD_ENABLE) {
       if (gd->options & (MOD_PLAN_GROUP_SEQ_ANY | MOD_PLAN_GROUP_SEQ_ANY_IOP)) {
        ssize_t r = 0;

        for (; gd->members[r]; r++) {
         if (!mod_isbroken (gd->members[r])) {
          if (!inset ((const void **)current.enable, (void *)gd->members[r], SET_TYPE_STRING)) {
           current.enable = (char **)setadd ((void **)current.enable, (void *)gd->members[r], SET_TYPE_STRING);
           dm |= 2;
          }
          break;
         }
        }

        if (!gd->members[r]) {
         notice (2, "marking group %s broken (group requirement %s failed)", services[x], gd->members[r]);

         mod_mark (services[x], MARK_BROKEN);
        }
       } else if (gd->options & (MOD_PLAN_GROUP_SEQ_ALL | MOD_PLAN_GROUP_SEQ_MOST)) {
        ssize_t r = 0;

        for (; gd->members[r]; r++) {
         if (!mod_isbroken (gd->members[r])) {
          if (!inset ((const void **)current.enable, (void *)gd->members[r], SET_TYPE_STRING)) {
           current.enable = (char **)setadd ((void **)current.enable, (void *)gd->members[r], SET_TYPE_STRING);
           dm |= 2;
          }
         } else if (gd->options & (MOD_PLAN_GROUP_SEQ_ALL)) {
/* all required: any broken modules mean we're screwed */
          notice (2, "marking group %s broken (group requirement %s failed)", services[x], gd->members[r]);

          mod_mark (services[x], MARK_BROKEN);
          dm |= 2;
          break;
         }
        }
       }
      }
#define mod_apply_recurse_group_rec_add(CONSTANT, variable)\
      if (task & CONSTANT) {\
       ssize_t r = 0, broken = 0, nprov = 0;\
\
       for (; gd->members[r]; r++) {\
        if (!service_usage_query (SERVICE_IS_PROVIDED, NULL, gd->members[r])) {\
         nprov++;\
        } else {\
         if (!mod_isbroken (gd->members[r])) {\
          if (!inset ((const void **)current.variable, (void *)gd->members[r], SET_TYPE_STRING)) {\
           current.variable = (char **)setadd ((void **)current.variable, (void *)gd->members[r], SET_TYPE_STRING);\
           dm |= 2;\
          }\
         } else\
          broken++;\
        }\
       }\
\
       if ((nprov + broken) == r) {\
        current.variable = strsetdel (current.variable, services[x]);\
       }\
       if (broken == r) {\
        notice (2, "marking group %s broken (all requirements failed)", services[x]);\
        mod_mark (services[x], MARK_BROKEN);\
       }\
      }
      mod_apply_recurse_group_rec_add(MOD_DISABLE, disable);

      emutex_unlock (&ml_tb_current_mutex);
     } else {
      notice (2, "marking %s as unresolved", services[x]);

      mod_mark (services[x], MARK_UNRESOLVED);
     }
    } else if (lm) {
     char isdone = 0;
     ssize_t i = 0;

/* see if we're going to need to recurse further, repeat this if so */
     if (task & MOD_ENABLE) {
      char rec = mod_enable_requirements (lm[0]);
      if (rec == 2) {
       dm |= 2;
       continue;
      }
     }
     if (task & MOD_DISABLE) {
      char rec = mod_disable_users (lm[0]);
      if (rec == 2) {
       dm |= 2;
       continue;
      }
     }

/* always do resets/reloads/zaps */
     if ((task & MOD_ENABLE) || (task & MOD_DISABLE)) {
      for (; lm[i]; i++) {
       isdone |= mod_done(lm[i], task);
      }
     }

     if (!isdone) {
      if (task & MOD_ENABLE) {
       if (lm[0]->si && lm[0]->si->before) {
        ssize_t ix = 0;
        for (ix = 0; lm[0]->si->before[ix]; ix++) {
         if (inset_pattern ((const void **)now, (void *)lm[0]->si->before[ix], SET_TYPE_STRING)) {
          now = strsetdel (now, lm[0]->si->before[ix]);
          defer = streeadd (defer, lm[0]->si->before[ix], NULL, SET_NOALLOC, NULL);
//          defer = (char **)setadd ((void **)defer, (void *)lm[0]->si->before[ix], SET_TYPE_STRING);
         }
        }
       }
       if (lm[0]->si && lm[0]->si->after) {
        ssize_t ix = 0;
        for (ix = 0; lm[0]->si->after[ix]; ix++) {
         if (inset_pattern ((const void **)services, (void *)lm[0]->si->after[ix], SET_TYPE_STRING)) {
          defer = streeadd (defer, services[x], NULL, SET_NOALLOC, NULL);
//          defer = (char **)setadd ((void **)defer, (void *)services[x], SET_TYPE_STRING);
         }
        }
       }
      }

      if (!defer || !streefind (defer, services[x], TREE_FIND_FIRST))
       now = (char **)setadd ((void **)now, (void *)services[x], SET_TYPE_STRING);
     }
    } else {
     notice (2, "marking %s broken (service found but no modules)", services[x]);

     mod_mark (services[x], MARK_BROKEN);
    }
   }
  }
 }

 if (!now && defer) {
  struct stree *dcur = defer;

  while (dcur) {
   now = (char **)setadd ((void **)now, (const void *)dcur->key, SET_TYPE_STRING);

   dcur = streenext(dcur);
  }

  streefree (defer);
  defer = NULL;
 }

 if (now) {
  nservices = setcount ((const void **)now);
  for (x = 0; now[x]; x++) {
   emutex_lock (&ml_service_list_mutex);

   struct stree *des = module_logics_service_list ? streefind (module_logics_service_list, now[x], TREE_FIND_FIRST) : NULL;
   struct lmodule **lm = des ? (struct lmodule **)des->value : NULL;

   emutex_unlock (&ml_service_list_mutex);

   if (lm) {
    struct ma_task *subtask_data = ecalloc (1, sizeof (struct ma_task));
    subtask_data->st = des;
    subtask_data->task = task;

    if ((nservices - x) == 1) {
     mod_apply (subtask_data);
    } else {
     if (!ethread_create (&th, NULL, (void *(*)(void *))mod_apply, (void *)subtask_data)) {
      subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
     } else {
      mod_apply (subtask_data);
     }
    }
   }
  }

  free (now);
 }

 if (subthreads) {
  uint32_t u;
  uintptr_t ret;

//  pthread_cond_wait (&ml_cond_apply_loop, &ml_mutex_apply_loop);

  for (u = 0; subthreads[u]; u++) {
   ethread_join (*(subthreads[u]), (void *)&ret);
  }
  free (subthreads); subthreads = NULL;
 }

 free (services);

/*  char **left
  switch (task) {
   case MOD_ENABLE: repeat = 1; break;
   case MOD_DISABLE: repeat = 1; break;
  }*/

  repeat = 0;


 } while (repeat);
}

void mod_get_and_apply () {
 while (current.disable || current.enable) {
  pthread_t **subthreads = NULL;
  pthread_t th;

  if (current.disable) {
   if (!ethread_create (&th, NULL, (void *(*)(void *))mod_get_and_apply_recurse, (void *)MOD_DISABLE)) {
    subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
   } else {
    mod_get_and_apply_recurse (MOD_DISABLE);
   }
  }

  if (current.enable) {
   if (!ethread_create (&th, NULL, (void *(*)(void *))mod_get_and_apply_recurse, (void *)MOD_ENABLE)) {
    subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
   } else {
    mod_get_and_apply_recurse (MOD_ENABLE);
   }
  }

  if (subthreads) {
   uint32_t u;
   uintptr_t ret;

   for (u = 0; subthreads[u]; u++) {
    ethread_join (*(subthreads[u]), (void *)&ret);
   }
   free (subthreads); subthreads = NULL;
  }
 }

 emutex_lock (&ml_unresolved_mutex);

 if (broken_services) {
  struct einit_event ee = evstaticinit(EVENT_FEEDBACK_BROKEN_SERVICES);
  ee.set = (void **)broken_services;

  event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
  evstaticdestroy (ee);

  free (broken_services);
  broken_services = NULL;
 }
 if (unresolved_services) {
  struct einit_event ee = evstaticinit(EVENT_FEEDBACK_UNRESOLVED_SERVICES);
  ee.set = (void **)unresolved_services;

  event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
  evstaticdestroy (ee);

  free (unresolved_services);
  unresolved_services = NULL;
 }

 emutex_unlock (&ml_unresolved_mutex);
}

void cross_taskblock (struct module_taskblock *source, struct module_taskblock *target) {
 if (source->enable) {
  char **tmp = (char **)setcombine ((const void **)target->enable, (const void **)source->enable, SET_TYPE_STRING);
  if (target->enable) free (target->enable);
  target->enable = tmp;

  tmp = (char **)setslice ((const void **)target->disable, (const void **)source->enable, SET_TYPE_STRING);
  if (target->disable) free (target->disable);
  target->disable = tmp;
 }
 if (source->critical) {
  char **tmp = (char **)setcombine ((const void **)target->critical, (const void **)source->critical, SET_TYPE_STRING);
  if (target->critical) free (target->critical);
  target->critical = tmp;
 }

 if (source->disable) {
  char **tmp = (char **)setcombine ((const void **)target->disable, (const void **)source->disable, SET_TYPE_STRING);
  if (target->disable) free (target->disable);
  target->disable = tmp;

  tmp = (char **)setslice ((const void **)target->enable, (const void **)source->disable, SET_TYPE_STRING);
  if (target->enable) free (target->enable);
  target->enable = tmp;

  tmp = (char **)setslice ((const void **)target->critical, (const void **)source->disable, SET_TYPE_STRING);
  if (target->critical) free (target->critical);
  target->critical = tmp;
 }
}

struct mloadplan *mod_plan (struct mloadplan *plan, char **atoms, unsigned int task, struct cfgnode *mode) {
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
  char **critical = str2set (':', cfg_getstring ("enable/critical", mode));

  if (!enable)
   enable  = str2set (':', cfg_getstring ("enable/mod", mode));
  if (!disable)
   disable = str2set (':', cfg_getstring ("disable/mod", mode));

  if (mode->arbattrs) for (; mode->arbattrs[xi]; xi+=2) {
   if (strmatch(mode->arbattrs[xi], "base")) {
    base = str2set (':', mode->arbattrs[xi+1]);
   }
  }

  if (base) {
   int y = 0;
   struct cfgnode *cno;
   while (base[y]) {
    if (!inset ((const void **)plan->used_modes, (void *)base[y], SET_TYPE_STRING)) {
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
   char **tmp = (char **)setcombine ((const void **)plan->changes.enable, (const void **)enable, SET_TYPE_STRING);
   if (plan->changes.enable) free (plan->changes.enable);
   plan->changes.enable = tmp;
  }
  if (disable) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.disable, (const void **)disable, SET_TYPE_STRING);
   if (plan->changes.disable) free (plan->changes.disable);
   plan->changes.disable = tmp;
  }
  if (critical) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.critical, (const void **)critical, SET_TYPE_STRING);
   if (plan->changes.critical) free (plan->changes.critical);
   plan->changes.critical = tmp;
  }

  if (mode->id) {
   plan->used_modes = (char **)setadd ((void **)plan->used_modes, mode->id, SET_TYPE_STRING);
  }

  plan->mode = mode;
 } else {
  if (task & MOD_ENABLE) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.enable, (const void **)atoms, SET_TYPE_STRING);
   if (plan->changes.enable) free (plan->changes.enable);
   plan->changes.enable = tmp;
  }
  if (task & MOD_DISABLE) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.disable, (const void **)atoms, SET_TYPE_STRING);
   if (plan->changes.disable) free (plan->changes.disable);
   plan->changes.disable = tmp;
  }
 }

 disable_all = inset ((const void **)plan->changes.disable, (void *)"all", SET_TYPE_STRING);
 disable_all_but_feedback = inset ((const void **)plan->changes.disable, (void *)"all-but-feedback", SET_TYPE_STRING);

 if (disable_all || disable_all_but_feedback) {
  struct stree *cur;
  ssize_t i = 0;
  char **tmpy = service_usage_query_cr (SERVICE_GET_ALL_PROVIDED, NULL, NULL);

  emutex_lock (&ml_service_list_mutex);
  emutex_lock (&ml_tb_target_state_mutex);
//  char **tmpx = (char **)setcombine ((const void **)plan->changes.enable, (const void **)target_state.enable, SET_TYPE_STRING);
  char **tmpx = (char **)setcombine ((const void **)tmpy, (const void **)target_state.enable, SET_TYPE_STRING);

  emutex_unlock (&ml_tb_target_state_mutex);

  char **tmp = (char **)setcombine ((const void **)tmpx, (const void **)plan->changes.disable, SET_TYPE_STRING);

  free (tmpx);
  free (tmpy);

  if (plan->changes.disable) {
   free (plan->changes.disable);
   plan->changes.disable = NULL;
  }

  if (tmp) {
   for (; tmp[i]; i++) {
    char add = 1;

    if (inset ((const void **)plan->changes.disable, (void *)tmp[i], SET_TYPE_STRING)) {
     add = 0;
    } else if ((disable_all && strmatch(tmp[i], "all")) ||
               (disable_all_but_feedback && strmatch(tmp[i], "all-but-feedback"))) {
     add = 0;
    } else if (module_logics_service_list && (cur = streefind (module_logics_service_list, tmp[i], TREE_FIND_FIRST))) {
     struct lmodule **lm = (struct lmodule **)cur->value;
     if (lm) {
      ssize_t y = 0;
      for (; lm[y]; y++) {
       if (disable_all_but_feedback && (lm[y]->module->mode & EINIT_MOD_FEEDBACK)) {
        add = 0;

        break;
       }
      }
     }
    } else if (!service_usage_query (SERVICE_IS_PROVIDED, NULL, tmp[i])) {
     add = 0;
    }

    if (add) {
     plan->changes.disable = (char **)setadd((void **)plan->changes.disable, (void *)tmp[i], SET_TYPE_STRING);
    }
   }

   free (tmp);
  }

  emutex_unlock (&ml_service_list_mutex);
 }

 return plan;
}

unsigned int mod_plan_commit (struct mloadplan *plan) {
 struct einit_event *fb = evinit (EVE_FEEDBACK_PLAN_STATUS);

 if (!plan) return 0;

// do some extra work if the plan was derived from a mode
 if (plan->mode) {
  char *cmdt;
  cmode = plan->mode;

  struct einit_event eex = evstaticinit (EVE_SWITCHING_MODE);
  eex.para = (void *)plan->mode;
  event_emit (&eex, EINIT_EVENT_FLAG_BROADCAST);
  evstaticdestroy (eex);

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

 emutex_lock (&ml_tb_target_state_mutex);

 cross_taskblock (&(plan->changes), &target_state);

 emutex_lock (&ml_tb_current_mutex);

 cross_taskblock (&target_state, &current);

 uint32_t i = 0;

 if (current.enable) {
  char **tmp = NULL;
  for (i = 0; current.enable[i]; i++) {
   if (!service_usage_query (SERVICE_IS_PROVIDED, NULL, current.enable[i])) {
    tmp = (char **)setadd ((void **)tmp, (void *)current.enable[i], SET_TYPE_STRING);
   }
  }
  free (current.enable);
  current.enable = tmp;
 }
 if (current.disable) {
  char **tmp = NULL;
  for (i = 0; current.disable[i]; i++) {
   if (service_usage_query (SERVICE_IS_PROVIDED, NULL, current.disable[i])) {
    tmp = (char **)setadd ((void **)tmp, (void *)current.disable[i], SET_TYPE_STRING);
   }
  }
  free (current.disable);
  current.disable = tmp;
 }

 emutex_unlock (&ml_tb_current_mutex);
 emutex_unlock (&ml_tb_target_state_mutex);

 mod_get_and_apply ();

 fb->task = MOD_SCHEDULER_PLAN_COMMIT_FINISH;
 status_update (fb);

// do some more extra work if the plan was derived from a mode
 if (plan->mode) {
  char *cmdt;
  cmode = plan->mode;
  amode = plan->mode;

  struct einit_event eex = evstaticinit (EVE_MODE_SWITCHED);
  eex.para = (void *)plan->mode;
  event_emit (&eex, EINIT_EVENT_FLAG_BROADCAST);
  evstaticdestroy (eex);

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
  if (plan->changes.critical) free (plan->changes.critical);

  if (plan->used_modes) free (plan->used_modes);

  free (plan);
 }
 return 0;
}

double __mod_get_plan_progress_f (struct mloadplan *plan) {
 if (plan) {
  return 0.0;
 } else {
  double all = 0, left = 0;
  emutex_lock (&ml_tb_target_state_mutex);
  if (target_state.enable) all += setcount ((const void **)target_state.enable);
  if (target_state.disable) all += setcount ((const void **)target_state.disable);
  emutex_unlock (&ml_tb_target_state_mutex);

  emutex_lock (&ml_tb_current_mutex);
  if (current.enable) left += setcount ((const void **)current.enable);
  if (current.disable) left += setcount ((const void **)current.disable);
  emutex_unlock (&ml_tb_current_mutex);

  return 1.0 - (double)(left / all);
 }
}

void mod_sort_service_list_items_by_preference() {
 struct stree *cur;

 emutex_lock (&ml_service_list_mutex);

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
     debugx ("applying module preferences for service %s", cur->key);

     for (mpx = 0; preference[mpx]; mpx++) {
      for (mpy = 0; lm[mpy]; mpy++) {
       if (lm[mpy]->module && lm[mpy]->module->rid && strmatch(lm[mpy]->module->rid, preference[mpx])) {
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

 emutex_unlock (&ml_service_list_mutex);
}


int mod_switchmode (char *mode) {
 if (!mode) return -1;
 struct cfgnode *cur = cfg_findnode (mode, EI_NODETYPE_MODE, NULL);
 struct mloadplan *plan = NULL;

  if (!cur) {
   notice (1, "scheduler: scheduled mode not defined, aborting");
   return -1;
  }

  plan = mod_plan (NULL, NULL, 0, cur);
  if (!plan) {
   notice (1, "scheduler: scheduled mode defined but nothing to be done");
  } else {
   pthread_t th;
   mod_plan_commit (plan);
/* make it so that the erase operation will not disturb the flow of the program */
   ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))mod_plan_free, (void *)plan);
  }

 return 0;
}

int mod_modaction (char **argv) {
 int argc = setcount ((const void **)argv), ret = 1;
 int32_t task = 0;
 struct mloadplan *plan;
 if (!argv || (argc != 2)) return -1;

 if (strmatch (argv[1], "enable")) task = MOD_ENABLE;
 else if (strmatch (argv[1], "disable")) task = MOD_DISABLE;
 else {
  struct lmodule **tm = NULL;
  uint32_t r = 0;

  emutex_lock (&ml_service_list_mutex);
  if (module_logics_service_list) {
   struct stree *cur = streefind (module_logics_service_list, argv[0], TREE_FIND_FIRST);
   if (cur) {
    tm = cur->value;
   }
  }

  emutex_unlock (&ml_service_list_mutex);

  ret = 1;
  if (tm) {
   if (strmatch (argv[1], "status")) {
    for (; tm[r]; r++) {
     if (tm[r]->status & STATUS_WORKING) {
      ret = 2;
      break;
     }
     if (tm[r]->status & STATUS_ENABLED) {
      ret = 0;
      break;
     }
    }
   } else {
    for (; tm[r]; r++) {
     int retx = mod (MOD_CUSTOM, tm[r], argv[1]);

     if (retx == STATUS_OK)
      ret = 0;
    }
   }
  }

  return ret;
 }

 argv[1] = NULL;

 if ((plan = mod_plan (NULL, argv, task, NULL))) {
  pthread_t th;

  ret = mod_plan_commit (plan);

  ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))mod_plan_free, (void *)plan);
 }

// free (argv[0]);
// free (argv);

 return ret;
}

void module_logic_einit_event_handler(struct einit_event *ev) {
 if ((ev->type == EVE_UPDATE_CONFIGURATION) && !initdone) {
  initdone = 1;

  function_register("module-logic-get-plan-progress", 1, (void (*)(void *))__mod_get_plan_progress_f);
 } else if (ev->type == EVE_MODULE_LIST_UPDATE) {
/* update list with services */
  struct stree *new_service_list = NULL;
  struct lmodule *cur = mlist;

  emutex_lock (&ml_service_list_mutex);

  while (cur) {
   if (cur->si && cur->si->provides) {
    ssize_t i = 0;

    for (; cur->si->provides[i]; i++) {
     struct stree *slnode = new_service_list ?
       streefind (new_service_list, cur->si->provides[i], TREE_FIND_FIRST) :
       NULL;
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

  emutex_unlock (&ml_service_list_mutex);

  mod_sort_service_list_items_by_preference();

  emutex_lock (&ml_unresolved_mutex);

  if (unresolved_services) {
   free (unresolved_services);
   unresolved_services = NULL;
  }
  if (broken_services) {
   free (broken_services);
   broken_services = NULL;
  }

  emutex_unlock (&ml_unresolved_mutex);
  emutex_lock (&ml_group_data_mutex);

  if (module_logics_group_data) {
   streefree (module_logics_group_data);
   module_logics_group_data = NULL;
  }

  emutex_unlock (&ml_group_data_mutex);
 } else if ((ev->type == EVE_SERVICE_UPDATE) && (!(ev->status & STATUS_WORKING))) {
/* something's done now, update our lists */
  mod_done ((struct lmodule *)ev->para, ev->task);

  pthread_cond_broadcast (&ml_cond_apply_loop);
 } else switch (ev->type) {
  case EVE_SWITCH_MODE:
   if (!ev->string) return;
   else {
    if (ev->para) {
     struct einit_event ee = evstaticinit(EVENT_FEEDBACK_REGISTER_FD);
     ee.para = ev->para;
     event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
     evstaticdestroy(ee);
    }

    mod_switchmode (ev->string);

    if (ev->para) {
     struct einit_event ee = evstaticinit(EVENT_FEEDBACK_UNREGISTER_FD);
     ee.para = ev->para;
     event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
     evstaticdestroy(ee);
    }
   }
   return;
  case EVE_CHANGE_SERVICE_STATUS:
   if (!ev->set) return;
   else {
    if (ev->para) {
     struct einit_event ee = evstaticinit(EVENT_FEEDBACK_REGISTER_FD);
     ee.para = ev->para;
     event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
     evstaticdestroy(ee);
    }

    if (mod_modaction ((char **)ev->set)) {
     ev->integer = 1;
    }

    if (ev->para) {
     struct einit_event ee = evstaticinit(EVENT_FEEDBACK_UNREGISTER_FD);
     ee.para = ev->para;
     event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
     evstaticdestroy(ee);
    }
   }
   return;
 }
}

void module_logic_ipc_event_handler (struct einit_event *ev) {
 if (ev->set && ev->set[0] && ev->set[1] && ev->para) {
  if (strmatch (ev->set[0], "examine") && strmatch (ev->set[1], "configuration")) {
   struct cfgnode *cfgn = cfg_findnode ("mode-enable", 0, NULL);
   char **modes = NULL;

   while (cfgn) {
    if (cfgn->arbattrs && cfgn->mode && cfgn->mode->id && (!modes || !inset ((const void **)modes, (const void *)cfgn->mode->id, SET_TYPE_STRING))) {
     uint32_t i = 0;
     modes = (char **)setadd ((void **)modes, (void *)cfgn->mode->id, SET_TYPE_STRING);

     for (i = 0; cfgn->arbattrs[i]; i+=2) {
      if (strmatch(cfgn->arbattrs[i], "services")) {
       char **tmps = str2set (':', cfgn->arbattrs[i+1]);

       if (tmps) {
        uint32_t i = 0;

        emutex_lock(&ml_service_list_mutex);

        for (; tmps[i]; i++) {
         if (!streefind (module_logics_service_list, tmps[i], TREE_FIND_FIRST) && !mod_group_get_data(tmps[i])) {
          eprintf ((FILE *)ev->para, " * mode \"%s\": service \"%s\" referenced but not found\n", cfgn->mode->id, tmps[i]);
          ev->task++;
         }
        }

        emutex_unlock(&ml_service_list_mutex);

        free (tmps);
       }
       break;
      }
     }
    }

    cfgn = cfg_findnode ("mode-enable", 0, cfgn);
   }

   ev->flag = 1;
  } else if (strmatch (ev->set[0], "list")) {
   if (strmatch (ev->set[1], "services")) {
    struct stree *modes = NULL;
    struct stree *cur = NULL;
    struct cfgnode *cfgn = cfg_findnode ("mode-enable", 0, NULL);

    while (cfgn) {
     if (cfgn->arbattrs && cfgn->mode && cfgn->mode->id && (!modes || !streefind (modes, cfgn->mode->id, TREE_FIND_FIRST))) {
      uint32_t i = 0;
      for (i = 0; cfgn->arbattrs[i]; i+=2) {
       if (strmatch(cfgn->arbattrs[i], "services")) {
        char **tmps = str2set (':', cfgn->arbattrs[i+1]);

        modes = streeadd (modes, cfgn->mode->id, tmps, SET_NOALLOC, tmps);

        break;
       }
      }
     }

     cfgn = cfg_findnode ("mode-enable", 0, cfgn);
    }

    emutex_lock(&ml_service_list_mutex);

    cur = module_logics_service_list;

    while (cur) {
     char **inmodes = NULL;
     struct stree *mcur = modes;

     while (mcur) {
      if (inset ((const void **)mcur->value, (void *)cur->key, SET_TYPE_STRING)) {
       inmodes = (char **)setadd((void **)inmodes, (void *)mcur->key, SET_TYPE_STRING);
      }

      mcur = streenext(mcur);
     }

     if (inmodes) {
      char *modestr;
      if (ev->status & EIPC_OUTPUT_XML) {
       modestr = set2str (':', (const char **)inmodes);
       eprintf ((FILE *)ev->para, " <service id=\"%s\" used-in=\"%s\">\n", cur->key, modestr);
      } else {
       modestr = set2str (' ', (const char **)inmodes);
       eprintf ((FILE *)ev->para, (ev->status & EIPC_OUTPUT_ANSI) ?
                                "\e[1mservice \"%s\" (%s)\n\e[0m" :
                                  "service \"%s\" (%s)\n",
                                cur->key, modestr);
      }
      free (modestr);
      free (inmodes);
     } else if (!(ev->status & EIPC_ONLY_RELEVANT)) {
      if (ev->status & EIPC_OUTPUT_XML) {
       eprintf ((FILE *)ev->para, " <service id=\"%s\">\n", cur->key);
      } else {
       eprintf ((FILE *)ev->para, (ev->status & EIPC_OUTPUT_ANSI) ?
                                "\e[1mservice \"%s\" (not in any mode)\e[0m\n" :
                                  "service \"%s\" (not in any mode)\n",
                                cur->key);
      }
     }

     if (inmodes || (!(ev->status & EIPC_ONLY_RELEVANT))) {
      if (ev->status & EIPC_OUTPUT_XML) {
       if (cur->value) {
        struct lmodule **xs = cur->value;
        uint32_t u = 0;
        for (u = 0; xs[u]; u++) {
         eprintf ((FILE *)ev->para, "  <module id=\"%s\" name=\"%s\" />\n",
                   xs[u]->module && xs[u]->module->rid ? xs[u]->module->rid : "unknown",
                   xs[u]->module && xs[u]->module->name ? xs[u]->module->name : "unknown");
        }
       }

       eputs (" </service>\n", (FILE*)ev->para);
      } else {
       if (cur->value) {
        struct lmodule **xs = cur->value;
        uint32_t u = 0;
        for (u = 0; xs[u]; u++) {
         eprintf ((FILE *)ev->para, (ev->status & EIPC_OUTPUT_ANSI) ?
           ((xs[u]->module && (xs[u]->module->options & EINIT_MOD_DEPRECATED)) ?
                                  " \e[31m- \e[0mcandidate \"%s\" (%s)\n" :
                                  " \e[33m* \e[0mcandidate \"%s\" (%s)\n") :
             " * candidate \"%s\" (%s)\n",
           xs[u]->module && xs[u]->module->rid ? xs[u]->module->rid : "unknown",
           xs[u]->module && xs[u]->module->name ? xs[u]->module->name : "unknown");
        }
       }
      }
     }

     cur = streenext (cur);
    }

    emutex_unlock(&ml_service_list_mutex);

    ev->flag = 1;
   }
#ifdef DEBUG
   else if (strmatch (ev->set[1], "control-blocks")) {
    emutex_lock (&ml_tb_target_state_mutex);

    if (target_state.enable) {
     char *r = set2str (' ', (const char **)target_state.enable);
     if (r) {
      eprintf ((FILE *)ev->para, "target_state.enable = { %s }\n", r);
      free (r);
     }
    }
    if (target_state.disable) {
     char *r = set2str (' ', (const char **)target_state.disable);
     if (r) {
      eprintf ((FILE *)ev->para, "target_state.disable = { %s }\n", r);
      free (r);
     }
    }
    if (target_state.critical) {
     char *r = set2str (' ', (const char **)target_state.critical);
     if (r) {
      eprintf ((FILE *)ev->para, "target_state.critical = { %s }\n", r);
      free (r);
     }
    }

    emutex_unlock (&ml_tb_target_state_mutex);
    emutex_lock (&ml_tb_current_mutex);

    if (current.enable) {
     char *r = set2str (' ', (const char **)current.enable);
     if (r) {
      eprintf ((FILE *)ev->para, "current.enable = { %s }\n", r);
      free (r);
     }
    }
    if (current.disable) {
     char *r = set2str (' ', (const char **)current.disable);
     if (r) {
      eprintf ((FILE *)ev->para, "current.disable = { %s }\n", r);
      free (r);
     }
    }
    if (current.critical) {
     char *r = set2str (' ', (const char **)current.critical);
     if (r) {
      eprintf ((FILE *)ev->para, "current.critical = { %s }\n", r);
      free (r);
     }
    }

    emutex_unlock (&ml_tb_current_mutex);

    ev->flag = 1;
   }
#endif
  }
 }
}
