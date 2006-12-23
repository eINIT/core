/*
 *  linux-process.c
 *  einit
 *
 *  Created by Magnus Deininger on 21/11/2006.
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

#define _MODULE

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

const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= 0,
	.options	= 0,
	.name		= "eINIT Process Function Library (Linux-Specific Parts)",
	.rid		= "linux-process",
    .si           = {
        .provides = NULL,
        .requires = NULL,
        .after    = NULL,
        .before   = NULL
    }
};

struct process_status ** update_processes_proc_linux (struct process_status **pstat) {
 DIR *dir;
 char *path = cfg_getpath ("configuration-system-proc-path");
 struct dirent *entry;

 time_t starttime = time(NULL);

 if (path) {
  size_t plength = strlen (path) +1;
  dir = opendir (path);
  if (dir != NULL) {
   char *txf = emalloc (plength);
   txf = memcpy (txf, path, plength);

   while (entry = readdir (dir)) {
    uint32_t cl = 0, cont = 1;
    if (entry->d_name[0] == '.') continue;

    for (; entry->d_name[cl]; cl++) if (!isdigit (entry->d_name[cl])) { cont = 0; break; }

    if (cont) {
     struct process_status tmppse = {.update = starttime, .pid = atoi (entry->d_name), .cwd = NULL, .cmd = NULL};
     char linkbuffer[1024];
     size_t linklen;
     txf = erealloc (txf, strlen (entry->d_name) + plength + 4);
     *(txf+plength-1) = 0;

     strcat (txf, entry->d_name);
     strcat (txf, "/cwd");

     if ((linklen = readlink (txf, linkbuffer, 1023)) != -1) {
      *(linkbuffer+linklen) = 0;

      tmppse.cwd = emalloc (linklen+1);
      memcpy (tmppse.cwd, linkbuffer, linklen+1);

//      puts (tmppse.cwd);
     }

     pstat = (struct process_status **)setadd ((void **)pstat, (void *)&tmppse, sizeof (struct process_status));

//     puts (entry->d_name);
    }

   }
   if (txf) free (txf);
  }
  closedir (dir);
 }

 return pstat;
}

int configure (struct lmodule *irr) {
 process_configure (irr);
 function_register ("einit-process-status-updater", 1, update_processes_proc_linux);
}

int cleanup (struct lmodule *this) {
 function_unregister ("einit-process-status-updater", 1, update_processes_proc_linux);
 process_cleanup (irr);
}

/* passive module... */
