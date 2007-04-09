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
 if (m->status & STATUS_ENABLED) {
  emutex_unlock (&m->imutex);
  mod (MOD_DISABLE | MOD_IGNORE_DEPENDENCIES | MOD_NOMUTEX, m, NULL);
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

 struct einit_event ee = evstaticinit (EVE_UPDATE_MODULE);
 ee.para = (void *)module;
 event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
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

int mod (unsigned int task, struct lmodule *module, char *custom_command) {
 struct einit_event *fb;
 unsigned int ret;

 if (!module) return 0;

/* wait if the module is already being processed in a different thread */
 if (!(task & MOD_NOMUTEX))
  emutex_lock (&module->mutex);

 if (task & MOD_CUSTOM) {
  if (!custom_command) return STATUS_FAIL;

  goto skipdependencies;
 }

 if (task & MOD_IGNORE_DEPENDENCIES) {
  notice (2, "module: skipping dependency-checks");
  task ^= MOD_IGNORE_DEPENDENCIES;
  goto skipdependencies;
 }

 if (module->status & MOD_LOCKED) { // this means the module is locked. maybe we're shutting down just now.
  if (!(task & MOD_NOMUTEX))
   emutex_unlock (&module->mutex);

  if (task & MOD_ENABLE)
   return STATUS_FAIL;
  else if (task & MOD_DISABLE)
   return STATUS_OK;
  else
   return STATUS_OK;
 }

 module->status |= STATUS_WORKING;

/* check if the task requested has already been done (or if it can be done at all) */
 if ((task & MOD_ENABLE) && (!module->enable || (module->status & STATUS_ENABLED))) {
  wontload:
  module->status ^= STATUS_WORKING;
  if (!(task & MOD_NOMUTEX))
   emutex_unlock (&module->mutex);
  return STATUS_IDLE;
 }
 if ((task & MOD_DISABLE) && (!module->disable || (module->status & STATUS_DISABLED) || (module->status == STATUS_IDLE)))
  goto wontload;

 if (task & MOD_ENABLE) {
  if (!service_usage_query(SERVICE_REQUIREMENTS_MET, module, NULL))
   goto wontload;
 } else if (task & MOD_DISABLE) {
  if (!service_usage_query(SERVICE_NOT_IN_USE, module, NULL))
   goto wontload;
 }

 skipdependencies:

/* inform everyone about what's going to happen */
 {
  struct einit_event eem = evstaticinit (EVE_MODULE_UPDATE);
  eem.task = task;
  eem.status = STATUS_WORKING;
  eem.para = (void *)module;
  event_emit (&eem, EINIT_EVENT_FLAG_BROADCAST);
  evstaticdestroy (eem);

/* same for services */
  if (module->si && module->si->provides) {
   struct einit_event ees = evstaticinit (EVE_SERVICE_UPDATE);
   ees.task = task;
   ees.status = STATUS_WORKING;
   ees.string = (module->module && module->module->rid) ? module->module->rid : module->si->provides[0];
   ees.set = (void **)module->si->provides;
   ees.para = (void *)module;
   event_emit (&ees, EINIT_EVENT_FLAG_BROADCAST);
   evstaticdestroy (ees);
  }
 }

/* actual loading bit */
 {
  fb = evinit (EVE_FEEDBACK_MODULE_STATUS);
  fb->para = (void *)module;
  fb->task = task | MOD_FEEDBACK_SHOW;
  fb->status = STATUS_WORKING;
  fb->flag = 0;
  fb->string = NULL;
  fb->integer = module->fbseq+1;
  status_update (fb);

  if (task & MOD_CUSTOM) {
   if (module->custom) {
    module->status = module->custom(module->param, custom_command, fb);
   } else if (strmatch (custom_command, "zap")) {
    module->status = STATUS_IDLE;
    fb->status = STATUS_OK | STATUS_IDLE;
   } else {
    fb->status = STATUS_FAIL | STATUS_COMMAND_NOT_IMPLEMENTED;
   }
  } else if (task & MOD_ENABLE) {
    ret = module->enable (module->param, fb);
    if (ret & STATUS_OK) {
     module->status = STATUS_ENABLED;
     fb->status = STATUS_OK | STATUS_ENABLED;
    } else {
     fb->status = STATUS_FAIL;
    }
  } else if (task & MOD_DISABLE) {
    ret = module->disable (module->param, fb);
    if (ret & STATUS_OK) {
     module->status = STATUS_DISABLED;
     fb->status = STATUS_OK | STATUS_DISABLED;
    } else {
     fb->status = STATUS_FAIL;
    }
  }

  module->fbseq = fb->integer + 1;

  status_update (fb);
//  event_emit(fb, EINIT_EVENT_FLAG_BROADCAST);
//  if (fb->task & MOD_FEEDBACK_SHOW) fb->task ^= MOD_FEEDBACK_SHOW; fb->string = NULL;

  module->lastfb = fb->status;

/* module status update */
  if (module) {
   struct einit_event eem = evstaticinit (EVE_MODULE_UPDATE);
   eem.task = task;
   eem.status = fb->status;
   eem.para = (void *)module;
   event_emit (&eem, EINIT_EVENT_FLAG_BROADCAST);
   evstaticdestroy (eem);

/* service status update */
   if (module->si && module->si->provides) {
    struct einit_event ees = evstaticinit (EVE_SERVICE_UPDATE);
    ees.task = task;
    ees.status = fb->status;
    ees.string = (module->module && module->module->rid) ? module->module->rid : module->si->provides[0];
    ees.set = (void **)module->si->provides;
    ees.para = (void *)module;
    event_emit (&ees, EINIT_EVENT_FLAG_BROADCAST);
    evstaticdestroy (ees);
   }
  }

  evdestroy (fb);

  service_usage_query(SERVICE_UPDATE, module, NULL);

  if (!(task & MOD_NOMUTEX))
   emutex_unlock (&module->mutex);

 }
 return module->status;
}

uint16_t service_usage_query (const uint16_t task, const struct lmodule *module, const char *service) {
 uint16_t ret = 0;
 struct stree *ha;
 char **t;
 uint32_t i;
 struct service_usage_item *item;

 if ((!module || !module->module) && !service) return 0;

 emutex_lock (&service_usage_mutex);

 if (task & SERVICE_NOT_IN_USE) {
  ret |= SERVICE_NOT_IN_USE;
  struct stree *ha = service_usage;

  while (ha) {
   if (((struct service_usage_item *)(ha->value))->users &&
       inset ((const void **)(((struct service_usage_item *)(ha->value))->provider), module, -1)) {

    ret ^= SERVICE_NOT_IN_USE;
    break;
   }
   ha = streenext (ha);
  }
 } else if (task & SERVICE_REQUIREMENTS_MET) {
  ret |= SERVICE_REQUIREMENTS_MET;
  if (module->si && (t = module->si->requires)) {
   for (i = 0; t[i]; i++) {
    if (!service_usage || !(ha = streefind (service_usage, t[i], TREE_FIND_FIRST)) ||
        !((struct service_usage_item *)(ha->value))->provider) {
     ret ^= SERVICE_REQUIREMENTS_MET;
     break;
    }
   }
  }
 } else if (task & SERVICE_UPDATE) {
  if (module->status & STATUS_ENABLED) {
   if (module->si && (t = module->si->requires)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], TREE_FIND_FIRST)) && (item = (struct service_usage_item *)ha->value)) {
      item->users = (struct lmodule **)setadd ((void **)item->users, (void *)module, SET_NOALLOC);
     }
    }
   }
   if (module->si && (t = module->si->provides)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], TREE_FIND_FIRST)) && (item = (struct service_usage_item *)ha->value)) {
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

   if (!(module->status & STATUS_ENABLED)) {
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
 } else if (task & SERVICE_IS_REQUIRED) {
  if (service_usage && (ha = streefind (service_usage, service, TREE_FIND_FIRST)) && (item = (struct service_usage_item *)ha->value) && (item->users))
   ret |= SERVICE_IS_REQUIRED;
 } else if (task & SERVICE_IS_PROVIDED) {
  if (service_usage && (ha = streefind (service_usage, service, TREE_FIND_FIRST)) && (item = (struct service_usage_item *)ha->value) && (item->provider))
   ret |= SERVICE_IS_PROVIDED;
 }

 emutex_unlock (&service_usage_mutex);
 return ret;
}

uint16_t service_usage_query_group (const uint16_t task, const struct lmodule *module, const char *service) {
 uint16_t ret = 0;
 struct stree *ha;

 if (!service) return 0;

 emutex_lock (&service_usage_mutex);
 if (task & SERVICE_ADD_GROUP_PROVIDER) {
  if (!module || !module->module) {
   emutex_unlock (&service_usage_mutex);

   return 0;
  }

  if (!service_usage || !(ha = streefind (service_usage, service, TREE_FIND_FIRST))) {
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
 if (task & SERVICE_SET_GROUP_PROVIDERS) {
  if (!service_usage || !(ha = streefind (service_usage, service, TREE_FIND_FIRST))) {
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

char **service_usage_query_cr (const uint16_t task, const struct lmodule *module, const char *service) {
 emutex_lock (&service_usage_mutex);

 struct stree *ha = service_usage;
 char **ret = NULL;
 uint32_t i;

 if (task & SERVICE_GET_ALL_PROVIDED) {
  while (ha) {
   ret = (char **)setadd ((void **)ret, (void *)ha->key, SET_TYPE_STRING);
   ha = streenext (ha);
  }
 } else if (task & SERVICE_GET_SERVICES_THAT_USE) {
  if (module) {
   while (ha) {
    if (((struct service_usage_item *)(ha->value))->users &&
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
 } else if (task & SERVICE_GET_SERVICES_USED_BY) {
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
