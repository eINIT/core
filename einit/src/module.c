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
 if (sconfiguration == NULL) return -1;
 if (sconfiguration->modulepath == NULL) return -1;
 mplen = strlen (sconfiguration->modulepath) +1;
 dir = opendir (sconfiguration->modulepath);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
   if (entry->d_name[0] == '.') continue;
   tmp = (char *)malloc (mplen + strlen (entry->d_name));
   puts (entry->d_name);
   if (tmp != NULL) {
	struct smodule *modinfo;
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
	if (modinfo == NULL) puts ("unknown");
	else {
     if (modinfo->name == NULL) puts ("unknown");
     else puts (modinfo->name);
    }
    dlclose (sohandle);
	free (tmp);
   } else {
	closedir (dir);
	return -1;
   }
  }
  closedir (dir);
 } else {
  fputs ("couldn't open module directory\n", stderr);
  return -1;
 }
}
