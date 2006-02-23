/***************************************************************************
 *            module.c
 *
 *  Mon Feb  6 15:27:39 2006
 *  Copyright  2006  Magnus Deininger
 *  dma05@web.de
 ****************************************************************************/
/*
Copyright (c) 2006, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include "config.h"
#include "module.h"
#include "bitch.h"
/*
 dynamic linker functions (POSIX)

 void *dlopen(const char *filename, int flag);
 char *dlerror(void);
 void *dlsym(void *handle, const char *symbol);
 int dlclose(void *handle);
*/

int mod_scanmodules () {
 DIR *dir;
 struct dirent *entry;
 char *tmp;
 int mplen;
 void *sohandle;
 struct lmodule *cmod = NULL, *nmod;

 if (sconfiguration == NULL) return bitch(BTCH_ERRNO);
 if (sconfiguration->modulepath == NULL) return bitch(BTCH_ERRNO);

 mplen = strlen (sconfiguration->modulepath) +1;
 dir = opendir (sconfiguration->modulepath);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
   if (entry->d_name[0] == '.') continue;
   tmp = (char *)malloc (mplen + strlen (entry->d_name));
#ifdef DEBUG
   fputs (entry->d_name, stdout);
   fputs (" [", stdout);
#endif
   if (tmp != NULL) {
	struct smodule *modinfo;
    int (*func)(void *);
    *tmp = 0;
    strcat (tmp, sconfiguration->modulepath);
    strcat (tmp, entry->d_name);
	dlerror ();
    sohandle = dlopen (tmp, RTLD_NOW);
	if (sohandle == NULL) {
	 puts (dlerror ());
	 continue;
	}
	modinfo = (struct smodule *)dlsym (sohandle, "self");
#ifdef DEBUG
	if (modinfo == NULL) fputs ("unknown", stdout);
	else {
#else
	if (modinfo != NULL) {
#endif
     nmod = calloc (1, sizeof (struct lmodule));
     if (!nmod) {
      dlclose (sohandle);
      free (tmp);
      closedir (dir);
      return bitch(BTCH_ERRNO);
     }
	 if (cmod == NULL)
      mlist = nmod;
	 else
      cmod->next = nmod;
	 cmod = nmod;
     cmod->sohandle = sohandle;
     cmod->module = modinfo;
#ifdef DEBUG
     if (modinfo->name == NULL) fputs ("unknown", stdout);
     else fputs (modinfo->name, stdout);
#endif
	 if (modinfo->mode & EINIT_MOD_LOADER) {
      func = (int (*)(void *)) dlsym (sohandle, "scanmodules");
#ifdef DEBUG
      if (func == NULL) {
       fputs (" -MOD", stdout);
      } else {
       fputs (" +MOD", stdout);
#endif
       cmod->func = func;
       func ((void *)mlist);
#ifdef DEBUG
      }
#endif
	 }
    }
	
	free (tmp);
   } else {
	closedir (dir);
	return bitch(BTCH_ERRNO);
   }
#ifdef DEBUG
   fputs ("]\n", stdout);
#endif
  }
  closedir (dir);
 } else {
  fputs ("couldn't open module directory\n", stderr);
  return bitch(BTCH_ERRNO);
 }
 return 1;
}

void mod_freedesc (struct lmodule *m) {
 if (m->next != NULL)
  mod_freedesc (m->next);
 dlclose (m->sohandle);
 free (m);
}

int mod_freemodules () {
 if (mlist != NULL)
  mod_freedesc (mlist);
 mlist = NULL;
 return 1;
}
