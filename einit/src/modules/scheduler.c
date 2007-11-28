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
 .rid       = "einit-scheduler",
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

pthread_mutex_t
 schedcpidmutex = PTHREAD_MUTEX_INITIALIZER,
 sched_timer_data_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t *signal_semaphore = NULL;

#if ((_POSIX_SEMAPHORES - 200112L) >= 0)
sem_t signal_semaphore_static;
#endif

stack_t signalstack;

struct spidcb *cpids = NULL;
struct spidcb *sched_deadorphans = NULL;

char sigint_called = 0;

extern char shutting_down;

int cleanup ();
void sched_signal_sigalrm (int signal, siginfo_t *siginfo, void *context);

time_t *sched_timer_data = NULL;

int scheduler_compare_time (time_t a, time_t b) {
 if (!a) return -1;
 if (!b) return 1;

 double d = difftime (a, b);

 if (d < 0) return 1;
 if (d > 0) return -1;
 return 0;
}

time_t scheduler_get_next_tick () {
 time_t next = 0;

 emutex_lock (&sched_timer_data_mutex);

 if (sched_timer_data) next = sched_timer_data[0];

 emutex_unlock (&sched_timer_data_mutex);

 return next;
}

void sched_handle_timers () {
 time_t next_tick = scheduler_get_next_tick();
 time_t now = time(NULL);

 if (!next_tick) {
//  notice (1, "no more timers left.\n");

  return;
 }

 if (next_tick <= now) {
//  notice (1, "next timer NAO\n");

  struct einit_event ev = evstaticinit (einit_timer_tick);

  ev.integer = next_tick;

  event_emit (&ev, einit_event_flag_broadcast);

  evstaticdestroy (ev);

  uintptr_t tmpinteger = ev.integer;
  sched_timer_data = (time_t *)setdel ((void **)sched_timer_data, (void *)tmpinteger);

  sched_handle_timers();
 } else {
  if (next_tick > now) {
//   notice (1, "next timer in %i seconds\n", (next_tick - now));

   alarm (next_tick - now);
  }
 }
}

void sched_timer_event_handler_set (struct einit_event *ev) {
 emutex_lock (&sched_timer_data_mutex);

 uintptr_t tmpinteger = ev->integer;
 sched_timer_data = (time_t *)setadd ((void **)sched_timer_data, (void *)tmpinteger, SET_NOALLOC);
 setsort ((void **)sched_timer_data, set_sort_order_custom, (int (*)(const void *, const void *))scheduler_compare_time);

/*  if (sched_timer_data) {
   uint32_t i = 0;

   notice (1, "timestamps:\n");

   for (; sched_timer_data[i]; i++) {
    notice (1, " * %i\n", (int)sched_timer_data[i]);
   }
  }*/

 emutex_unlock (&sched_timer_data_mutex);

 sched_handle_timers();
}

#ifdef __GLIBC__
#include <execinfo.h>

extern int sched_trace_target;

#define TRACE_MESSAGE_HEADER "eINIT has crashed! Please submit the following to a developer:\n --- VERSION INFORMATION ---\n eINIT, version: " EINIT_VERSION_LITERAL "\n --- END OF VERSION INFORMATION ---\n --- BACKTRACE ---\n"
#define TRACE_MESSAGE_HEADER_LENGTH sizeof(TRACE_MESSAGE_HEADER)

#define TRACE_MESSAGE_FOOTER " --- END OF BACKTRACE ---\n"
#define TRACE_MESSAGE_FOOTER_LENGTH sizeof(TRACE_MESSAGE_FOOTER)

#define TRACE_MESSAGE_FOOTER_STDOUT " --- END OF BACKTRACE ---\n\n > switching back to the default mode in 15 seconds + 5 seconds cooldown.\n"
#define TRACE_MESSAGE_FOOTER_STDOUT_LENGTH sizeof(TRACE_MESSAGE_FOOTER_STDOUT)

void sched_signal_trace (int signal, siginfo_t *siginfo, void *context) {
 void *trace[250];
 ssize_t trace_length;
 int timer = 15;

 trace_length = backtrace (trace, 250);

 if (sched_trace_target) {
//  write (sched_trace_target, TRACE_MESSAGE_HEADER, TRACE_MESSAGE_HEADER_LENGTH);
  backtrace_symbols_fd (trace, trace_length, sched_trace_target);
//  write (sched_trace_target, TRACE_MESSAGE_FOOTER, TRACE_MESSAGE_FOOTER_LENGTH);
 }

 write (STDOUT_FILENO, TRACE_MESSAGE_HEADER, TRACE_MESSAGE_HEADER_LENGTH);
 backtrace_symbols_fd (trace, trace_length, STDOUT_FILENO);
 write (STDOUT_FILENO, TRACE_MESSAGE_FOOTER_STDOUT, TRACE_MESSAGE_FOOTER_STDOUT_LENGTH);
 fsync (STDOUT_FILENO);

 while ((timer = sleep (timer)));

// raise(SIGKILL);
 _exit(einit_exit_status_die_respawn);
}
#endif

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

 action.sa_sigaction = sched_signal_sigalrm;
 action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
 if ( sigaction (SIGALRM, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

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

/* some signals REALLY should be ignored */
 action.sa_sigaction = (void (*)(int, siginfo_t *, void *))SIG_IGN;
 if ( sigaction (SIGTRAP, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGABRT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

 if ( sigaction (SIGPIPE, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGIO, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTTIN, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTTOU, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");


/* catch a couple of signals and print traces for them */
#ifdef __GLIBC__
 action.sa_sigaction = sched_signal_trace;
 action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO | SA_NODEFER;

 if ( sigaction (SIGQUIT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGABRT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGUSR1, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGUSR2, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTSTP, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTERM, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGSEGV, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
#ifdef SIGPOLL
 if ( sigaction (SIGPOLL, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
#endif
 if ( sigaction (SIGPROF, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGXCPU, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGXFSZ, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
#endif
}

int __sched_watch_pid (pid_t pid) {
 struct spidcb *nele;
 nele = ecalloc (1, sizeof (struct spidcb));
 nele->pid = pid;
 nele->cfunc = NULL;
 nele->dead = 0;
 nele->status = 0;

 emutex_lock (&schedcpidmutex);
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

void sched_ipc_event_handler (struct einit_event *ev) {
 errno = 0;
 if (!ev) return;
 else {
  if (strmatch (ev->argv[0], "power") && (ev->argc > 1)) {
   if (strmatch (ev->argv[1], "down") || strmatch (ev->argv[1], "off")) {
    shutting_down = 1;
    ev->implemented = 1;

    struct einit_event ee = evstaticinit(einit_core_switch_mode);
    ee.string = "power-down";
    event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
    eputs (" >> shutdown queued\n", ev->output);
    evstaticdestroy(ee);
   }
   if (strmatch (ev->argv[1], "reset")) {
    shutting_down = 1;
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
//      exit (EXIT_SUCCESS);
      _exit (einit_exit_status_last_rites_halt);
     }

     if (shutdownfunctionsubnames) free (shutdownfunctionsubnames);

// if we still live here, something's twocked
     eputs ("scheduler: failed, exiting\n", stderr);
     _exit (einit_exit_status_last_rites_halt);
//     exit (EXIT_FAILURE);
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
	ee.stringset = (char **)ee.set;
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

void *sched_pidthread_processor(FILE *pipe) {
 char buffer [BUFFERSIZE];
 char **message = NULL;

 do {
  while (fgets (buffer, BUFFERSIZE, pipe) != NULL) {
   if (strmatch (buffer, "\n")) { // message complete
    if (message) {
     if (message[0] && !message[1]) {
      char **command = str2set (' ', message[0]);

// parse the pid X (died|terminated) messages
      if (strmatch (command [0], "pid") && command[1] && command [2] &&
	      (strmatch (command[2], "terminated") || strmatch (command[2], "died"))) {

       struct einit_event ev = evstaticinit (einit_process_died);

       ev.integer = parse_integer (command[1]);

       event_emit (&ev, einit_event_flag_broadcast | einit_event_flag_duplicate | einit_event_flag_spawn_thread);

       evstaticdestroy(ev);
      }

      free (command);
     } else {
      char *noticebuffer = set2str ('\n', (const char **)message);

      free (noticebuffer);
     }

     free (message);
     message = NULL;
    }
   } else { // continue constructing
    strtrim(buffer);

    message = (char **)setadd ((void **)message, buffer, SET_TYPE_STRING);
   }
  }
 } while (1);

 return NULL;
}

void sched_einit_event_handler_main_loop_reached (struct einit_event *ev) {
 if (ev->file) {
  ethread_spawn_detached ((void *(*)(void *))sched_pidthread_processor, (void *)ev->file);
 }

 sched_run_sigchild(NULL);
}

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
    ee.integer = cur->pid;
    ee.status = cur->status;
    event_emit (&ee, einit_event_flag_broadcast | einit_event_flag_spawn_thread | einit_event_flag_duplicate);
    evstaticdestroy (ee);

    check++;

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

  if (einit_join_threads) {
   pthread_t thread;
   struct einit_join_thread *t = NULL;

   emutex_lock (&thread_key_detached_mutex);
   if (einit_join_threads) {
    t = einit_join_threads;

    einit_join_threads = t->next;

    thread = t->thread;
   }
   emutex_unlock (&thread_key_detached_mutex);

   if (t) {
    void **n = NULL;
    pthread_join (thread, n);

//    fprintf (stderr, "reaped thread...\n");

    check = 1;
    free (t);
   }
  }

  if (!check) {
   sched_handle_timers();

   if (!(coremode & einit_core_exiting)) sem_wait (signal_semaphore);
   else {
    debug ("scheduler SIGCHLD thread now going to sleep\n");
    while (sleep (1)) {
     debug ("still not dead...");
    }
   }
   if (sigint_called) {
    shutting_down = 1;
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

 return NULL;
}

void sched_signal_sigchld (int signal, siginfo_t *siginfo, void *context) {
 if (!(coremode & einit_core_exiting) && signal_semaphore) {
  if (sem_post (signal_semaphore)) {
   bitch(bitch_stdio, 0, "sem_post() failed.");
  }
 }

 return;
}

void sched_signal_sigalrm (int signal, siginfo_t *siginfo, void *context) {
 if (!(coremode & einit_core_exiting) && signal_semaphore) {
  if (sem_post (signal_semaphore)) {
   bitch(bitch_stdio, 0, "sem_post() failed.");
  }
 }

 return;
}

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

 event_listen (einit_timer_set, sched_timer_event_handler_set);
 event_listen (einit_core_main_loop_reached, sched_einit_event_handler_main_loop_reached);
 event_listen (einit_ipc_request_generic, sched_ipc_event_handler);

 function_register ("einit-scheduler-watch-pid", 1, __sched_watch_pid);

 sched_reset_event_handlers ();

 return 0;
}
