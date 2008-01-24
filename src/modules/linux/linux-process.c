/*
 *  linux-process.c
 *  einit
 *
 *  Created by Magnus Deininger on 21/11/2006.
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
#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <string.h>
#include <time.h>
#include <einit-modules/process.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>


#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_process_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule module_linux_process_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT Process Function Library (Linux-Specific Parts)",
 .rid       = "linux-process",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_process_configure
};

module_register(module_linux_process_self);

#endif

struct process_status ** update_processes_proc_linux (struct process_status **pstat) {
 DIR *dir;
 char *path = cfg_getpath ("configuration-system-proc-path");
 struct dirent *entry;
 struct process_status **npstat = NULL;

 time_t starttime = time(NULL);

 if (pstat) {
  uint32_t i = 0;
  char buffer[BUFFERSIZE];
  struct stat st;

  for (; pstat[i]; i++) {
   esprintf (buffer, BUFFERSIZE, "%s%i", path, pstat[i]->pid);
   if (!stat (buffer, &st)) {
    npstat = (struct process_status **)set_fix_add ((void **)npstat, (void *)pstat[i], sizeof (struct process_status));
   }
  }
 }

 if (path) {
  size_t plength = strlen (path) +1;
  if ((dir = eopendir (path))) {
   char *txf = emalloc (plength);
   txf = memcpy (txf, path, plength);

   while ((entry = ereaddir (dir))) {
    uint32_t cl = 0;
    char cont = 1, recycled = 0;
    if (entry->d_name[0] == '.') continue;

    for (; entry->d_name[cl]; cl++) if (!isdigit (entry->d_name[cl])) { cont = 0; break; }

    if (cont) {
     struct process_status tmppse = {.update = starttime, .pid = atoi (entry->d_name), .cwd = NULL, .cmd = NULL};
     char linkbuffer[BUFFERSIZE];
     size_t linklen;
     txf = erealloc (txf, strlen (entry->d_name) + plength + 4);
     *(txf+plength-1) = 0;

     strcat (txf, entry->d_name);
     strcat (txf, "/cwd");

     if ((linklen = readlink (txf, linkbuffer, BUFFERSIZE-1)) != -1) {
      *(linkbuffer+linklen) = 0;

      tmppse.cwd = emalloc (linklen+1);
      memcpy (tmppse.cwd, linkbuffer, linklen+1);
     }

     if (npstat) {
      uint32_t i = 0;
      for (; npstat[i]; i++) {
       if (npstat[i]->pid == tmppse.pid) {
        recycled = 1;

        if (npstat[i]->cwd) efree (npstat[i]->cwd);
        if (npstat[i]->cmd) efree (npstat[i]->cmd);
        memcpy (npstat[i], &tmppse, sizeof(struct process_status));
       }
      }
     }

     if (!recycled) {
      npstat = (struct process_status **)set_fix_add ((void **)npstat, (void *)&tmppse, sizeof (struct process_status));
     }
    }

   }
   if (txf) efree (txf);

   eclosedir (dir);
  }
 }

 return npstat;
}

pid_t *filter_processes_files_below (struct pc_conditional * cond, pid_t * ret, struct process_status ** stat) {
 uint32_t i = 0;
 char *path = cfg_getpath ("configuration-system-proc-path");
 if (!path) path = "/proc/";

 if (stat && cond && cond->para)
  for (; stat[i]; i++) {
  uintptr_t tmppx = (stat[i]->pid);
  if (inset ((const void **)ret, (const void *)tmppx, SET_NOALLOC)) {
    continue;
   } else {
    DIR *dir;
    char tmp[BUFFERSIZE];
    esprintf (tmp, BUFFERSIZE, "%s%i/fd/", path, stat[i]->pid);

    if ((dir = eopendir (tmp))) {
     struct dirent *entry;

     while ((entry = ereaddir (dir))) {
      struct stat buf;
      esprintf (tmp, BUFFERSIZE, "%s%i/fd/%s", path, stat[i]->pid, entry->d_name);

      if (!lstat(tmp, &buf) && S_ISLNK(buf.st_mode)) {
       char ttarget[BUFFERSIZE];
       int r = readlink (tmp, ttarget, BUFFERSIZE-1);
       if (r == -1) continue;
       ttarget[r] = 0;

       if (strstr (ttarget, cond->para) == ttarget) {
        ret = (pid_t *)set_noa_add ((void **)ret, (void *)tmppx);
        goto kk;
       }
      }
     }
     kk:
     eclosedir (dir);
    }
   }
  }

 return ret;
}

char process_linux_pid_is_running (pid_t pid) {
 char tmp[BUFFERSIZE];
 struct stat st;
 esprintf (tmp, BUFFERSIZE, "/proc/%i", pid);

 return (!stat (tmp, &st));
}

int linux_process_cleanup (struct lmodule *this) {
 function_unregister ("einit-process-status-updater", 1, update_processes_proc_linux);
 function_unregister ("einit-process-filter-files-below", 1, filter_processes_files_below);
 function_unregister ("einit-process-is-running", 1, process_linux_pid_is_running);
 process_cleanup (irr);

 return 0;
}

int linux_process_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->cleanup = linux_process_cleanup;

 process_configure (irr);
 function_register ("einit-process-status-updater", 1, update_processes_proc_linux);
 function_register ("einit-process-filter-files-below", 1, filter_processes_files_below);
 function_register ("einit-process-is-running", 1, process_linux_pid_is_running);

 return 0;
}

/* passive module... */
