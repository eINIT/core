/*
 *  module.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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

#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

struct lmodule *mlist = NULL;
extern char shutting_down;

pthread_mutex_t mlist_mutex = PTHREAD_MUTEX_INITIALIZER;

struct stree *service_usage = NULL;
pthread_mutex_t service_usage_mutex = PTHREAD_MUTEX_INITIALIZER;
int modules_work_count = 0;
time_t modules_last_change = 0;

int mod_suspend (struct lmodule *m) {
 if (m && !(m->status & status_suspended)) {
  if (m->suspend && m->do_suspend && m->resume && m->do_resume) {
   if ((m->suspend (m) == status_ok) && (m->do_suspend (m) == status_ok)) {
//    notice (3, "module suspended");
    m->status |= status_suspended;
    return status_ok;
   } else return status_failed;
  } else {
   return status_failed;
  }
 } else {
  return status_ok;
 }
}

int mod_resume (struct lmodule *m) {
 if (m && (m->status & status_suspended)) {
  if (m->resume && m->do_resume) {
   if ((m->do_resume (m) == status_ok) && (m->resume (m) == status_ok)) {
//    notice (3, "module resumed");
    m->status ^= status_suspended;
    return status_ok;
   } else return status_failed;
  } else {
   return status_failed;
  }
 } else {
  return status_ok;
 }
}

#if 0
void mod_freedesc (struct lmodule *m) {
 emutex_lock (&m->mutex);

 if (m->next != NULL)
  mod_freedesc (m->next);

 m->next = NULL;
 if (m->status & status_enabled) {
  mod (einit_module_disable | einit_module_ignore_dependencies | einit_module_ignore_mutex, m, NULL);
 }

 if (m->cleanup)
  m->cleanup (m);

// m->status |= MOD_LOCKED;

 emutex_unlock (&m->mutex);
 emutex_destroy (&m->mutex);

// if (m->sohandle)
//  dlclose (m->sohandle);

// efree (m);
}

int mod_freemodules ( void ) {
 if (mlist != NULL)
  mod_freedesc (mlist);
 mlist = NULL;
 return 1;
}
#endif

struct lmodule *mod_update (struct lmodule *module) {
 if (!module->module) return module;

 struct einit_event ee = evstaticinit (einit_core_update_module);
 ee.para = (void *)module;
 event_emit (&ee, einit_event_flag_broadcast);
 evstaticdestroy(ee);

 return module;
}

char **mod_blocked_rids = NULL;
pthread_mutex_t mod_blocked_rids_mutex = PTHREAD_MUTEX_INITIALIZER;

struct lmodule *mod_add (void *sohandle, const struct smodule *module) {
 struct lmodule *nmod;

 emutex_lock (&mod_blocked_rids_mutex);
 if (inset ((const void **)mod_blocked_rids, module->rid, SET_TYPE_STRING)) {
  emutex_unlock (&mod_blocked_rids_mutex);
  return NULL;
 }
 emutex_unlock (&mod_blocked_rids_mutex);

 emutex_lock (&mlist_mutex);
 struct lmodule *m = mlist;

 while (m) {
  if (m->module && m->module->rid && module && module->rid && strmatch (m->module->rid, module->rid)) {
   break;
  }

  m = m->next;
 }
 emutex_unlock (&mlist_mutex);

 if (m) return m;

 nmod = ecalloc (1, sizeof (struct lmodule));

 nmod->sohandle = sohandle;
 nmod->module = module;
 emutex_init (&nmod->mutex, NULL);

 if (module->si.provides || module->si.requires || module->si.after || module->si.before) {
  nmod->si = ecalloc (1, sizeof (struct service_information));

  if (module->si.provides)
   nmod->si->provides = (char **)setdup((const void **)module->si.provides, SET_TYPE_STRING);
  if (module->si.requires)
   nmod->si->requires = (char **)setdup((const void **)module->si.requires, SET_TYPE_STRING);
  if (module->si.after)
   nmod->si->after = (char **)setdup((const void **)module->si.after, SET_TYPE_STRING);
  if (module->si.before)
   nmod->si->before = (char **)setdup((const void **)module->si.before, SET_TYPE_STRING);
 } else
  nmod->si = NULL;

 if (module->configure) {
  int rv = module->configure (nmod);
  if (rv & (status_configure_done | status_configure_failed)) {
   /* module doesn't want to be loaded for real */

   if (nmod->si) {
    if (nmod->si->provides) {
     efree (nmod->si->provides);
    }
    if (nmod->si->requires) {
     efree (nmod->si->requires);
    }
    if (nmod->si->after) {
     efree (nmod->si->after);
    }
    if (nmod->si->before) {
     efree (nmod->si->before);
    }

    efree (nmod->si);
   }

   efree (nmod);

   if (rv & status_block) {
    emutex_lock (&mod_blocked_rids_mutex);
    mod_blocked_rids = (char **)setadd ((void **)mod_blocked_rids, module->rid, SET_TYPE_STRING);
    emutex_unlock (&mod_blocked_rids_mutex);
   }

   return NULL;
  }
 }
 if (nmod->scanmodules) {
  nmod->scanmodules(mlist);
 }

 emutex_lock (&mlist_mutex);
 nmod->next = mlist;
 mlist = nmod;
 emutex_unlock (&mlist_mutex);

 nmod = mod_update (nmod);

 return nmod;
}

int mod (enum einit_module_task task, struct lmodule *module, char *custom_command) {
 struct einit_event *fb;
 unsigned int ret;

 if (!module) return 0;

/* wait if the module is already being processed in a different thread */
 if ((task & einit_module_suspend) || (task & einit_module_resume)) {
  if (pthread_mutex_trylock (&module->mutex))
   return status_failed;
 } else
  emutex_lock (&module->mutex);

 if (task & einit_module_suspend) {
  int retval = mod_suspend (module);

  emutex_unlock (&module->mutex);

  return retval;
 }

 if (task & einit_module_resume) {
  int retval = mod_resume (module);

  emutex_unlock (&module->mutex);

  return retval;
 }

 if (module->status & status_suspended) {
  if (!(mod_resume (module) == status_ok)) {
   emutex_unlock (&module->mutex);

   return status_failed;
  }
 }

 if (task & einit_module_custom) {
  if (!custom_command) {
   emutex_unlock (&module->mutex);

   return status_failed;
  }

  goto skipdependencies;
 }

 if (task & einit_module_ignore_dependencies) {
  notice (2, "module: skipping dependency-checks");
  task ^= einit_module_ignore_dependencies;
  goto skipdependencies;
 }

 module->status |= status_working;

/* check if the task requested has already been done (or if it can be done at all) */
 if ((task & einit_module_enable) && (!module->enable || (module->status & status_enabled))) {
  wontload:
  module->status ^= status_working;

  {
/* service status update */
   char *nserv[] = { "no-service", NULL };

   struct einit_event ees = evstaticinit (einit_core_service_update);
   ees.task = task;
   ees.status = module->status;
   ees.string = (module->module && module->module->rid) ? module->module->rid : "??";
   ees.set = (module->si && module->si->provides) ? (void **)module->si->provides : (void **)nserv;
   ees.para = (void *)module;
   event_emit (&ees, einit_event_flag_broadcast);
   evstaticdestroy (ees);
  }

  emutex_unlock (&module->mutex);
  return status_idle;
 }
 if ((task & einit_module_disable) && (!module->disable || (module->status & status_disabled) || (module->status == status_idle)))
  goto wontload;

 if (task & einit_module_enable) {
  if (!mod_service_requirements_met(module))
   goto wontload;
 } else if (task & einit_module_disable) {
  if (!mod_service_not_in_use(module))
   goto wontload;
 }

 skipdependencies:

/* inform everyone about what's going to happen */
 {
  modules_work_count++;

/* same for services */
  if (module->si && module->si->provides) {
   int i = 0;

   if (task & (einit_module_enable | einit_module_disable))
    for (i = 0; module->si->provides[i]; i++) {
     struct einit_event eei = evstaticinit ((task & einit_module_enable) ? einit_core_service_enabling : einit_core_service_disabling);
     eei.string = module->si->provides[i];
     event_emit (&eei, einit_event_flag_broadcast);
     evstaticdestroy (eei);
    }

   struct einit_event ees = evstaticinit (einit_core_service_update);
   ees.task = task;
   ees.status = status_working;
   ees.string = (module->module && module->module->rid) ? module->module->rid : module->si->provides[0];
   ees.set = (void **)module->si->provides;
   ees.para = (void *)module;
   event_emit (&ees, einit_event_flag_broadcast);
   evstaticdestroy (ees);
  }
 }

/* actual loading bit */
 {
  fb = evinit (einit_feedback_module_status);
  fb->para = (void *)module;
  fb->task = task | einit_module_feedback_show;
  fb->status = status_working;
  fb->flag = 0;
  fb->string = NULL;
  fb->stringset = (char **)setadd ((void **)NULL, (module->module && module->module->rid) ? module->module->rid : module->si->provides[0], SET_TYPE_STRING);
  fb->integer = module->fbseq+1;
  status_update (fb);

  if (task & einit_module_custom) {
   if (strmatch (custom_command, "zap")) {
    char zerror = module->status & status_failed ? 1 : 0;
    fb->string = "module ZAP'd.";
    module->status = status_idle;
    module->status = status_disabled;
    if (zerror)
     module->status |= status_failed;
   } else if (module->custom) {
    module->status = module->custom(module->param, custom_command, fb);
   } else {
    module->status = (module->status & (status_enabled | status_disabled)) | status_failed | status_command_not_implemented;
   }
  } else if (task & einit_module_enable) {
    ret = module->enable (module->param, fb);
    if (ret & status_ok) {
     module->status = status_ok | status_enabled;
    } else {
     module->status = status_failed | status_disabled;
    }
  } else if (task & einit_module_disable) {
    ret = module->disable (module->param, fb);
    if (ret & status_ok) {
     module->status = status_ok | status_disabled;
    } else {
     module->status = status_failed | status_enabled;
    }
  }

  fb->status = module->status;
  module->fbseq = fb->integer + 1;

  status_update (fb);
//  event_emit(fb, einit_event_flag_broadcast);
//  if (fb->task & einit_module_feedback_show) fb->task ^= einit_module_feedback_show; fb->string = NULL;

/* module status update */
  {
/* service status update */
   char *nserv[] = { "no-service", NULL };

    struct einit_event ees = evstaticinit (einit_core_service_update);
    ees.task = task;
    ees.status = fb->status;
    ees.string = (module->module && module->module->rid) ? module->module->rid : "??";
    ees.set = (module->si && module->si->provides) ? (void **)module->si->provides : (void **)nserv;
    ees.para = (void *)module;
    event_emit (&ees, einit_event_flag_broadcast);
    evstaticdestroy (ees);
  }

  efree (fb->stringset);
  evdestroy (fb);

  modules_work_count--;
 }

 mod_update_usage_table(module);

 if (shutting_down) {
  if ((task & einit_module_disable) && (module->status & (status_enabled | status_failed))) {
   mod (einit_module_custom, module, "zap");
  }
 }

 return module->status;
}

void mod_update_usage_table (struct lmodule *module) {
 struct stree *ha;
 char **t;
 uint32_t i;
 struct service_usage_item *item;

 char **disabled = NULL;
 char **enabled = NULL;

 emutex_lock (&service_usage_mutex);
 modules_last_change = time(NULL);

 if (module->status & status_enabled) {
  if (module->si) {
   if ((t = module->si->requires)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], tree_find_first)) && (item = (struct service_usage_item *)ha->value)) {
      item->users = (struct lmodule **)setadd ((void **)item->users, (void *)module, SET_NOALLOC);
     }
    }
   }
   if ((t = module->si->provides)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], tree_find_first)) && (item = (struct service_usage_item *)ha->value)) {
      if (!item->provider) {
       enabled = (char **)setadd ((void **)enabled, t[i], SET_TYPE_STRING);
      }

      item->provider = (struct lmodule **)setadd ((void **)item->provider, (void *)module, SET_NOALLOC);
     } else {
      struct service_usage_item nitem;
      memset (&nitem, 0, sizeof (struct service_usage_item));
      nitem.provider = (struct lmodule **)setadd ((void **)nitem.provider, (void *)module, SET_NOALLOC);
      service_usage = streeadd (service_usage, t[i], &nitem, sizeof (struct service_usage_item), NULL);

      enabled = (char **)setadd ((void **)enabled, t[i], SET_TYPE_STRING);
     }
    }
   }
  }
 }

 emutex_unlock (&module->mutex);

/* more cleanup code */
 ha = streelinear_prepare(service_usage);
 while (ha) {
  item = (struct service_usage_item *)ha->value;

  if (!(module->status & status_enabled)) {
   char wasprovider = inset ((const void **)item->provider, module, SET_NOALLOC);

   item->provider = (struct lmodule **)setdel ((void **)item->provider, (void *)module);
   item->users = (struct lmodule **)setdel ((void **)item->users, (void *)module);

   if (wasprovider && !item->provider) {
    disabled = (char **)setadd ((void **)disabled, ha->key, SET_TYPE_STRING);
   }
  }

#if 0
  if (!item->provider && !item->users) {
   service_usage = streedel (ha);
   if (!service_usage) break;

   ha = streelinear_prepare(service_usage);
  } else
#endif
  ha = streenext (ha);
 }

 emutex_unlock (&service_usage_mutex);

 if (enabled) {
  struct einit_event eei = evstaticinit (einit_core_service_enabled);

  for (i = 0; enabled[i]; i++) {
   eei.string = enabled[i];
   event_emit (&eei, einit_event_flag_broadcast);
  }

  evstaticdestroy (eei);
  efree (enabled);
 }

 if (disabled) {
  struct einit_event eei = evstaticinit (einit_core_service_disabled);

  for (i = 0; disabled[i]; i++) {
   eei.string = disabled[i];
   event_emit (&eei, einit_event_flag_broadcast);
  }

  evstaticdestroy (eei);
  efree (disabled);
 }
}

char mod_service_is_provided (char *service) {
 struct stree *ha;
 struct service_usage_item *item;
 char rv = 0;

 emutex_lock (&service_usage_mutex);

 if (service_usage && (ha = streefind (service_usage, service, tree_find_first)) && (item = (struct service_usage_item *)ha->value) && (item->provider))
  rv = 1;

 emutex_unlock (&service_usage_mutex);

 return rv;
}

char mod_service_is_in_use (char *service) {
 struct stree *ha;
 struct service_usage_item *item;
 char rv = 0;

 emutex_lock (&service_usage_mutex);

 if (service_usage && (ha = streefind (service_usage, service, tree_find_first)) && (item = (struct service_usage_item *)ha->value) && (item->users))
  rv = 1;

 emutex_unlock (&service_usage_mutex);

 return rv;
}

char mod_service_requirements_met (struct lmodule *module) {
 char ret = 1;
 struct stree *ha;
 uint32_t i;
 char **t;

 emutex_lock (&service_usage_mutex);

 if (module->si && (t = module->si->requires)) {
  for (i = 0; t[i]; i++) {
   if (!service_usage || !(ha = streefind (service_usage, t[i], tree_find_first)) ||
       !((struct service_usage_item *)(ha->value))->provider) {
    ret = 0;
    break;
   }
  }
 }

 emutex_unlock (&service_usage_mutex);

 return ret;
}

char mod_service_not_in_use(struct lmodule *module) {
 char ret = 1;
 struct stree *ha;

 emutex_lock (&service_usage_mutex);

 ha = streelinear_prepare(service_usage);

/* a service is "in use" if
  * it's the provider for something, and
  * it's the only provider for that for that specific service, and
  * all of the users of that service use the */

  while (ha) {
   if (((struct service_usage_item *)(ha->value))->users &&
       (((struct service_usage_item *)(ha->value))->provider) &&
       ((((struct service_usage_item *)(ha->value))->provider)[0]) &&
       (!((((struct service_usage_item *)(ha->value))->provider)[1])) &&
       inset ((const void **)(((struct service_usage_item *)(ha->value))->provider), module, -1)) {

/* this one might be a culprit */
    uint32_t i = 0, r = 0;

    for (; (((struct service_usage_item *)(ha->value))->users)[i]; i++) {
     if ((((struct service_usage_item *)(ha->value))->users)[i]->si &&
         (((struct service_usage_item *)(ha->value))->users)[i]->si->requires) {

      if (inset ((const void **)((((struct service_usage_item *)(ha->value))->users)[i]->si->requires), ha->key, SET_TYPE_STRING)) {
       r++;
      }

     }
    }

/* yep, really is in use */
    if (i == r) {
     ret = 0;
     break;
    }
   }
   ha = streenext (ha);
  }


 emutex_unlock (&service_usage_mutex);

 return ret;
}

char **mod_list_all_provided_services () {
 emutex_lock (&service_usage_mutex);

 struct stree *ha = streelinear_prepare(service_usage);
 char **ret = NULL;
 struct service_usage_item *item;

 while (ha) {
  item = ha->value;
  if (item->provider)
   ret = (char **)setadd ((void **)ret, ha->key, SET_TYPE_STRING);

  ha = streenext(ha);
 }

 emutex_unlock (&service_usage_mutex);

 return ret;
}

char **service_usage_query_cr (enum einit_usage_query task, const struct lmodule *module, const char *service) {
 emutex_lock (&service_usage_mutex);

 struct stree *ha = streelinear_prepare(service_usage);
 char **ret = NULL;
 uint32_t i;

 if (task & service_get_services_that_use) {
  if (module) {
   while (ha) {
    if (((struct service_usage_item *)(ha->value))->users &&
        (((struct service_usage_item *)(ha->value))->provider) &&
        ((((struct service_usage_item *)(ha->value))->provider)[0]) &&
        (!((((struct service_usage_item *)(ha->value))->provider)[1])) &&
        inset ((const void **)(((struct service_usage_item*)ha->value)->provider), module, -1)) {

     for (i = 0; ((struct service_usage_item *)(ha->value))->users[i]; i++) {
      if (((struct service_usage_item *)(ha->value))->users[i]->si &&
          ((struct service_usage_item *)(ha->value))->users[i]->si->provides)

       ret = (char **)setcombine ((const void **)ret, (const void **)((struct service_usage_item *)(ha->value))->users[i]->si->provides, SET_TYPE_STRING);

     }
    }
    ha = streenext (ha);
   }
  }
 } else if (task & service_get_services_used_by) {
  if (module) {
   while (ha) {
    if (inset ((const void **)(((struct service_usage_item*)ha->value)->users), module, -1)) {
     ret = (char **)setadd ((void **)ret, (void *)ha->key, SET_TYPE_STRING);
    }
    ha = streenext (ha);
   }
  }
 }

 emutex_unlock (&service_usage_mutex);
 return ret;
}

struct lmodule **mod_get_all_users (struct lmodule *module) {
 struct lmodule **ret = NULL;

 emutex_lock (&service_usage_mutex);
 struct stree *ha = streelinear_prepare(service_usage);

 while (ha) {
  struct service_usage_item *item = ha->value;

  if (item->provider && item->users && inset ((const void **)item->provider, module, SET_NOALLOC)) {
   int i = 0;
   for (; item->users[i]; i++) {
    ret = (struct lmodule **)setadd ((void **)ret, item->users[i], SET_NOALLOC);
   }
  }
  ha = streenext (ha);
 }
 emutex_unlock (&service_usage_mutex);

 return ret;
}

struct lmodule **mod_get_all_providers (char *service) {
 struct lmodule **ret = NULL;

 emutex_lock (&service_usage_mutex);
 struct stree *ha = streefind (service_usage, service, tree_find_first);

 if (ha) {
  struct service_usage_item *item = ha->value;

  if (item->provider) {
   int i = 0;
   for (; item->provider[i]; i++) {
    ret = (struct lmodule **)setadd ((void **)ret, item->provider[i], SET_NOALLOC);
   }
  }
 }
 emutex_unlock (&service_usage_mutex);

 return ret;
}
