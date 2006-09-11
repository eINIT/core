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
#include <sys/wait.h>
#include <einit/scheduler.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <einit/event.h>
#include <signal.h>
#include <errno.h>
#include <sys/reboot.h>

pthread_cond_t schedthreadcond = PTHREAD_COND_INITIALIZER;
pthread_cond_t schedthreadsigchildcond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t schedthreadmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t schedthreadsigchildmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t schedschedulemodmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t schedcpidmutex = PTHREAD_MUTEX_INITIALIZER;

char *currentmode = "void";
char *newmode = "void";
stack_t signalstack;

struct spidcb *cpids = NULL;
struct spidcb *sched_deadorphans = NULL;

int cleanup ();

#ifdef LINUX
#include <linux/reboot.h>

int epoweroff () {
 stack_t curstack;
 pthread_cond_destroy (&schedthreadcond);
 pthread_mutex_destroy (&schedthreadmutex);

 if (!sigaltstack (NULL, &curstack) && !(curstack.ss_flags & SS_ONSTACK)) {
  curstack.ss_size = SIGSTKSZ;
  curstack.ss_flags = SS_DISABLE;
  sigaltstack (&curstack, NULL);
  free (curstack.ss_sp);
 } else {
  fputs ("schedule: no alternate signal stack or alternate stack in use; not cleaning up", stderr);
 }

#ifndef SANDBOX
 reboot (LINUX_REBOOT_CMD_POWER_OFF);
// reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
// bitch (BTCH_ERRNO);
 puts ("\naight, who hasn't eaten his cereals this morning?");
 exit (EXIT_FAILURE);
#else
 puts ("compiled in sandbox-mode: not sending power-off command");
 exit (EXIT_SUCCESS);
#endif
}

int epowerreset () {
 stack_t curstack;
 pthread_cond_destroy (&schedthreadcond);
 pthread_mutex_destroy (&schedthreadmutex);

 if (!sigaltstack (NULL, &curstack) && !(curstack.ss_flags & SS_ONSTACK)) {
  curstack.ss_size = SIGSTKSZ;
  curstack.ss_flags = SS_DISABLE;
  sigaltstack (&curstack, NULL);
  free (curstack.ss_sp);
 } else {
  fputs ("schedule: no alternate signal stack or alternate stack in use; not cleaning up", stderr);
 }

#ifndef SANDBOX
 reboot (LINUX_REBOOT_CMD_RESTART);
// reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART, NULL);
// bitch (BTCH_ERRNO);
 puts ("\naight, who hasn't eaten his cereals this morning?");
 exit (EXIT_FAILURE);
#else
 puts ("compiled in sandbox-mode: not sending reboot command");
 exit (EXIT_SUCCESS);
#endif
}
#else
int epoweroff () {
 pthread_cond_destroy (&schedthreadcond);
 pthread_mutex_destroy (&schedthreadmutex);
 exit (EXIT_SUCCESS);
}
int epowerreset () {
 pthread_cond_destroy (&schedthreadcond);
 pthread_mutex_destroy (&schedthreadmutex);
 exit (EXIT_SUCCESS);
}
#endif

int sched_switchmode (char *mode) {
 if (!mode) return -1;
 struct einit_event *fb = evinit (EINIT_EVENT_TYPE_FEEDBACK);
  struct cfgnode *cur = cfg_findnode (mode, EI_NODETYPE_MODE, NULL);
  struct cfgnode *opt;
  struct mloadplan *plan = NULL;
  char **elist, **dlist;
  unsigned int optmask = 0;

  if (!cur) {
   puts ("scheduler: scheduled mode not defined, aborting");
   free (fb);
   return -1;
  }
  opt = NULL;
  while (opt = cfg_findnode ("disable-unspecified", 0, opt)) {
   if (opt->mode == cur) {
    if (opt->flag) optmask |= MOD_DISABLE_UNSPEC;
    else optmask &= !MOD_DISABLE_UNSPEC;
   }
  }
  elist = (char **)setdup ((void **)cur->enable, -1);
  dlist = (char **)setdup ((void **)cur->disable, -1);
  newmode = mode;

  if (cur->base) {
   int y = 0;
   struct cfgnode *cno;
   while (cur->base[y]) {
    cno = cfg_findnode (cur->base[y], EI_NODETYPE_MODE, NULL);
    if (cno) {
     elist = (char **)setcombine ((void **)setdup ((void **)cno->enable, -1), (void **)elist, -1);
     dlist = (char **)setcombine ((void **)setdup ((void **)cno->disable, -1), (void **)dlist, -1);
    }
    y++;
   }
  }

  elist = strsetdeldupes (elist);
  dlist = strsetdeldupes (dlist);

  if (elist || optmask)
   plan = mod_plan (plan, elist, MOD_ENABLE | optmask);
  if (dlist)
   plan = mod_plan (plan, dlist, MOD_DISABLE | optmask);
  if (!plan) {
   puts ("scheduler: scheduled mode defined but nothing to be done");
  } else {
#ifdef DEBUG
   mod_plan_ls (plan);
#endif
   fb->task = MOD_SCHEDULER_PLAN_COMMIT_START;
   fb->para = (void *)plan;
   status_update (fb);
   mod_plan_commit (plan);
   currentmode = mode;
   fb->task = MOD_SCHEDULER_PLAN_COMMIT_FINISH;
   status_update (fb);
   mod_plan_free (plan);
  }

 evdestroy (fb);
 return 0;
}

int sched_modaction (char **argv) {
 int argc = setcount ((void **)argv);
 int32_t task;
 struct mloadplan *plan;
 if (!argv || (argc != 2)) return -1;

 if (!strcmp (argv[1], "enable")) task = MOD_ENABLE;
 else if (!strcmp (argv[1], "disable")) task = MOD_DISABLE;
 else if (!strcmp (argv[1], "reset")) task = MOD_RESET;
 else if (!strcmp (argv[1], "reload")) task = MOD_RELOAD;

 argv[1] = NULL;

 if (plan = mod_plan (NULL, argv, task)) {
#ifdef DEBUG
  mod_plan_ls (plan);
#endif
  mod_plan_commit (plan);
 }

// free (argv[0]);
 free (argv);

 return 0;
}

void sched_init () {
 struct sigaction action;

 signalstack.ss_sp = emalloc (SIGSTKSZ);
 signalstack.ss_size = SIGSTKSZ;
 signalstack.ss_flags = 0;
 sigaltstack (&signalstack, NULL);

/* create our sigchld-scheduler-thread right away */
 pthread_mutex_lock (&schedthreadsigchildmutex);
 pthread_create (&schedthreadsigchild, NULL, sched_run_sigchild, NULL);

/* signal handlers */
 action.sa_sigaction = sched_signal_sigchld;
 sigemptyset(&(action.sa_mask));
 action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK;
// SA_NODEFER should help with a waitpid()-race... and since we don't do any locking in the handler anymore...

 if ( sigaction (SIGCHLD, &action, NULL) ) bitch (BTCH_ERRNO);

 action.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK;
 action.sa_sigaction = sched_signal_sigint;
 if ( sigaction (SIGINT, &action, NULL) ) bitch (BTCH_ERRNO);

 event_listen (EINIT_EVENT_TYPE_IPC, sched_event_handler);
}

int sched_queue (unsigned int task, void *param) {
 struct sschedule *nele;

 if (task == SCHEDULER_PID_NOTIFY) {
  pthread_cond_signal (&schedthreadsigchildcond);
  return 0;
 }

 nele = ecalloc (1, sizeof (struct sschedule));
 nele->task = task;
 nele->param = param;

 pthread_mutex_lock (&schedschedulemodmutex);
  schedule = (struct sschedule **) setadd ((void **)schedule, (void *)nele, -1);
 pthread_mutex_unlock (&schedschedulemodmutex);

 pthread_cond_signal (&schedthreadcond);
 return 0;
}

int sched_watch_pid (pid_t pid, void *(*function)(struct spidcb *)) {
 struct spidcb *nele;
 pthread_mutex_lock (&schedcpidmutex);
  if (sched_deadorphans) {
   struct spidcb *start = sched_deadorphans, *prev = NULL, *cur = start;
   for (; cur; cur = cur->next) {
    if (cur->pid == (pid_t)pid) {
     cur->cfunc = function;
     if (prev)
      prev->next = cur->next;
     else
      sched_deadorphans = cur->next;
     cur->next = cpids;
     cpids = cur;
     pthread_mutex_unlock (&schedcpidmutex);
     pthread_cond_signal (&schedthreadsigchildcond);
     return 0;
    }
    if (start != sched_deadorphans) {
     cur = sched_deadorphans;
     start = cur;
     prev = NULL;
    } else
     prev = cur;
   }
  }
  nele = ecalloc (1, sizeof (struct spidcb));
  nele->pid = pid;
  nele->cfunc = function;
  nele->dead = 0;
  nele->status = 0;
  nele->next = cpids;
 cpids = nele;
 pthread_mutex_unlock (&schedcpidmutex);
}

void *sched_run (void *p) {
 int i, l, status;
 pid_t pid;
 pthread_mutex_lock (&schedthreadmutex);
 while (1) {
  pthread_mutex_lock (&schedschedulemodmutex);
  if (schedule && schedule [0]) {
   pthread_mutex_unlock (&schedschedulemodmutex);
   struct sschedule *c = schedule[0];
   switch (c->task) {
    case SCHEDULER_SWITCH_MODE:
     sched_switchmode (c->param);
     break;
    case SCHEDULER_MOD_ACTION:
     sched_modaction ((char **)c->param);
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
   pthread_mutex_lock (&schedschedulemodmutex);
    schedule = (struct sschedule **) setdel ((void **)schedule, (void *)c);
   pthread_mutex_unlock (&schedschedulemodmutex);
   free (c);
  } else {
   pthread_mutex_unlock (&schedschedulemodmutex);
   pthread_cond_wait (&schedthreadcond, &schedthreadmutex);
  }
 }
}

void *sched_run_sigchild (void *p) {
 int i, l, status;
 pid_t pid;
 pthread_detach (schedthreadsigchild);
 int check;
 while (1) {
  pthread_mutex_lock (&schedcpidmutex);
  struct spidcb *start = cpids, *prev = NULL, *cur = start;
  check = 0;
  for (; cur; cur = cur->next) {
   pid = cur->pid;
   if (cur->dead) {
    check++;

    if (cur->cfunc) {
     pthread_t th;
     pthread_create (&th, NULL, (void *(*)(void *))cur->cfunc, (void *)cur);
     pthread_detach (th);
    }

     if (prev)
      prev->next = cur->next;
     else
      cpids = cur->next;

    break;
   }
   if (start != cpids) {
    cur = cpids;
    start = cur;
    prev = NULL;
   } else
    prev = cur;
  }
  pthread_mutex_unlock (&schedcpidmutex);
  if (!check) {
   pthread_cond_wait (&schedthreadsigchildcond, &schedthreadsigchildmutex);
  }
 }
}

/* signal handlers */

/* apparently some of the pthread*() functions aren't asnyc-safe... still it
   appears to be working, so... */

/* I came up with this pretty cheap solution to prevent deadlocks while still being able to use mutexes */
void *sched_signal_sigchld_addentrythreadfunction (struct spidcb *nele) {
 char known = 0;
 pthread_mutex_lock (&schedcpidmutex);
 struct spidcb *cur = cpids;
  for (; cur; cur = cur->next) {
   if (cur->pid == (pid_t)nele->pid) {
    known++;
    cur->status = nele->status;
    cur->dead = 1;
    free (nele);
    break;
   }
  }

  if (!known) {
   nele->cfunc = NULL;
   nele->dead = 1;

   nele->next = sched_deadorphans;
   sched_deadorphans = nele;
  }
 pthread_mutex_unlock (&schedcpidmutex);

// sched_queue (SCHEDULER_PID_NOTIFY, (void *)pid);
 pthread_cond_signal (&schedthreadsigchildcond);
}

/* this should prevent any zombies from being created */
void sched_signal_sigchld (int signal, siginfo_t *siginfo, void *context) {
 int i, status;
 pid_t pid;
 struct spidcb *nele;
 pthread_t th;

 while (pid = waitpid (-1, &status, WNOHANG)) {
  if (pid == -1) {
   break;
  }
//  fprintf (stderr, "scheduler: %i died\n", (pid_t)pid);

  nele = ecalloc (1, sizeof (struct spidcb));
  nele->pid = pid;
  nele->status = status;

  if (pthread_create (&th, NULL, (void *(*)(void *))sched_signal_sigchld_addentrythreadfunction, (void *)nele)) {
   fprintf (stdout, "couldn't create sigchld thread: %s\n", strerror (errno));
  } else
   pthread_detach (th);
 }

 return;
}

// (on linux) SIGINT to INIT means ctrl+alt+del was pressed

void sched_signal_sigint (int signal, siginfo_t *siginfo, void *context) {
#ifdef LINUX
 if (siginfo->si_code == SI_KERNEL) {
#endif
  sched_queue (SCHEDULER_SWITCH_MODE, "power-reset");
  sched_queue (SCHEDULER_POWER_RESET, NULL);
#ifdef LINUX
 }
#endif
 return;
}

void sched_event_handler(struct einit_event *event) {
 errno = 0;
 if (!event) return;
 else {
  char **argv = (char **)event->set;
  int argc = setcount ((void **)argv);
  if (!argv) {
   bitch (BTCH_ERRNO);
   return;
  } else if (!argv[0]) {
   free (argv);
   bitch (BTCH_ERRNO);
   return;
  }

  if (!strcmp (argv[0], "power") && (argc > 1)) {
   if (!strcmp (argv[1], "off")) {
    sched_queue (SCHEDULER_SWITCH_MODE, "power-off");
    sched_queue (SCHEDULER_POWER_OFF, NULL);
    write (event->integer, "request processed\n", 19);
   }
   if (!strcmp (argv[1], "reset")) {
    sched_queue (SCHEDULER_SWITCH_MODE, "power-reset");
    sched_queue (SCHEDULER_POWER_RESET, NULL);
    write (event->integer, "request processed\n", 19);
   }
  }

  if (!strcmp (argv[0], "rc") && (argc > 2)) {
   if (!strcmp (argv[1], "switch-mode")) {
    sched_queue (SCHEDULER_SWITCH_MODE, argv[2]);
    write (event->integer, "modeswitch queued\n", 19);
   } else {
    sched_queue (SCHEDULER_MOD_ACTION, (void *)setdup ((void **)argv+1, 0));
    write (event->integer, "request processed\n", 19);
   }
  }
  bitch (BTCH_ERRNO);
 }
}

