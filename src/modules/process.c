/*
 *  process.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/09/2006.
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
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/event.h>
#include <einit-modules/process.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_process_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_process_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT Process Function Library",
 .rid       = "einit-process",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_process_configure
};

module_register(einit_process_self);

#endif

struct process_status **ps = NULL;

pthread_mutex_t process_kill_command_mutex = PTHREAD_MUTEX_INITIALIZER;

pid_t *collect_processes(struct pc_conditional **pcc) {
 emutex_lock (&process_kill_command_mutex);

 pid_t *ret = NULL;
 process_status_updater pse = function_find_one("einit-process-status-updater", 1, NULL);
 uint32_t i;

 if (!pcc) return NULL;

 if (pse) {
  struct process_status **nps = pse (ps);
  efree (ps);
  ps = nps;
 }

 if (ps) for (i = 0; pcc[i]; i++) {
  process_filter pf = NULL;
  const char *pm[2] = { (pcc[i]->match), NULL };

  if (!(pcc[i]->match)) continue;

  pf = function_find_one("einit-process-filter", 1, pm);
  if (pf) ret = pf (pcc[i], ret, ps);
 }

 emutex_unlock (&process_kill_command_mutex);
 return ret;
}


pid_t *filter_processes_cwd_below (struct pc_conditional * cond, pid_t * ret, struct process_status ** stat) {
 uint32_t i = 0;
 if (stat && cond && cond->para)
  for (; stat[i]; i++) {
   if (stat[i]->cwd && (strprefix (stat[i]->cwd, (char *)cond->para))) {
    uintptr_t tmp = (stat[i]->pid);
    ret = (pid_t *)set_noa_add((void **)ret, (void *)tmp);
   }
  }

 return ret;
}

pid_t *filter_processes_cwd (struct pc_conditional * cond, pid_t * ret, struct process_status ** stat) {
 uint32_t i = 0;
 if (stat && cond && cond->para)
  for (; stat[i]; i++) {
   if (stat[i]->cwd && strmatch ((char *)cond->para, stat[i]->cwd)) {
    uintptr_t tmp = (stat[i]->pid);
    ret = (pid_t *)set_noa_add((void **)ret, (void *)tmp);
   }
  }

 return ret;
}

void einit_process_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && strmatch (ev->argv[0], "list")) {
  if (ev->argv[1] && strmatch (ev->argv[1], "processes") && ev->argv[2]) {
   uintptr_t tnum = atoi (ev->argv[3]);
   struct pc_conditional pcc = {
    .match = ev->argv[2],
    .para = (ev->argv[3] ?
      ((strmatch (ev->argv[2], "cwd") || strmatch (ev->argv[2], "cwd-below") || strmatch (ev->argv[2], "files-below")) ?
      (void *)ev->argv[3] : (void *)tnum) : NULL),
    .match_options = einit_pmo_additive },
    *pcl[2] = { &pcc, NULL };
   pid_t *process_list = NULL, i;

   process_list = pcollect ( pcl );

   if (process_list) {
    for (i = 0; process_list[i]; i++) {
     if (ev->ipc_options & einit_ipc_output_xml) {
      eprintf (ev->output, " <process pid=\"%i\" />\n", process_list[i]);
     } else {
      eprintf (ev->output, "process [pid=%i]\n", process_list[i]);
     }
    }
    efree (process_list);
   } else {
    eputs ("einit-process: ipc-event-handler: your query has matched no processes\n", ev->output);
   }

   ev->implemented ++;
  }
 }
}

int ekill_f (struct pc_conditional **pcc, int sign) {
 pid_t *pl = pcollect (pcc);

 uint32_t i = 0;

 if (!pl) return -1;

 for (; pl[i]; i++) if ((pl[i] != 1) && (pl[i] != getpid())) {
  if (coremode != einit_mode_sandbox) {
   debugx(" >> sending signal %i to process %i\n", sign, pl[i]);
   kill ((pid_t)pl[i], sign);
  }
 }

 efree (pl);

 return i;
}

int pekill_f (struct pc_conditional **pcc) {
/* pid **pl = pcollect (pcc);

 efree (pl);*/
 return ekill (pcc, SIGKILL);
}

int einit_process_cleanup (struct lmodule *irr) {
 event_ignore (einit_ipc_request_generic, einit_process_ipc_event_handler);
 function_unregister ("einit-process-killing-spree", 1, pekill_f);
 function_unregister ("einit-process-ekill", 1, ekill_f);
 function_unregister ("einit-process-collect", 1, collect_processes);
 function_unregister ("einit-process-filter-cwd-below", 1, filter_processes_cwd_below);
 function_unregister ("einit-process-filter-cwd", 1, filter_processes_cwd);
 process_cleanup (irr);

 return 0;
}

int einit_process_configure (struct lmodule *irr) {
 module_init (irr);
 irr->cleanup = einit_process_cleanup;

 process_configure (irr);
 function_register ("einit-process-filter-cwd", 1, filter_processes_cwd);
 function_register ("einit-process-filter-cwd-below", 1, filter_processes_cwd_below);
 function_register ("einit-process-collect", 1, collect_processes);
 function_register ("einit-process-ekill", 1, ekill_f);
 function_register ("einit-process-killing-spree", 1, pekill_f);
 event_listen (einit_ipc_request_generic, einit_process_ipc_event_handler);

 return 0;
}

/* passive module, no enable/disable */
