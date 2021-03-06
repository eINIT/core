/*
 *  module-so.c
 *  einit
 *
 *  split from module.c on 19/03/2007.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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

char **einit_mod_so_modules = NULL;

int einit_mod_so_scanmodules (struct lmodule *);

int einit_mod_so_cleanup (struct lmodule *pa) {
 return 0;
}

int einit_mod_so_scanmodules ( struct lmodule *modchain ) {
 void *sohandle;
 char **modules = NULL;
 struct cfgnode *node = NULL;
 struct lmodule *lm = modchain;

 emutex_lock (&modules_update_mutex);

 while ((node = cfg_findnode ("core-settings-modules", 0, node))) {
  char **nmodules = readdirfilter(node, "/lib/einit/modules/", ".*\\.so", NULL, 0);

  if (nmodules) {
   modules = (char **)setcombine_nc ((void **)modules, (const void **)nmodules, SET_TYPE_STRING);

   efree (nmodules);
  }
 }

 if (!modules) {
  modules = readdirfilter(cfg_getnode ("core-settings-modules", NULL),
#ifdef DO_BOOTSTRAP
                                 BOOTSTRAP_MODULE_PATH
#else
                                 "/lib/einit/modules/"
#endif
                                 , ".*\\.so", NULL, 0);
 }

/* make sure all bootstrap modules get updated */
 while (lm) {
  if (lm->source && (strprefix(lm->source, BOOTSTRAP_MODULE_PATH) || inset ((const void **)einit_mod_so_modules, lm->source, SET_TYPE_STRING))) {
   lm = mod_update (lm);

   if (lm->module && (lm->module->mode & einit_module_loader) && (lm->scanmodules != NULL)) {
    lm->scanmodules (modchain);
   }
  }
  lm = lm->next;
 }

/* load all new modules */
 if (modules) {
  uint32_t z = 0;

  for (; modules[z]; z++) {
//   fprintf (stderr, "* loading: %s\n", modules[z]);
   if (inset ((const void **)einit_mod_so_modules, modules[z], SET_TYPE_STRING)) {
    continue;
   }

   einit_mod_so_modules = set_str_add_stable (einit_mod_so_modules, modules[z]);

   struct smodule **modinfo;
   lm = modchain;

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
    einit_mod_so_modules = strsetdel (einit_mod_so_modules, modules[z]);

    puts (dlerror ());
    goto cleanup_continue;
   }

   modinfo = (struct smodule **)dlsym (sohandle, "self");
   if ((modinfo != NULL) && ((*modinfo) != NULL)) {
    if ((*modinfo)->eibuild == BUILDNUMBER) {
     struct lmodule *new = mod_add (sohandle, (*modinfo));
     if (new) {
      new->source = (char *)str_stabilise(modules[z]);
     } else {
      notice (6, "module %s: not loading: module refused to get loaded.\n", modules[z]);

      einit_mod_so_modules = strsetdel (einit_mod_so_modules, modules[z]);

      dlclose (sohandle);
     }
    } else {
     notice (1, "module %s: not loading: different build number: %i.\n", modules[z], (*modinfo)->eibuild);

     einit_mod_so_modules = strsetdel (einit_mod_so_modules, modules[z]);

     dlclose (sohandle);
    }
   } else {
    notice (1, "module %s: not loading: missing header.\n", modules[z]);

    einit_mod_so_modules = strsetdel (einit_mod_so_modules, modules[z]);

    dlclose (sohandle);
   }

   cleanup_continue: ;
  }

  efree (modules);
 } else {
  perror ("opening module path");
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
