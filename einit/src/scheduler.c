/*
 *  scheduler.c
 *  einit
 *
 *  Created by Magnus Deininger on 02/05/2006.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <einit/scheduler.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <signal.h>

pthread_t schedthread = 0;

int cleanup ();

#ifdef LINUX
#include <linux/reboot.h>

int epoweroff () {
 reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
 bitch (BTCH_ERRNO);
 puts ("\naight, who hasn't eaten his cereals this morning?");
 exit (EXIT_FAILURE);
}

int epowerreset () {
 reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART, NULL);
 bitch (BTCH_ERRNO);
 puts ("\naight, who hasn't eaten his cereals this morning?");
 exit (EXIT_FAILURE);
}
#else
int epoweroff () {
 exit (EXIT_SUCCESS);
}
int epowerreset () {
 exit (EXIT_SUCCESS);
}
#endif

int switchmode (char *mode) {
 if (!mode) return -1;
 printf ("scheduler: switching to mode \"%s\": ", mode);
 if (sconfiguration) {
  struct cfgnode *cur = cfg_findnode (mode, EI_NODETYPE_MODE, NULL);
  struct cfgnode *opt;
  struct mloadplan *plan;
  char **elist;
  unsigned int optmask = 0;

  if (!cur) {
   puts ("mode not defined, aborting");
   return -1;
  }
  opt = NULL;
  while (opt = cfg_findnode ("disable-unspecified", 0, opt)) {
   if (opt->mode == cur) {
    if (opt->flag) optmask |= MOD_DISABLE_UNSPEC;
    else optmask &= !MOD_DISABLE_UNSPEC;
   }
  }
  elist = strsetdup (cur->enable);
  newmode = mode;

  if (cur->base) {
   int y = 0;
   struct cfgnode *cno;
   while (cur->base[y]) {
	cno = cfg_findnode (cur->base[y], EI_NODETYPE_MODE, NULL);
	if (cno) {
     elist = (char **)setcombine ((void **)strsetdup (cno->enable), (void **)elist);
    }
    y++;
   }
  }

  elist = strsetdeldupes (elist);

  plan = mod_plan (NULL, elist, MOD_ENABLE | optmask);
  if (!plan) {
   puts ("I guess I'm... clueless");
  } else {
#ifdef DEBUG
   mod_plan_ls (plan);
#endif
   puts ("commencing");
   mod_plan_commit (plan);
   currentmode = mode;
   mod_plan_free (plan);
  }
 }

 return 0;
}

void sched_init () {
 struct sigaction action;

 action.sa_sigaction = sched_signal_sigchld;
 sigemptyset(&(action.sa_mask));
 action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO | SA_RESTART;

 if ( sigaction (SIGCHLD, &action, NULL) ) bitch (BTCH_ERRNO);

 action.sa_sigaction = sched_signal_sigint;
 if ( sigaction (SIGINT, &action, NULL) ) bitch (BTCH_ERRNO);
}

int sched_queue (unsigned int task, void *param) {
 struct sschedule *nele = ecalloc (1, sizeof (struct sschedule));
 nele->task = task;
 nele->param = param;
 schedule = (struct sschedule **) setadd ((void **)schedule, (void *)nele);
 if (!schedthread) pthread_create (&schedthread, NULL, sched_run, NULL);
}

int sched_watch_pid (pid_t pid, void (*function)(pid_t)) {
 struct spidcb *nele = ecalloc (1, sizeof (struct spidcb));
 nele->pid = pid;
 nele->cfunc = function;
 cpids = (struct spidcb **) setadd ((void **)cpids, (void *)nele);
}

void *sched_run (void *p) {
 int i, l;
 pthread_detach (schedthread);
 while (schedule) {
  struct sschedule *c = schedule[0];
  switch (c->task) {
   case SCHEDULER_PID_NOTIFY:
    if (!cpids || !cpids[0]) {
     fputs ("scheduler: warning: SCHEDULER_PID_NOTIFY scheduled, but cpids[] is empty", stderr);
     break;
    }
    l = setcount ((void **)cpids);
    for (i = 0; i < l; i++) {
     if (cpids[i]->pid == (pid_t)c->param) {
      if (cpids[i]->cfunc)
       cpids[i]->cfunc ((pid_t)c->param);
      cpids = (struct spidcb **) setdel ((void **)cpids, (void *)cpids[i]);
      break;
     }
    }
    break;
   case SCHEDULER_SWITCH_MODE:
    switchmode (c->param);
    break;
   case SCHEDULER_POWER_OFF:
    puts ("scheduler: sync()-ing");
    sync ();
    puts ("scheduler: cleaning up");
    cleanup ();
    puts ("scheduler: power off");
    epoweroff ();
// if we still live here, something's twocked
    exit (EXIT_FAILURE);
    break;
   case SCHEDULER_POWER_RESET:
    puts ("scheduler: sync()-ing");
    sync ();
    puts ("scheduler: cleaning up");
    cleanup ();
    puts ("scheduler: reset");
    epowerreset ();
// if we still live here, something's twocked
    exit (EXIT_FAILURE);
    break;
  }
  schedule = (struct sschedule **) setdel ((void **)schedule, (void *)c);
  free (c);
 }
 schedthread = 0;
}

/* signal handlers */

/* there's one potential bug in here: the set* functions need to *alloc(), but
   these functions aren't considered to be "safe functions" by POSIX :/ */

// this should prevent any zombies from being created

void sched_signal_sigchld (int signal, siginfo_t *siginfo, void *context) {
 int i, l = setcount ((void **)cpids);
 pid_t pid = siginfo->si_pid;
// check if anyone was waiting to be informed of this pid's death
 for (i = 0; i < l; i++) {
  if (cpids[i]->pid == pid) {
   sched_queue (SCHEDULER_PID_NOTIFY, (void *)pid);
   return;
  }
 }
// wait on it to prevent zombie-kids
 waitpid (pid, &i, 0);
 return;
}

// (on linux) SIGINT to INIT means ctrl+alt+del was pressed

void sched_signal_sigint (int signal, siginfo_t *siginfo, void *context) {
 if (siginfo->si_code == SI_KERNEL) {
  sched_queue (SCHEDULER_SWITCH_MODE, "power-reset");
  sched_queue (SCHEDULER_POWER_RESET, NULL);
 }
 return;
}
