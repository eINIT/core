/*
 *  module.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <pthread.h>

struct lmodule *mlist = NULL;
int mcount = 0;

char **provided = NULL;
char **required = NULL;
pthread_mutex_t mlist_mutex = PTHREAD_MUTEX_INITIALIZER;

struct uhash *service_usage = NULL;
pthread_mutex_t service_usage_mutex = PTHREAD_MUTEX_INITIALIZER;

int mod_scanmodules () {
 DIR *dir;
 struct dirent *entry;
 char *tmp;
 int mplen;
 void *sohandle;
 struct lmodule *cmod = NULL, *nmod;

 event_listen (EINIT_EVENT_TYPE_IPC, mod_event_handler);

 char *modulepath = cfg_getpath ("module-path");
 if (!modulepath) return -1;

 mplen = strlen (modulepath) +1;
 dir = opendir (modulepath);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
   if (entry->d_name[0] == '.') continue;
   tmp = (char *)emalloc ((mplen + strlen (entry->d_name))*sizeof (char));
   struct stat sbuf;
   struct smodule *modinfo;
   *tmp = 0;
   strcat (tmp, modulepath);
   strcat (tmp, entry->d_name);
   dlerror ();
   if (stat (tmp, &sbuf) || !S_ISREG (sbuf.st_mode)) {
    free (tmp);
    continue;
   }

   sohandle = dlopen (tmp, RTLD_NOW);
   if (sohandle == NULL) {
    puts (dlerror ());
    free (tmp);
    continue;
   }
   modinfo = (struct smodule *)dlsym (sohandle, "self");
   if (modinfo != NULL)
    mod_add (sohandle, NULL, NULL, NULL, modinfo);
   else
    dlclose (sohandle);

   free (tmp);
  }
  closedir (dir);
 } else {
  fputs ("couldn't open module directory\n", stderr);
  return bitch(BTCH_ERRNO);
 }
 return 1;
}

void mod_freedesc (struct lmodule *m) {
 pthread_mutex_lock (&m->mutex);
 pthread_mutex_lock (&m->imutex);

 if (m->next != NULL)
  mod_freedesc (m->next);
 if (m->cleanup)
  m->cleanup (m);
 if (m->sohandle)
  dlclose (m->sohandle);

 m->status |= MOD_LOCKED;

 pthread_mutex_unlock (&m->mutex);
 pthread_mutex_destroy (&m->mutex);
 pthread_mutex_unlock (&m->imutex);
 pthread_mutex_destroy (&m->imutex);
 free (m);
}

int mod_freemodules () {
 if (mlist != NULL)
  mod_freedesc (mlist);
 mlist = NULL;
 mcount = 0;
 return 1;
}

struct lmodule *mod_add (void *sohandle, int (*enable)(void *, struct einit_event *), int (*disable)(void *, struct einit_event *), void *param, struct smodule *module) {
 struct lmodule *nmod, *cur;
 int (*scanfunc)(struct lmodule *);
 int (*ftload)  (void *, struct einit_event *);
 int (*configfunc)(struct lmodule *);

 nmod = ecalloc (1, sizeof (struct lmodule));

 pthread_mutex_lock (&mlist_mutex);
 nmod->next = mlist;
 mlist = nmod;
 mcount++;
 pthread_mutex_unlock (&mlist_mutex);

 nmod->sohandle = sohandle;
 nmod->module = module;
 nmod->param = param;
 nmod->enable = enable;
 nmod->disable = disable;
 pthread_mutex_init (&nmod->mutex, NULL);
 pthread_mutex_init (&nmod->imutex, NULL);

// this will do additional initialisation functions for certain module-types
 if (module && sohandle) {
// look for and execute any configure() functions in modules
  configfunc = (int (*)(struct lmodule *)) dlsym (sohandle, "configure");
  if (configfunc != NULL) {
   configfunc (nmod);
  }
// look for any cleanup() functions
  configfunc = (int (*)(struct lmodule *)) dlsym (sohandle, "cleanup");
  if (configfunc != NULL) {
   nmod->cleanup = configfunc;
  }
// EINIT_MOD_LOADER modules will usually want to provide a function to scan
//  for modules so they can be included in the dependency chain
  if (module->mode & EINIT_MOD_LOADER) {
   scanfunc = (int (*)(struct lmodule *)) dlsym (sohandle, "scanmodules");
   if (scanfunc != NULL) {
    scanfunc (mlist);
   }
   else bitch(BTCH_ERRNO + BTCH_DL);
  }
// we need to scan for load and unload functions if NULL was supplied for these
  if (enable == NULL) {
   ftload = (int (*)(void *, struct einit_event *)) dlsym (sohandle, "enable");
   if (ftload != NULL) {
    nmod->enable = ftload;
   }
  }
  if (disable == NULL) {
   ftload = (int (*)(void *, struct einit_event *)) dlsym (sohandle, "disable");
   if (ftload != NULL) {
    nmod->disable = ftload;
   }
  }
  ftload = (int (*)(void *, struct einit_event *)) dlsym (sohandle, "reset");
  if (ftload != NULL) {
   nmod->reset = ftload;
  }
  ftload = (int (*)(void *, struct einit_event *)) dlsym (sohandle, "reload");
  if (ftload != NULL) {
   nmod->reload = ftload;
  }
 }

 return nmod;
}

struct lmodule *mod_find (char *rid, unsigned int modeflags) {
 struct lmodule *cur = mlist;
 if (mlist == NULL)
  return NULL;

 if (rid) {
  while (!cur->module || !cur->module->rid ||
    (modeflags && (cur->module->mode ^ modeflags)) ||
    strcmp(rid, cur->module->rid)) {
   if (!cur->next) return NULL;
   cur = cur->next;
  }
 } else {
  while (!cur->module ||
    (modeflags && (cur->module->mode ^ modeflags))) {
   if (!cur->next) return NULL;
   cur = cur->next;
  }
 }

 return cur;
}

int mod (unsigned int task, struct lmodule *module) {
 struct einit_event *fb;
 char providefeedback;
 struct smodule *t;
 int ti, errc;
 unsigned int ret;
 struct uhash *ha;
 if (!module) return 0;
/* wait if the module is already being processed in a different thread */
 if (errc = pthread_mutex_lock (&module->mutex)) {
// this is bad...
  puts (strerror (errc));
 }

 if (module->status & MOD_LOCKED) { // this means the module is locked. maybe we're shutting down just now.
  pthread_mutex_unlock (&module->mutex);
  if (task & MOD_ENABLE)
   return STATUS_FAIL;
  else if (task & MOD_DISABLE)
   return STATUS_OK;
  else
   return STATUS_OK;
 }

 module->status = module->status | STATUS_WORKING;

/* check if the task requested has already been done (or if it can be done at all) */
 if ((task & MOD_ENABLE) && (!module->enable || (module->status & STATUS_ENABLED))) {
  wontload:
  printf ("refusing to change state of %s\n", module->module->rid);
  pthread_mutex_unlock (&module->mutex);
  return STATUS_IDLE;
 }
 if ((task & MOD_DISABLE) && (!module->disable || (module->status & STATUS_DISABLED)))
  goto wontload;
 if ((task & MOD_RELOAD) && (module->status & STATUS_DISABLED))
  goto wontload;

#if 0
 if (task & MOD_ENABLE) {
   if (t = module->module) {
    if (t->requires) for (ti = 0; t->requires[ti]; ti++) {
      pthread_mutex_unlock (&module->mutex);
      return STATUS_FAIL_REQ;
     }*/
     if (!inset ((void **)provided, (void *)t->requires[ti], SET_TYPE_STRING)) {
      pthread_mutex_unlock (&module->mutex);
      return STATUS_FAIL_REQ;
     }
    }
   }
 } else if (task & MOD_DISABLE) {
   if (t = module->module) {
    if (t->provides) for (ti = 0; t->provides[ti]; ti++)
      pthread_mutex_unlock (&module->mutex);
      return STATUS_FAIL_REQ;*/
     if (inset ((void **)required, (void *)t->provides[ti], SET_TYPE_STRING)) {
      pthread_mutex_unlock (&module->mutex);
      return STATUS_FAIL_REQ;
     }
   }
 }
#else
 if (task & MOD_ENABLE) {
  if (!service_usage_query(SERVICE_REQUIREMENTS_MET, module, NULL))
   goto wontload;
 } else if (task & MOD_DISABLE) {
  if (!service_usage_query(SERVICE_NOT_IN_USE, module, NULL))
   goto wontload;
 }
#endif

// fb = (struct einit_event *)ecalloc (1, sizeof (struct einit_event));
 fb = evinit (EINIT_EVENT_TYPE_FEEDBACK);
 fb->para = (void *)module;
 fb->task = task | MOD_FEEDBACK_SHOW;
 fb->status = STATUS_WORKING;
 fb->flag = 0;
 fb->string = NULL;
 status_update (fb);

 if (task & MOD_ENABLE) {
   ret = module->enable (module->param, fb);
   if (ret & STATUS_OK) {
    if (t = module->module) {
     if (t->provides)
      provided = (char **)setcombine ((void **)provided, (void **)t->provides, -1);
     if (t->requires)
      required = (char **)setcombine ((void **)required, (void **)t->requires, -1);
    }
    module->status = STATUS_ENABLED;
    fb->status = STATUS_OK | STATUS_ENABLED;
   } else {
    fb->status = STATUS_FAIL;
   }
 } else if (task & MOD_DISABLE) {
   ret = module->disable (module->param, fb);
   if (ret & STATUS_OK) {
    if (t = module->module) {
     if (t->provides) for (ti = 0; provided && t->provides[ti]; ti++)
      provided = strsetdel (provided, t->provides[ti]);
     if (t->requires) for (ti = 0; required && t->requires[ti]; ti++)
      required = strsetdel (required, t->requires[ti]);
    }
    module->status = STATUS_DISABLED;
    fb->status = STATUS_OK | STATUS_DISABLED;
   } else {
    fb->status = STATUS_FAIL;
   }
 } else if (task & MOD_RESET) {
  if (module->reset) {
   ret = module->reset (module->param, fb);
   if (ret & STATUS_OK) {
    fb->status = STATUS_OK | module->status;
   } else
    fb->status = STATUS_FAIL;
  } else if (module->disable && module->enable) {
   ret = module->disable (module->param, fb);
   if (ret & STATUS_OK) {
    ret = module->enable (module->param, fb);
   } else
    fb->status = STATUS_FAIL;
  }
 } else if (task & MOD_RESET) {
  ret = module->reload (module->param, fb);
  if (ret & STATUS_OK) {
   fb->status = STATUS_OK | module->status;
  } else
   fb->status = STATUS_FAIL;
 }

// printf ("%i:%i\n", required, provided);

 status_update (fb);
 evdestroy (fb);
// free (fb);

 service_usage_query(SERVICE_UPDATE, module, NULL);

 pthread_mutex_unlock (&module->mutex);
 return module->status;
}

// event handler
void mod_event_handler(struct einit_event *event) {
 if (!event || !event->set) return;
 char **argv = (char **) event->set;
 if (argv[0] && argv[1] && !strcmp (argv[0], "modules")) {
  if (!strcmp (argv[1], "ls_modules")) {
   char buffer[1024];
   struct lmodule *cur = mlist;
   while (cur) {
    if (cur->module)
     snprintf (buffer, 1024, "%s (%s) [status=%i]\n", cur->module->rid, cur->module->name, cur->status);
    write (event->integer, buffer, strlen (buffer));
    cur = cur->next;
   }
  }
 }
}

uint16_t service_usage_query (uint16_t task, struct lmodule *module, char *service) {
 uint16_t ret = 0;
 struct uhash *ha;
 char **t;
 uint32_t i;
 struct service_usage_item *item;

 if ((!module || !module->module) && !service) return 0;

 pthread_mutex_lock (&service_usage_mutex);
 if (task & SERVICE_NOT_IN_USE) {
  ret |= SERVICE_NOT_IN_USE;
  if (t = module->module->provides) {
   for (i = 0; t[i]; i++) {
    if ((ha = hashfind (service_usage, t[i])) &&
        ((struct service_usage_item *)(ha->value))->users) {
     ret ^= SERVICE_NOT_IN_USE;
     break;
    }
   }
  }
 } else if (task & SERVICE_REQUIREMENTS_MET) {
  ret |= SERVICE_REQUIREMENTS_MET;
  if (t = module->module->requires) {
   for (i = 0; t[i]; i++) {
    if (!(ha = hashfind (service_usage, t[i])) ||
        !((struct service_usage_item *)(ha->value))->provider) {
     ret ^= SERVICE_REQUIREMENTS_MET;
     break;
    }
   }
  }
 } else if (task & SERVICE_UPDATE) {
  if (t = module->module->requires) {
   for (i = 0; t[i]; i++) {
    if ((ha = hashfind (service_usage, t[i])) && (item = (struct service_usage_item *)ha->value)) {
     item->users = module->status & STATUS_ENABLED ?
      (struct lmodule **)setadd ((void **)item->users, (void *)module, SET_NOALLOC) :
      (struct lmodule **)setdel ((void **)item->users, (void *)module);
    }
   }
  }
  if (t = module->module->provides) {
   for (i = 0; t[i]; i++) {
    if ((ha = hashfind (service_usage, t[i])) && (item = (struct service_usage_item *)ha->value)) {
     item->provider = module->status & STATUS_ENABLED ?
      (struct lmodule **)setadd ((void **)item->provider, (void *)module, SET_NOALLOC) :
      (struct lmodule **)setdel ((void **)item->provider, (void *)module);
    } else if (module->status & STATUS_ENABLED) {
     struct service_usage_item nitem;
     memset (&nitem, 0, sizeof (struct service_usage_item));
     nitem.provider = (struct lmodule **)setadd ((void **)nitem.provider, (void *)module, SET_NOALLOC);
     service_usage = hashadd (service_usage, t[i], &nitem, sizeof (struct service_usage_item), NULL);
    }
   }
  }
 } else if (task & SERVICE_IS_REQUIRED) {
  if ((ha = hashfind (service_usage, service)) && (item = (struct service_usage_item *)ha->value) && (item->users))
   ret |= SERVICE_IS_REQUIRED;
 } else if (task & SERVICE_IS_PROVIDED) {
  if ((ha = hashfind (service_usage, service)) && (item = (struct service_usage_item *)ha->value) && (item->provider))
   ret |= SERVICE_IS_PROVIDED;
/* this will be removed ASAP */
 } else if (task & SERVICE_INJECT_PROVIDER) {
  if (!(ha = hashfind (service_usage, service))) {
   struct service_usage_item nitem;
   memset (&nitem, 0, sizeof (struct service_usage_item));
   nitem.provider = (struct lmodule **)setadd ((void **)nitem.provider, (void *)module, SET_NOALLOC);
   service_usage = hashadd (service_usage, service, &nitem, sizeof (struct service_usage_item), NULL);
  }
 }

 pthread_mutex_unlock (&service_usage_mutex);
 return ret;
}
