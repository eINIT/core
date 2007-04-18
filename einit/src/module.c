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

struct lmodule *mlist = NULL;

pthread_mutex_t mlist_mutex = PTHREAD_MUTEX_INITIALIZER;

struct stree *service_usage = NULL;
pthread_mutex_t service_usage_mutex = PTHREAD_MUTEX_INITIALIZER;

void mod_freedesc (struct lmodule *m) {
 emutex_lock (&m->mutex);
 emutex_lock (&m->imutex);

 if (m->next != NULL)
  mod_freedesc (m->next);

 m->next = NULL;
 if (m->status & status_enabled) {
  emutex_unlock (&m->imutex);
  mod (einit_module_disable | einit_module_ignore_dependencies | einit_module_ignore_mutex, m, NULL);
  emutex_lock (&m->imutex);
 }

 if (m->cleanup)
  m->cleanup (m);

 m->status |= MOD_LOCKED;

 emutex_unlock (&m->mutex);
 emutex_destroy (&m->mutex);
 emutex_unlock (&m->imutex);
 emutex_destroy (&m->imutex);

// if (m->sohandle)
//  dlclose (m->sohandle);

// free (m);
}

int mod_freemodules ( void ) {
 if (mlist != NULL)
  mod_freedesc (mlist);
 mlist = NULL;
 return 1;
}

struct lmodule *mod_update (struct lmodule *module) {
 if (!module->module) return module;

 if (pthread_mutex_trylock (&module->mutex)) {
  perror ("mod_update(): locking mutex");
  return module;
 }

 struct einit_event ee = evstaticinit (einit_core_update_module);
 ee.para = (void *)module;
 event_emit (&ee, einit_event_flag_broadcast);
 evstaticdestroy(ee);

 emutex_unlock (&module->mutex);

 return module;
}

struct lmodule *mod_add (void *sohandle, const struct smodule *module) {
 struct lmodule *nmod;

 nmod = ecalloc (1, sizeof (struct lmodule));

 emutex_lock (&mlist_mutex);
 nmod->next = mlist;
 mlist = nmod;
 emutex_unlock (&mlist_mutex);

 nmod->sohandle = sohandle;
 nmod->module = module;
 emutex_init (&nmod->mutex, NULL);
 emutex_init (&nmod->imutex, NULL);

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


 if (module->configure) module->configure (nmod);
 if (nmod->scanmodules) nmod->scanmodules(mlist);

 nmod = mod_update (nmod);

 return nmod;
}

int mod (enum einit_module_task task, struct lmodule *module, char *custom_command) {
 struct einit_event *fb;
 unsigned int ret;

 if (!module) return 0;

/* wait if the module is already being processed in a different thread */
 if (!(task & einit_module_ignore_mutex))
  emutex_lock (&module->mutex);

 if (task & einit_module_custom) {
  if (!custom_command) return status_failed;

  goto skipdependencies;
 }

 if (task & einit_module_ignore_dependencies) {
  notice (2, "module: skipping dependency-checks");
  task ^= einit_module_ignore_dependencies;
  goto skipdependencies;
 }

 if (module->status & MOD_LOCKED) { // this means the module is locked. maybe we're shutting down just now.
  if (!(task & einit_module_ignore_mutex))
   emutex_unlock (&module->mutex);

  if (task & einit_module_enable)
   return status_failed;
  else if (task & einit_module_disable)
   return status_ok;
  else
   return status_ok;
 }

 module->status |= status_working;

/* check if the task requested has already been done (or if it can be done at all) */
 if ((task & einit_module_enable) && (!module->enable || (module->status & status_enabled))) {
  wontload:
  module->status ^= status_working;
  if (!(task & einit_module_ignore_mutex))
   emutex_unlock (&module->mutex);
  return status_idle;
 }
 if ((task & einit_module_disable) && (!module->disable || (module->status & status_disabled) || (module->status == status_idle)))
  goto wontload;

 if (task & einit_module_enable) {
  if (!service_usage_query(service_requirements_met, module, NULL))
   goto wontload;
 } else if (task & einit_module_disable) {
  if (!service_usage_query(service_not_in_use, module, NULL))
   goto wontload;
 }

 skipdependencies:

/* inform everyone about what's going to happen */
 {
  struct einit_event eem = evstaticinit (einit_core_module_update);
  eem.task = task;
  eem.status = status_working;
  eem.para = (void *)module;
  event_emit (&eem, einit_event_flag_broadcast);
  evstaticdestroy (eem);

/* same for services */
  if (module->si && module->si->provides) {
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
  fb->integer = module->fbseq+1;
  status_update (fb);

  if (task & einit_module_custom) {
   if (strmatch (custom_command, "zap")) {
    fb->string = "module ZAP'd.";
    module->status = status_idle;
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
  if (module) {
   struct einit_event eem = evstaticinit (einit_core_module_update);
   eem.task = task;
   eem.status = fb->status;
   eem.para = (void *)module;
   event_emit (&eem, einit_event_flag_broadcast);
   evstaticdestroy (eem);

/* service status update */
   if (module->si && module->si->provides) {
    struct einit_event ees = evstaticinit (einit_core_service_update);
    ees.task = task;
    ees.status = fb->status;
    ees.string = (module->module && module->module->rid) ? module->module->rid : module->si->provides[0];
    ees.set = (void **)module->si->provides;
    ees.para = (void *)module;
    event_emit (&ees, einit_event_flag_broadcast);
    evstaticdestroy (ees);
   }
  }

  evdestroy (fb);

  service_usage_query(service_update, module, NULL);

  if (!(task & einit_module_ignore_mutex))
   emutex_unlock (&module->mutex);

 }
 return module->status;
}

uint16_t service_usage_query (enum einit_usage_query task, const struct lmodule *module, const char *service) {
 uint16_t ret = 0;
 struct stree *ha;
 char **t;
 uint32_t i;
 struct service_usage_item *item;

 if ((!module || !module->module) && !service) return 0;

 emutex_lock (&service_usage_mutex);

 if (task & service_not_in_use) {
  ret |= service_not_in_use;
  struct stree *ha = service_usage;

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
     ret ^= service_not_in_use;
     break;
    }
   }
   ha = streenext (ha);
  }
 } else if (task & service_requirements_met) {
  ret |= service_requirements_met;
  if (module->si && (t = module->si->requires)) {
   for (i = 0; t[i]; i++) {
    if (!service_usage || !(ha = streefind (service_usage, t[i], tree_find_first)) ||
        !((struct service_usage_item *)(ha->value))->provider) {
     ret ^= service_requirements_met;
     break;
    }
   }
  }
 } else if (task & service_update) {
  if (module->status & status_enabled) {
   if (module->si && (t = module->si->requires)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], tree_find_first)) && (item = (struct service_usage_item *)ha->value)) {
      item->users = (struct lmodule **)setadd ((void **)item->users, (void *)module, SET_NOALLOC);
     }
    }
   }
   if (module->si && (t = module->si->provides)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], tree_find_first)) && (item = (struct service_usage_item *)ha->value)) {
      item->provider = (struct lmodule **)setadd ((void **)item->provider, (void *)module, SET_NOALLOC);
     } else {
      struct service_usage_item nitem;
      memset (&nitem, 0, sizeof (struct service_usage_item));
      nitem.provider = (struct lmodule **)setadd ((void **)nitem.provider, (void *)module, SET_NOALLOC);
      service_usage = streeadd (service_usage, t[i], &nitem, sizeof (struct service_usage_item), NULL);
     }
    }
   }
  }

/* more cleanup code */
  ha = service_usage;
  while (ha) {
   item = (struct service_usage_item *)ha->value;

   if (!(module->status & status_enabled)) {
     item->provider = (struct lmodule **)setdel ((void **)item->provider, (void *)module);
     item->users = (struct lmodule **)setdel ((void **)item->users, (void *)module);
   }

   if (!item->provider && !item->users) {
//    service_usage = streedel (service_usage, ha);
    service_usage = streedel (ha);
    ha = service_usage;
   } else
    ha = streenext (ha);
  }
 } else if (task & service_is_required) {
  if (service_usage && (ha = streefind (service_usage, service, tree_find_first)) && (item = (struct service_usage_item *)ha->value) && (item->users))
   ret |= service_is_required;
 } else if (task & service_is_provided) {
  if (service_usage && (ha = streefind (service_usage, service, tree_find_first)) && (item = (struct service_usage_item *)ha->value) && (item->provider))
   ret |= service_is_provided;
 }

 emutex_unlock (&service_usage_mutex);
 return ret;
}

uint16_t service_usage_query_group (enum einit_usage_query task, const struct lmodule *module, const char *service) {
 uint16_t ret = 0;
 struct stree *ha;

 if (!service) return 0;

 emutex_lock (&service_usage_mutex);
 if (task & service_add_group_provider) {
  if (!module || !module->module) {
   emutex_unlock (&service_usage_mutex);

   return 0;
  }

  if (!service_usage || !(ha = streefind (service_usage, service, tree_find_first))) {
   struct service_usage_item nitem;
   memset (&nitem, 0, sizeof (struct service_usage_item));
   nitem.provider = (struct lmodule **)setadd ((void **)nitem.provider, (void *)module, SET_NOALLOC);
   service_usage = streeadd (service_usage, service, &nitem, sizeof (struct service_usage_item), NULL);
  } else {
   struct service_usage_item *citem = (struct service_usage_item *)ha->value;

   if (citem) {
    if (!inset ((const void **)citem->provider, (void *)module, SET_NOALLOC)) {
     citem->provider = (struct lmodule **)setadd ((void **)citem->provider, (void *)module, SET_NOALLOC);
    }
   }
  }
 }
 if (task & service_set_group_providers) {
  if (!service_usage || !(ha = streefind (service_usage, service, tree_find_first))) {
   struct service_usage_item nitem;
   memset (&nitem, 0, sizeof (struct service_usage_item));
   nitem.provider = (struct lmodule **)setdup ((const void **)module, SET_NOALLOC);
   service_usage = streeadd (service_usage, service, &nitem, sizeof (struct service_usage_item), NULL);
  } else {
   struct service_usage_item *citem = (struct service_usage_item *)ha->value;

   if (citem) {
    free (citem->provider);
    citem->provider = (struct lmodule **)setdup ((const void **)module, SET_NOALLOC);
   }
  }
 }

 emutex_unlock (&service_usage_mutex);
 return ret;
}

char **service_usage_query_cr (enum einit_usage_query task, const struct lmodule *module, const char *service) {
 emutex_lock (&service_usage_mutex);

 struct stree *ha = service_usage;
 char **ret = NULL;
 uint32_t i;

 if (task & service_is_provided) {
  while (ha) {
   ret = (char **)setadd ((void **)ret, (void *)ha->key, SET_TYPE_STRING);
   ha = streenext (ha);
  }
 } else if (task & service_get_services_that_use) {
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
