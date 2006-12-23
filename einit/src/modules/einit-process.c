/*
 *  einit-process.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/09/2006.
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
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/event.h>
#include <einit-modules/process.h>
#include <errno.h>
#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= 0,
	.options	= 0,
	.name		= "eINIT Process Function Library",
	.rid		= "einit-process",
    .si           = {
        .provides = NULL,
        .requires = NULL,
        .after    = NULL,
        .before   = NULL
    }
};

struct process_status **ps = NULL;

pid_t **collect_processes(struct pc_conditional **pcc) {
 pid_t **ret = NULL;
 process_status_updater pse = function_find_one("einit-process-status-updater", 1, NULL);
 uint32_t i;

 if (!pcc) return;

 if (pse)
  ps = pse (ps);

 if (ps) for (i = 0; pcc[i]; i++) {
  process_filter pf = NULL;
  char *pm[2] = { (pcc[i]->match), NULL };

  if (!(pcc[i]->match)) continue;

  pf = function_find_one("einit-process-filter", 1, pm);
  if (pf) ret = pf (pcc[i], ret, ps);
 }

 return ret;
}


pid_t **filter_processes_cwd_below (struct pc_conditional * cond, pid_t ** ret, struct process_status ** stat) {
 uint32_t i = 0;
 if (stat && cond && cond->para)
  for (; stat[i]; i++) {
   if (stat[i]->cwd && (strstr (stat[i]->cwd, (char *)cond->para) == stat[i]->cwd))
    ret = (pid_t **)setadd ((void **)ret, (void *)(stat[i]->pid), SET_NOALLOC);
  }

 return ret;
}

pid_t **filter_processes_cwd (struct pc_conditional * cond, pid_t ** ret, struct process_status ** stat) {
 uint32_t i = 0;
 if (stat && cond && cond->para)
  for (; stat[i]; i++) {
   if (stat[i]->cwd && !strcmp ((char *)cond->para, stat[i]->cwd))
    ret = (pid_t **)setadd ((void **)ret, (void *)(stat[i]->pid), SET_NOALLOC);
  }

 return ret;
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && !strcmp (ev->set[0], "list")) {
  if (ev->set[1] && !strcmp (ev->set[1], "processes") && ev->set[2]) {
   struct pc_conditional pcc = {.match = ev->set[2], .para = (ev->set[3] ? ((!strcmp (ev->set[2], "cwd") || !strcmp (ev->set[2], "cwd-below")) ? (void *)ev->set[3] : (void *)atoi (ev->set[3])) : NULL), .match_options = PC_COLLECT_ADDITIVE},
                         *pcl[2] = { &pcc, NULL };
   pid_t **process_list = NULL, i;

   process_list = pcollect ( pcl );

   if (process_list) {
    for (i = 0; process_list[i]; i++) {
     char buffer [1024];
     snprintf (buffer, 1024, "pid=%i\n", process_list[i]);
     fdputs (buffer, ev->integer);
    }
    free (process_list);
   } else {
    fdputs ("einit-process: ipc-event-handler: your query has matched no processes\n", ev->integer);
   }

   ev->flag ++;
  }
 }
}

int __ekill (struct pc_conditional **pcc, int sign) {
 pid_t **pl = pcollect (pcc);
 uint32_t i = 0;

 if (!pl) return -1;

 for (; pl[i]; i++) if ((pl[i] != 1) && (pl[i] != getpid())){
  fprintf (stderr, "sending signal %i to process %i", sign, pl[i]);
  kill ((pid_t)pl[i], sign);
 }

 free (pl);
}

int __pekill (struct pc_conditional **pcc) {
/* pid **pl = pcollect (pcc);

 free (pl);*/
 ekill (pcc, SIGKILL);
}

int configure (struct lmodule *irr) {
 process_configure (irr);
 function_register ("einit-process-filter-cwd", 1, filter_processes_cwd);
 function_register ("einit-process-filter-cwd-below", 1, filter_processes_cwd_below);
 function_register ("einit-process-collect", 1, collect_processes);
 function_register ("einit-process-ekill", 1, __ekill);
 function_register ("einit-process-killing-spree", 1, __pekill);
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 function_unregister ("einit-process-killing-spree", 1, __pekill);
 function_unregister ("einit-process-ekill", 1, __ekill);
 function_unregister ("einit-process-collect", 1, collect_processes);
 function_unregister ("einit-process-filter-cwd-below", 1, filter_processes_cwd_below);
 function_unregister ("einit-process-filter-cwd", 1, filter_processes_cwd);
 process_cleanup (irr);
}

/* passive module, no enable/disable */
