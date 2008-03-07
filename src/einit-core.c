/*
 *  einit-core.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <einit-modules/configuration.h>
#include <einit/configuration.h>
#include <einit/einit.h>

#include <sys/wait.h>
#include <einit/event.h>
#include <signal.h>

#include <fcntl.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <sys/mount.h>
#endif

#if defined(__linux__)
#include <sys/prctl.h>
#endif

char shutting_down = 0;
int sched_trace_target = STDOUT_FILENO;

int main(int, char **, char **);
int print_usage_info ();

pthread_key_t einit_function_macro_key;

/* some more variables that are only of relevance to main() */
char **einit_startup_mode_switches = NULL;
char **einit_startup_configuration_files = NULL;

char *einit_default_startup_mode_switches[] = { "default", NULL };  // the list of modes to activate by default

// the list of files to  parse by default
char *einit_default_startup_configuration_files[] = { EINIT_LIB_BASE "/einit.xml", NULL };

struct lmodule *mlist;

int print_usage_info () {
 eputs ("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006-2008, Magnus Deininger\n"
  "Usage:\n"
  " einit [options]\n"
  "\n"
  "Options:\n"
  "-h, --help            display this text\n"
  "-v                    print version, then exit\n"
  "-L                    print copyright notice, then exit\n"
  "\n"
  "--sandbox             run einit in \"sandbox mode\"\n"
  "\n"
  "Environment Variables (or key=value kernel parametres):\n"
  "mode=<mode>[:<mode>] a colon-separated list of modes to switch to.\n", stdout);
 return -1;
}

pthread_mutex_t core_modules_update_mutex = PTHREAD_MUTEX_INITIALIZER;

void core_einit_event_handler_update_modules (struct einit_event *ev) {
 emutex_lock (&core_modules_update_mutex);

 mod_update_source ("core");

 emutex_unlock (&core_modules_update_mutex);

/* give the module-logic code and others a chance at processing the current list */
 struct einit_event update_event = evstaticinit(einit_core_module_list_update);
 update_event.para = mlist;
 event_emit (&update_event, einit_event_flag_broadcast);
 evstaticdestroy(update_event);
}

void core_einit_event_handler_recover (struct einit_event *ev) {
 struct lmodule *lm = mlist;

 while (lm) {
  if (lm->recover) {
   lm->recover (lm);
  }

  lm = lm->next;
 }
}

void core_event_einit_boot_root_device_ok (struct einit_event *ev) {
 int e;
 fprintf (stderr, "scheduling startup switches.\n");

 for (e = 0; einit_startup_mode_switches[e]; e++) {
  struct einit_event ee = evstaticinit(einit_core_switch_mode);

  ee.string = einit_startup_mode_switches[e];
  event_emit (&ee, einit_event_flag_broadcast);
  evstaticdestroy(ee);
 }
}

void core_einit_core_module_action_complete (struct einit_event *ev) {
 if (ev->rid)
  mod_complete (ev->rid, ev->task, ev->status);
}

void einit_process_raw_event (int fd) {
 char buffer[BUFFERSIZE];
 ssize_t r;

 while (memset (buffer, 0, BUFFERSIZE), ((r = read(fd, buffer, BUFFERSIZE-1)) > 0)) {
  fprintf (stderr, "\n.\n** this is the fragment i got: %s\n.\n", buffer);

  einit_event_loop_decoder (buffer, r, NULL);
 }
}

pthread_mutex_t
  sched_timer_data_mutex = PTHREAD_MUTEX_INITIALIZER;

stack_t signalstack;

void sched_signal_sigint (int, siginfo_t *, void *);
void sched_signal_sigalrm (int, siginfo_t *, void *);
void sched_run_sigchild ();

char sigint_called = 0;

extern char shutting_down;

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

void insert_timer_event (time_t n) {
 emutex_lock (&sched_timer_data_mutex);

 uintptr_t tmpinteger = n;
 sched_timer_data = (time_t *)set_noa_add ((void **)sched_timer_data, (void *)tmpinteger);
 setsort ((void **)sched_timer_data, set_sort_order_custom, (int (*)(const void *, const void *))scheduler_compare_time);

 emutex_unlock (&sched_timer_data_mutex);
}

time_t scheduler_prune_time = 0;

time_t scheduler_get_next_tick (time_t now) {
 time_t next = 0;

 emutex_lock (&sched_timer_data_mutex);

 if (sched_timer_data) next = sched_timer_data[0];

 emutex_unlock (&sched_timer_data_mutex);

 if (next && ((next) <= (now + 60))) /* see if the event is within 60 seconds */
  return next;
 else {
  scheduler_prune_time = now + 60;

  insert_timer_event (now + 60);
  return scheduler_get_next_tick(now);
 }
}

void sched_handle_timers () {
 time_t now = time(NULL);
 time_t next_tick = scheduler_get_next_tick(now);

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

  if (next_tick == scheduler_prune_time) {
   ethread_prune_thread_pool();
  }

  sched_handle_timers();
 } else {
  if (next_tick > now) {
//   notice (1, "next timer in %i seconds\n", (next_tick - now));

   alarm (next_tick - now);
  }
 }
}

void sched_timer_event_handler_set (struct einit_event *ev) {
 insert_timer_event (ev->integer);

 sched_handle_timers();
}

#if defined(__GLIBC__)
#if ! defined(__UCLIBC__)
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
#endif

void sched_reset_event_handlers () {
 struct sigaction action;

 signalstack.ss_sp = emalloc (SIGSTKSZ);
 signalstack.ss_size = SIGSTKSZ;
 signalstack.ss_flags = 0;
 sigaltstack (&signalstack, NULL);

 sigemptyset(&(action.sa_mask));

 action.sa_sigaction = sched_signal_sigalrm;
 action.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
 if ( sigaction (SIGALRM, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

 action.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
 action.sa_sigaction = sched_signal_sigint;
 if ( sigaction (SIGINT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

 /* some signals REALLY should be ignored */
 action.sa_sigaction = (void (*)(int, siginfo_t *, void *))SIG_IGN;
 if ( sigaction (SIGTRAP, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGABRT, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

 if ( sigaction (SIGPIPE, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGIO, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTTIN, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");
 if ( sigaction (SIGTTOU, &action, NULL) ) bitch (bitch_stdio, 0, "calling sigaction() failed.");

#if 1
 /* catch a couple of signals and print traces for them */
#if defined(__GLIBC__)
#if ! defined(__UCLIBC__)
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
#endif
#endif
}

// (on linux) SIGINT to INIT means ctrl+alt+del was pressed
void sched_signal_sigint (int signal, siginfo_t *siginfo, void *context) {
 sigint_called = 1;

 return;
}

void sched_signal_sigalrm (int signal, siginfo_t *siginfo, void *context) {
/* nothing to do here... really */

 return;
}

int einit_main_loop(int ipc_pipe_fd) {
 sigset_t sigmask, osigmask;

 sigemptyset(&sigmask);
 sigaddset(&sigmask, SIGINT);
 sigaddset(&sigmask, SIGALRM);
 sigprocmask(SIG_BLOCK, &sigmask, &osigmask);

 while (1) {
  int selectres;
  if (sigint_called) {
   shutting_down = 1;
   struct einit_event ee = evstaticinit (einit_core_switch_mode);
   ee.string = "power-reset";

//    ee.para = stdout;

   event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
//  evstaticdestroy(ee);

   sigint_called = 0;
   evstaticdestroy (ee);
  }

  sched_handle_timers();

  if (ipc_pipe_fd > 0) {
   fd_set rfds;

   FD_ZERO(&rfds);
   FD_SET(ipc_pipe_fd, &rfds);

   selectres = pselect(2, &rfds, NULL, NULL, 0, &osigmask);

   if (FD_ISSET (ipc_pipe_fd, &rfds)) {
    einit_process_raw_event (ipc_pipe_fd);
   }
  } else {
   selectres = pselect(0, NULL, NULL, NULL, 0, &osigmask);
  }
 }
}

/* t3h m41n l00ps0rzZzzz!!!11!!!1!1111oneeleven11oneone11!!11 */
int main(int argc, char **argv, char **environ) {
 int i;
 int pthread_errno;
 char need_recovery = 0;
 char debug = 0;
 int command_pipe = -1;
 int crash_pipe = 0;
// char crash_threshold = 5;
 char *einit_crash_data = NULL;
 char suppress_version = 0;
 char do_wait = 0;

#if defined(__linux__) && defined(PR_SET_NAME)
 prctl (PR_SET_NAME, "einit [core]", 0, 0, 0);
#endif

 uname (&osinfo);
 config_configure();

 event_listen (einit_core_update_modules, core_einit_event_handler_update_modules);
 event_listen (einit_core_recover, core_einit_event_handler_recover);
 event_listen (einit_timer_set, sched_timer_event_handler_set);

 event_listen (einit_core_module_action_complete, core_einit_core_module_action_complete);

 if (argv) einit_argv = set_str_dup_stable (argv);

/* check command line arguments */
 for (i = 1; i < argc; i++) {
  if (argv[i][0] == '-')
   switch (argv[i][1]) {
    case 'c':
     if ((++i) < argc)
      einit_default_startup_configuration_files[0] = argv[i];
     else
      return print_usage_info ();
     break;
    case 'h':
     return print_usage_info ();
     break;
    case 's':
     suppress_version = 1;
     break;
    case 'v':
     eputs("eINIT " EINIT_VERSION_LITERAL "\n", stdout);
     return 0;
    case 'L':
     eputs("eINIT " EINIT_VERSION_LITERAL
          "\nThis Program is Free Software, released under the terms of this (BSD) License:\n"
          "--------------------------------------------------------------------------------\n"
          "Copyright (c) 2006-2008, Magnus Deininger\n"
          BSDLICENSE "\n", stdout);
     return 0;
    case '-':
     if (strmatch(argv[i], "--help"))
      return print_usage_info ();
     else if (strmatch(argv[i], "--sandbox")) {
      einit_default_startup_configuration_files[0] = EINIT_LIB_BASE "/einit.xml";

      while (einit_default_startup_configuration_files[0][0] == '/')
       einit_default_startup_configuration_files[0]++;

      coremode |= einit_mode_sandbox;
      need_recovery = 1;
     } else if (strmatch(argv[i], "--recover")) {
      need_recovery = 1;
     } else if (strmatch(argv[i], "--command-pipe")) {
      command_pipe = parse_integer (argv[i+1]);
      fcntl (command_pipe, F_SETFD, FD_CLOEXEC | O_NONBLOCK);

      i++;
     } else if (strmatch(argv[i], "--crash-pipe")) {
      crash_pipe = parse_integer (argv[i+1]);
      fcntl (crash_pipe, F_SETFD, FD_CLOEXEC);

      i++;
     } else if (strmatch(argv[i], "--crash-data")) {
      einit_crash_data = estrdup (argv[i+1]);
      i++;
     } else if (strmatch(argv[i], "--debug")) {
      debug = 1;
     } else if (strmatch(argv[i], "--do-wait")) {
      do_wait = 1;
     }

     break;
   }
 }

/* check environment */
 if (environ) {
  uint32_t e = 0;
  for (e = 0; environ[e]; e++) {
   char *ed = (char *)str_stabilise (environ[e]);
   char *lp = strchr (ed, '=');

   *lp = 0;
   lp++;

   if (strmatch (ed, "softlevel")) {
    einit_startup_mode_switches = str2set (':', lp);
   } else if (strmatch (ed, "einit")) {
/* override default configuration files and/or mode-switches with the ones in the variable einit= */
    char **tmpstrset = str2set (',', lp);
    uint32_t rx = 0;

    for (rx = 0; tmpstrset[rx]; rx++) {
     char **atom = str2set (':', tmpstrset[rx]);

     if (strmatch (atom[0], "file")) {
/* specify configuration files */
      einit_startup_configuration_files = set_str_dup_stable (atom);
      einit_startup_configuration_files = (char **)strsetdel (einit_startup_configuration_files, (void *)"file");
     } else if (strmatch (atom[0], "mode")) {
/* specify mode-switches */
      einit_startup_mode_switches = set_str_dup_stable (atom);
      einit_startup_mode_switches = (char **)strsetdel (einit_startup_mode_switches, (void *)"mode");
     }

     efree (atom);
    }

    efree (tmpstrset);
   }
  }

  einit_initial_environment = set_str_dup_stable (environ);
 }

 if (!einit_startup_mode_switches) einit_startup_mode_switches = einit_default_startup_mode_switches;
 if (!einit_startup_configuration_files) einit_startup_configuration_files = einit_default_startup_configuration_files;

 enable_core_dumps ();

 sched_reset_event_handlers();

 sched_trace_target = crash_pipe;

/* actual system initialisation */
  if (!suppress_version) {
   eprintf (stdout, "eINIT " EINIT_VERSION_LITERAL ": Initialising: %s\n", osinfo.sysname);
  }

  if ((pthread_errno = pthread_key_create(&einit_function_macro_key, NULL))) {
   bitch(bitch_epthreads, pthread_errno, "pthread_key_create(einit_function_macro_key) failed.");

   if (einit_initial_environment) efree (einit_initial_environment);
   return -1;
  }

/* this should be a good place to initialise internal modules */
   if (coremodules) {
    uint32_t cp = 0;

    if (!suppress_version)
     eputs (" >> initialising in-core modules:", stdout);

    for (; coremodules[cp]; cp++) {
     struct lmodule *lmm;
     if (!suppress_version)
      eprintf (stdout, " [%s]", (*coremodules[cp])->rid);
     lmm = mod_add(NULL, (*coremodules[cp]));

     lmm->source = (char *)str_stabilise("core");
    }

    if (!suppress_version)
     eputs (" OK\n", stdout);
   }

/* update the process environment, just in case */
  update_local_environment();

/*  struct einit_event ev = evstaticinit(einit_core_update_modules);
  event_emit (&ev, einit_event_flag_broadcast);
  evstaticdestroy (ev);*/

  /* give the module-logic code and others a chance at processing the current list */
  struct einit_event update_event = evstaticinit(einit_core_module_list_update);
  update_event.para = mlist;
  event_emit (&update_event, einit_event_flag_broadcast);
  evstaticdestroy(update_event);

  if (do_wait) {
   struct einit_event eml = evstaticinit(einit_core_secondary_main_loop);
   event_emit (&eml, einit_event_flag_broadcast);
   evstaticdestroy(eml);
  } else {
/* actual init code */
   event_listen (einit_boot_root_device_ok, core_event_einit_boot_root_device_ok);

   if (need_recovery) {
    fprintf (stderr, "need to recover from something...\n");

    struct einit_event eml = evstaticinit(einit_core_recover);
    event_emit (&eml, einit_event_flag_broadcast);
    evstaticdestroy(eml);
   }

   if (einit_crash_data) {
    fprintf (stderr, "submitting crash data...\n");

    struct einit_event eml = evstaticinit(einit_core_crash_data);
    eml.string = einit_crash_data;
    event_emit (&eml, einit_event_flag_broadcast);
    evstaticdestroy(eml);

    efree (einit_crash_data);
    einit_crash_data = NULL;
   }

   {
    fprintf (stderr, "running early bootup code...\n");

    struct einit_event eml = evstaticinit(einit_boot_early);
    event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
    evstaticdestroy(eml);
   }

   fprintf (stderr, "main loop.\n");

   return einit_main_loop(command_pipe);
  }

/* this should never be reached... */
 if (einit_initial_environment) efree (einit_initial_environment);

 fprintf (stderr, "okay, you're in trouble: I couldn't reach my main loop, or it quit\n");

 return EXIT_FAILURE;
}
