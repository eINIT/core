/*
 *  module-so.c
 *  einit
 *
 *  split from module.c on 19/03/2007.
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>
#include <einit/configuration.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_mod_so_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_mod_so_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (.so)",
 .rid       = "einit-module-so",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_mod_so_configure
};

module_register(einit_mod_so_self);

#endif

pthread_mutex_t modules_update_mutex = PTHREAD_MUTEX_INITIALIZER;

int einit_mod_so_scanmodules (struct lmodule *);

int einit_mod_so_cleanup (struct lmodule *pa) {
 return 0;
}

int einit_mod_so_do_suspend (struct lmodule *pa) {
 if (pa->sohandle) {
  const struct smodule *o = pa->module;
  struct smodule *n = emalloc (sizeof (struct smodule));
  memset (n, 0, sizeof (struct smodule));

  n->rid = estrdup (o->rid);
  n->name = estrdup (o->name);

  if (o->si.requires) { n->si.requires = (char **)setdup ((const void **)o->si.requires, SET_TYPE_STRING); }
  if (o->si.provides) { n->si.provides = (char **)setdup ((const void **)o->si.provides, SET_TYPE_STRING); }
  if (o->si.before) { n->si.before = (char **)setdup ((const void **)o->si.before, SET_TYPE_STRING); }
  if (o->si.after) { n->si.after = (char **)setdup ((const void **)o->si.after, SET_TYPE_STRING); }
  if (o->si.shutdown_before) { n->si.shutdown_before = (char **)setdup ((const void **)o->si.shutdown_before, SET_TYPE_STRING); }
  if (o->si.shutdown_after) { n->si.shutdown_after = (char **)setdup ((const void **)o->si.shutdown_after, SET_TYPE_STRING); }

  pa->module = n;

  if (!dlclose (pa->sohandle)) {   
   pa->sohandle = NULL;

   return status_ok;
  } else {
   return status_failed;
  }
 } else return status_ok;
}

int einit_mod_so_do_resume (struct lmodule *pa) {
 void *sohandle = dlopen (pa->source, RTLD_NOW);
 if (sohandle == NULL) {
  eputs (dlerror (), stdout);
  return status_failed;
 }

 struct smodule **modinfo = (struct smodule **)dlsym (sohandle, "self");
 if ((modinfo != NULL) && ((*modinfo) != NULL)) {
  if ((*modinfo)->eibuild == BUILDNUMBER) {
   struct smodule *sm = (struct smodule *)pa->module;
   pa->module = *modinfo;

   if (sm) {
    if (sm->si.provides) free (sm->si.provides);
    if (sm->si.requires) free (sm->si.requires);
    if (sm->si.after) free (sm->si.after);
    if (sm->si.before) free (sm->si.before);
    if (sm->si.shutdown_after) free (sm->si.shutdown_after);
    if (sm->si.shutdown_before) free (sm->si.shutdown_before);

    free (sm->rid);
    free (sm->name);
    free (sm);
   }

   pa->do_suspend = einit_mod_so_do_suspend;
   pa->do_resume = einit_mod_so_do_resume;

   pa->sohandle = sohandle;

   pa->module->configure (pa);
  } else {
   notice (1, "module %s: not loading: different build number: %i.\n", pa->source, (*modinfo)->eibuild);

   dlclose (sohandle);
   return status_failed;
  }
 } else {
  notice (1, "module %s: not loading: missing header.\n", pa->source);

  dlclose (sohandle);
  return status_failed;
 }

 return status_ok;
}

int einit_mod_so_scanmodules ( struct lmodule *modchain ) {
 void *sohandle;
 char **modules = NULL;
 struct cfgnode *node = NULL;

 emutex_lock (&modules_update_mutex);

 while ((node = cfg_findnode ("core-settings-modules", 0, node))) {
  char **nmodules = readdirfilter(node, "/lib/einit/modules/", ".*\\.so", NULL, 0);

  if (nmodules) {
   modules = (char **)setcombine_nc ((void **)modules, (const void **)nmodules, SET_TYPE_STRING);
  }
 }

 if (!modules) {
  modules = readdirfilter(cfg_getnode ("core-settings-modules", NULL),
#ifdef DO_BOOTSTRAP
                                 bootstrapmodulepath
#else
                                 "/lib/einit/modules/"
#endif
                                 , ".*\\.so", NULL, 0);
 }

 if (modules) {
  uint32_t z = 0;

  for (; modules[z]; z++) {
   struct smodule **modinfo;
   struct lmodule *lm = modchain;

   while (lm) {
    if (lm->source && strmatch(lm->source, modules[z])) {
     lm = mod_update (lm);

     if (lm->module && (lm->module->mode & einit_module_loader) && (lm->scanmodules != NULL)) {
      lm->scanmodules (modchain);
     }

     goto cleanup_continue;
    }
    lm = lm->next;
   }

   dlerror();

   sohandle = dlopen (modules[z], RTLD_NOW);
   if (sohandle == NULL) {
    eputs (dlerror (), stdout);
    goto cleanup_continue;
   }

   modinfo = (struct smodule **)dlsym (sohandle, "self");
   if ((modinfo != NULL) && ((*modinfo) != NULL)) {
    if ((*modinfo)->eibuild == BUILDNUMBER) {
     struct lmodule *new = mod_add (sohandle, (*modinfo));
     if (new) {
      new->source = estrdup(modules[z]);
	  new->do_suspend = einit_mod_so_do_suspend;
	  new->do_resume = einit_mod_so_do_resume;
     }
    } else {
     notice (1, "module %s: not loading: different build number: %i.\n", modules[z], (*modinfo)->eibuild);

     dlclose (sohandle);
    }
   } else {
    notice (1, "module %s: not loading: missing header.\n", modules[z]);

    dlclose (sohandle);
   }

   cleanup_continue: ;
  }

  free (modules);
 }


 emutex_unlock (&modules_update_mutex);

 return 1;
}

int einit_mod_so_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = einit_mod_so_scanmodules;
 pa->cleanup = einit_mod_so_cleanup;

 return 0;
}
