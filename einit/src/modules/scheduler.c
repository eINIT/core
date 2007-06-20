/*
 *  scheduler.c
 *  einit
 *
 *  Created by Magnus Deininger on 02/05/2006.
 *  Renamed from scheduler.c on 03/19/2007.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <einit/event.h>
#include <signal.h>
#include <errno.h>
#include <einit/module-logic.h>
#include <semaphore.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <einit-modules/scheduler.h>

int einit_scheduler_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_scheduler_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT scheduler",
 .rid       = "scheduler",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_scheduler_configure
};

module_register(einit_scheduler_self);

#endif

pthread_mutex_t schedcpidmutex = PTHREAD_MUTEX_INITIALIZER;

sem_t *signal_semaphore = NULL;

#if ((_POSIX_SEMAPHORES - 200112L) >= 0)
sem_t signal_semaphore_static;
#endif

stack_t signalstack;

struct spidcb *cpids = NULL;
struct spidcb *sched_deadorphans = NULL;

char sigint_called = 0;

int cleanup ();

int scheduler_cleanup () {
 sem_t *sembck = signal_semaphore;
 stack_t curstack;
 signal_semaphore = NULL;

 if (!sigaltstack (NULL, &curstack) && !(curstack.ss_flags & SS_ONSTACK)) {
  curstack.ss_size = SIGSTKSZ;
  curstack.ss_flags = SS_DISABLE;
  sigaltstack (&curstack, NULL);
//  free (curstack.ss_sp);
 } else {
  notice (1, "schedule: no alternate signal stack or alternate stack in use; not cleaning up");
 }

#if ((_POSIX_SEMAPHORES - 200112L) >= 0)
 sem_destroy (sembck);
// free (sembck);
#elif defined(DARWIN)
 sem_close (sembck);
#else
 if (sem_destroy (sembck))
  sem_close (sembck);
#endif

 return 0;
}

void sched_reset_event_handlers () {
 struct sigaction action;

 signalstack.ss_sp = emalloc (SIGSTKSZ);
 signalstack.ss_size = SIGSTKSZ;
 signalstack.ss_flags = 0;
 sigaltstack (&signalstack, NULL);

 sigemptyset(&(action.sa_mask));

/* signal handlers */
 action.sa_sigaction = sched_signal_sigchld;
// action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK;
 action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
// SA_NODEFER should help with a waitpid()-race... and since we don't do any locking in the handler anymore...
 if ( sigaction (SIGCHLD, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

 action.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK;
 action.sa_sigaction = sched_signal_sigint;
 if ( sigaction (SIGINT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

#if 0
/* ignore most signals */
 action.sa_sigaction = (void (*)(int, siginfo_t *, void *))SIG_IGN;

 if ( sigaction (SIGQUIT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGABRT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
// if ( sigaction (SIGALRM, &action, NULL) ) bitch (BTCH_ERRNO);
 if ( sigaction (SIGUSR1, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGUSR2, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTSTP, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTERM, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
#ifdef SIGPOLL
 if ( sigaction (SIGPOLL, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
#endif
 if ( sigaction (SIGPROF, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
// if ( sigaction (SIGVTALRM, &action, NULL) ) bitch (BITCH_STDIO, 0, "calling sigaction() failed.");
 if ( sigaction (SIGXCPU, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGXFSZ, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
#ifdef SIGIO
#endif
#endif

 if ( sigaction (SIGPIPE, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGIO, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTTIN, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTTOU, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");


}

int __sched_watch_pid (pid_t pid, void *(*function)(struct spidcb *)) {
 struct spidcb *nele;
 emutex_lock (&schedcpidmutex);
#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
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
     emutex_unlock (&schedcpidmutex);
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
#endif
  nele = ecalloc (1, sizeof (struct spidcb));
  nele->pid = pid;
  nele->cfunc = function;
  nele->dead = 0;
  nele->status = 0;
  nele->next = cpids;
 cpids = nele;

 emutex_unlock (&schedcpidmutex);
 if (!(coremode & einit_core_exiting) && signal_semaphore) {
  if (sem_post (signal_semaphore)) {
   bitch(bitch_stdio, 0, "sem_post() failed.");
  }
 }

 return 0;
}

// (on linux) SIGINT to INIT means ctrl+alt+del was pressed
void sched_signal_sigint (int signal, siginfo_t *siginfo, void *context) {
#ifdef LINUX
/* only shut down if the SIGINT was sent by the kernel, (e)INIT (process 1) or by the parent process */
 if ((siginfo->si_code == SI_KERNEL) ||
     (((siginfo->si_code == SI_USER) && (siginfo->si_pid == 1)) || (siginfo->si_pid == getppid())))
#else
/* only shut down if the SIGINT was sent by process 1 or by the parent process */
/* if ((siginfo->si_pid == 1) || (siginfo->si_pid == getppid())) */
// note: this relies on a proper pthreads implementation so... i deactivated it for now.
#endif
 {
  sigint_called = 1;
  if (!(coremode & einit_core_exiting) && signal_semaphore)
   sem_post (signal_semaphore);

 }
 return;
}

void sched_ipc_event_handler(struct einit_event *ev) {
 errno = 0;
 if (!ev) return;
 else {
  if (strmatch (ev->argv[0], "power") && (ev->argc > 1)) {
   if (strmatch (ev->argv[1], "down") || strmatch (ev->argv[1], "off")) {
    ev->implemented = 1;

    struct einit_event ee = evstaticinit(einit_core_switch_mode);
    ee.string = "power-down";
    event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
    eputs (" >> shutdown queued\n", ev->output);
    evstaticdestroy(ee);
   }
   if (strmatch (ev->argv[1], "reset")) {
    ev->implemented = 1;

    struct einit_event ee = evstaticinit(einit_core_switch_mode);
    ee.string = "power-reset";
    event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
    eputs (" >> reset queued\n", ev->output);
    evstaticdestroy(ee);
   }
  }

/* actual power-down/power-reset IPC commands */
  if (strmatch (ev->argv[0], "scheduler") && (ev->argc > 1)) {
   char reset = 0;
   if (strmatch (ev->argv[1], "power-down") || (reset = strmatch (ev->argv[1], "power-reset"))) {
    ev->implemented = 1;

     notice (1, "scheduler: sync()-ing");

     sync ();

     if (coremode == einit_mode_sandbox) {
      notice (1, "scheduler: cleaning up");
     }

     coremode |= einit_core_exiting;
     if (signal_semaphore) {
      if (sem_post (signal_semaphore)) {
       bitch(bitch_stdio, 0, "sem_post() failed.");
      }
     }

     const char **shutdownfunctionsubnames = (const char **)str2set (':', cfg_getstring ("core-scheduler-shutdown-function-suffixes", NULL));

     void  ((**reset_functions)()) = (void (**)())
      (shutdownfunctionsubnames ? function_find((reset ? "core-power-reset" : "core-power-off"), 1, shutdownfunctionsubnames) : NULL);

     eputs ((reset ? "scheduler: reset\n" : "scheduler: power down\n"), stderr);

     if (reset_functions) {
      uint32_t xn = 0;

      for (; reset_functions[xn]; xn++) {
       (reset_functions[xn]) ();
      }
     } else {
      eputs ("scheduler: no (accepted) functions found, exiting\n", stderr);
      exit (EXIT_SUCCESS);
     }

     if (shutdownfunctionsubnames) free (shutdownfunctionsubnames);

// if we still live here, something's twocked
     eputs ("scheduler: failed, exiting\n", stderr);
     exit (EXIT_FAILURE);
   }
  }

  if (strmatch (ev->argv[0], "rc") && (ev->argc > 2)) {
   ev->implemented = 1;

   if (strmatch (ev->argv[1], "switch-mode")) {
    struct einit_event ee = evstaticinit(einit_core_switch_mode);
    ee.string = ev->argv[2];
    if (ev->ipc_options & einit_ipc_detach) {
     event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
     eputs (" >> modeswitch queued\n", ev->output);
    } else {
     ee.output = ev->output;
     ee.ipc_options = ev->ipc_options;
     event_emit (&ee, einit_event_flag_broadcast);
     ev->ipc_return = ee.integer;
    }
    evstaticdestroy(ee);
   } else {
    struct einit_event ee = evstaticinit(einit_core_change_service_status);
    ee.set = (void **)setdup ((const void **)ev->argv+1, SET_TYPE_STRING);
    if (ev->ipc_options & einit_ipc_detach) {
     event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
     eputs (" >> status change queued\n", ev->output);
    } else {
     ee.output = ev->output;
     ee.ipc_options = ev->ipc_options;
     event_emit (&ee, einit_event_flag_broadcast);
     ev->ipc_return = ee.integer;
    }
    evstaticdestroy(ee);
   }
  }

  if (errno) {
#ifdef DEBUG
   perror ("sched_ipc_event_handler: cleanup sanity check");
#endif
   errno = 0;
  }
 }
}

void sched_einit_event_handler(struct einit_event *ev) {
 if (ev->type == einit_core_main_loop_reached) {
  sched_run_sigchild(NULL);
 }
}

/* BUG: linux pthread-libraries on kernel <= 2.4 can not wait on other threads' children, thus
        when doing a ./configure, you have to specify the option --pthread-wait-bug. these functions
        are buggy too, though, and subject to racing bugs. */
#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
void *sched_run_sigchild (void *p) {
 int i, l, status;
 pid_t pid;
 int check;
 while (1) {
  emutex_lock (&schedcpidmutex);
  struct spidcb *start = cpids, *prev = NULL, *cur = start;
  check = 0;
  for (; cur; cur = cur->next) {
   pid = cur->pid;
   if (cur->dead) {
    struct einit_event ee = evstaticinit(einit_process_died);
    ee.source_pid = cur->pid;
    event_emit (&ee, einit_event_flag_broadcast | einit_event_flag_spawn_thread | einit_event_flag_duplicate);
    evstaticdestroy (ee);

    check++;

    if (cur->cfunc) {
     pthread_t th;
     ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))cur->cfunc, (void *)cur);
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
  emutex_unlock (&schedcpidmutex);
  if (!check) {
   sem_wait (signal_semaphore);
   if (coremode & einit_core_exiting) {
    return NULL;
   }

   if (sigint_called) {
    struct einit_event ee;
    ee.type = einit_core_switch_mode;
    ee.string = "power-reset";

//    ee.para = stdout;

    event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
//  evstaticdestroy(ee);

    sigint_called = 0;
   }
  }
 }
}

/* signal handlers */

/* apparently some of the pthread*() functions aren't asnyc-safe... still it
   appears to be working, so... */

/* I came up with this pretty cheap solution to prevent deadlocks while still being able to use mutexes */
void *sched_signal_sigchld_addentrythreadfunction (struct spidcb *nele) {
 char known = 0;
 emutex_lock (&schedcpidmutex);
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
 emutex_unlock (&schedcpidmutex);

 if (sem_post (signal_semaphore)) {
  bitch(bitch_stdio, 0, "sem_post() failed.");
 }
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

  nele = ecalloc (1, sizeof (struct spidcb));
  nele->pid = pid;
  nele->status = status;

  ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))sched_signal_sigchld_addentrythreadfunction, (void *)nele);
 }

 return;
}

#else
void *sched_run_sigchild (void *p) {
 int status, check;
 pid_t pid;
 while (1) {
  emutex_lock (&schedcpidmutex);
  struct spidcb *start = cpids, *prev = NULL, *cur = start;
  check = 0;

  for (; cur; cur = cur->next) {
   pid = cur->pid;
   if ((!cur->dead) && (waitpid (pid, &status, WNOHANG) > 0)) {
    if (WIFEXITED(status) || WIFSIGNALED(status)) cur->dead = 1;
   }

   if (cur->dead) {
    struct einit_event ee = evstaticinit(einit_process_died);
    ee.source_pid = cur->pid;
    event_emit (&ee, einit_event_flag_broadcast | einit_event_flag_spawn_thread | einit_event_flag_duplicate);
    evstaticdestroy (ee);

    check++;
    if (cur->cfunc) {
     pthread_t th;
     ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))cur->cfunc, (void *)cur);
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

  emutex_unlock (&schedcpidmutex);
  if (!check) {
   if (!(coremode & einit_core_exiting)) sem_wait (signal_semaphore);
   else {
    debug ("scheduler SIGCHLD thread now going to sleep\n");
    while (sleep (1)) {
     debug ("still not dead...");
    }
   }
   if (sigint_called) {
    debug ("scheduler SIGCHLD thread: making eINIT shut down\n");

    struct einit_event ee = evstaticinit (einit_core_switch_mode);
    ee.string = "power-reset";

//    ee.para = stdout;

    event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
//  evstaticdestroy(ee);

    sigint_called = 0;
    evstaticdestroy (ee);
   }
  }
 }
}

void sched_signal_sigchld (int signal, siginfo_t *siginfo, void *context) {
 if (!(coremode & einit_core_exiting) && signal_semaphore) {
  if (sem_post (signal_semaphore)) {
   bitch(bitch_stdio, 0, "sem_post() failed.");
  }
 }

 return;
}
#endif

int einit_scheduler_configure (struct lmodule *tm) {
 module_init(tm);

#if ((_POSIX_SEMAPHORES - 200112L) >= 0)
 signal_semaphore = &signal_semaphore_static;
 sem_init (signal_semaphore, 0, 0);
#elif defined(DARWIN)
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "/einit-sgchld-sem-%i", getpid());

 if ((int)(signal_semaphore = sem_open (tmp, O_CREAT, O_RDWR, 0)) == SEM_FAILED) {
  perror ("scheduler: semaphore setup");
  exit (EXIT_FAILURE);
 }
#else
#warning no proper or recognised semaphores implementation, i can not promise this code will work.
 /* let's just hope for the best... */
 char tmp[BUFFERSIZE];

 signal_semaphore = ecalloc (1, sizeof (sem_t));
 if (sem_init (signal_semaphore, 0, 0) == -1) {
  free (signal_semaphore);
  esprintf (tmp, BUFFERSIZE, "/einit-sigchild-semaphore-%i", getpid());

  if ((signal_semaphore = sem_open (tmp, O_CREAT, O_RDWR, 0)) == SEM_FAILED) {
   perror ("scheduler: semaphore setup");
   exit (EXIT_FAILURE);
  }
 }
#endif

 event_listen (einit_event_subsystem_core, sched_einit_event_handler);
 event_listen (einit_event_subsystem_ipc, sched_ipc_event_handler);

 function_register ("einit-scheduler-watch-pid", 1, __sched_watch_pid);

// pthread_create (&schedthreadsigchild, &thread_attribute_detached, sched_run_sigchild, NULL);

 sched_reset_event_handlers ();

 return 0;
}
