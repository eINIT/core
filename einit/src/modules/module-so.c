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

#define _MODULE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
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

#ifdef POSIXREGEX
#include <regex.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int _einit_mod_so_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)

const struct smodule _einit_mod_so_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_LOADER,
 .options   = 0,
 .name      = "Module Support (.so)",
 .rid       = "module-so",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _einit_mod_so_configure
};

module_register(_einit_mod_so_self);

#endif

pthread_mutex_t modules_update_mutex = PTHREAD_MUTEX_INITIALIZER;

int _einit_mod_so_scanmodules (struct lmodule *);

int _einit_mod_so_cleanup (struct lmodule *pa) {
 return 0;
}

int _einit_mod_so_scanmodules ( struct lmodule *modchain ) {
 DIR *dir;
 struct dirent *entry;
 char *tmp;
 int mplen;
 void *sohandle;
#ifdef POSIXREGEX
 regex_t allowpattern, disallowpattern;
 unsigned char haveallowpattern = 0, havedisallowpattern = 0;
 char *spattern = NULL;
#endif

 emutex_lock (&modules_update_mutex);

 char *modulepath = cfg_getpath ("core-settings-module-path");
 if (!modulepath) {
#ifdef DO_BOOTSTRAP
  modulepath = bootstrapmodulepath;
#endif
 }
 if (!modulepath) {
//  bitch(BITCH_STDIO, 0, "no path to load modules from.");
  emutex_unlock (&modules_update_mutex);
  return -1;
 }

 if (gmode == EINIT_GMODE_SANDBOX) {
// override module path in sandbox-mode to be relative
  if (modulepath[0] == '/') modulepath++;
 }

 notice (4, "updating modules in \"%s\".\n", modulepath);

#ifdef POSIXREGEX
 if ((spattern = cfg_getstring ("core-settings-module-load/pattern-allow", NULL))) {
  haveallowpattern = !eregcomp (&allowpattern, spattern);
 }

 if ((spattern = cfg_getstring ("core-settings-module-load/pattern-disallow", NULL))) {
  havedisallowpattern = !eregcomp (&disallowpattern, spattern);
 }
#endif

 mplen = strlen (modulepath) +4;
 dir = eopendir (modulepath);
 if (dir != NULL) {
  while ((entry = ereaddir (dir))) {
//   uint32_t el = 0;
// if we have posix regular expressions, match them against the filename, if not, exclude '.'-files
#ifdef POSIXREGEX
   if (haveallowpattern && regexec (&allowpattern, entry->d_name, 0, NULL, 0)) continue;
   if (havedisallowpattern && !regexec (&disallowpattern, entry->d_name, 0, NULL, 0)) continue;
#else
   if (entry->d_name[0] == '.') continue;
#endif

//   tmp = (char *)emalloc (el = (((mplen + strlen (entry->d_name))) & (~3))+4);
   tmp = (char *)emalloc (mplen + strlen (entry->d_name));
   struct stat sbuf;
   struct smodule **modinfo;
   struct lmodule *lm;
   *tmp = 0;
   strcat (tmp, modulepath);
   strcat (tmp, entry->d_name);
   dlerror ();
   if (stat (tmp, &sbuf) || !S_ISREG (sbuf.st_mode)) {
    goto cleanup_continue;
   }

   lm = modchain;
   while (lm) {
    if (lm->source && strmatch(lm->source, tmp)) {
     lm = mod_update (lm);

     if (lm->module && (lm->module->mode & EINIT_MOD_LOADER) && (lm->scanmodules != NULL)) {
      lm->scanmodules (modchain);
     }

     goto cleanup_continue;
    }
    lm = lm->next;
   }

   sohandle = dlopen (tmp, RTLD_NOW);
   if (sohandle == NULL) {
    eputs (dlerror (), stdout);
    goto cleanup_continue;
   }

   modinfo = (struct smodule **)dlsym (sohandle, "self");
   if ((modinfo != NULL) && ((*modinfo) != NULL)) {
    if ((*modinfo)->eibuild == BUILDNUMBER) {
     struct lmodule *new = mod_add (sohandle, (*modinfo));
     if (new) {
      new->source = estrdup(tmp);
     }
    } else {
     notice (1, "module %s: not loading: different build number: %i.\n", tmp, (*modinfo)->eibuild);

     dlclose (sohandle);
    }
   } else {
    notice (1, "module %s: not loading: missing header.\n", tmp);

    dlclose (sohandle);
   }

   cleanup_continue:
     free (tmp);
  }
  eclosedir (dir);
 }

#ifdef POSIXREGEX
 if (haveallowpattern) { haveallowpattern = 0; regfree (&allowpattern); }
 if (havedisallowpattern) { havedisallowpattern = 0; regfree (&disallowpattern); }
#endif

 emutex_unlock (&modules_update_mutex);

 return 1;
}

int _einit_mod_so_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = _einit_mod_so_scanmodules;
 pa->cleanup = _einit_mod_so_cleanup;

 return 0;
}
