/*
 *  einit-core.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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
#include <einit-modules/ipc.h>
#include <einit-modules/configuration.h>
#include <einit/configuration.h>

#include <fcntl.h>

#ifdef LINUX
#include <sys/syscall.h>
#include <sys/mount.h>
#endif

#if defined(LINUX)
#include <sys/prctl.h>
#endif

char shutting_down = 0;
int sched_trace_target = STDOUT_FILENO;

int main(int, char **, char **);
int print_usage_info ();
int cleanup ();

pid_t einit_sub = 0;
char isinit = 1, initoverride = 0;

char **einit_global_environment = NULL, **einit_initial_environment = NULL, **einit_argv = NULL;
struct stree *hconfiguration = NULL;

struct cfgnode *cmode = NULL, *amode = NULL;
enum einit_mode coremode = einit_mode_init;
unsigned char *gdebug = 0;
char einit_quietness = 0;

char einit_allow_code_unloading = 0;

time_t event_snooze_time = 0;

pthread_key_t einit_function_macro_key;

/* some more variables that are only of relevance to main() */
char **einit_startup_mode_switches = NULL;
char **einit_startup_configuration_files = NULL;

char *einit_default_startup_mode_switches[] = { "default", NULL };  // the list of modes to activate by default

// the list of files to  parse by default
char *einit_default_startup_configuration_files[] = { "/lib/einit/einit.xml", NULL };

int einit_have_feedback = 0;

char *bootstrapmodulepath = BOOTSTRAP_MODULE_PATH;

struct lmodule *mlist;

int einit_task_niceness_increment = 0;
int einit_core_niceness_increment = 0;

struct einit_join_thread *einit_join_threads = NULL;

void thread_autojoin_function (void *);

int print_usage_info () {
 eputs ("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006, 2007, Magnus Deininger\n"
  "Usage:\n"
  " einit [-c <filename>] [options]\n"
  "\n"
  "Options:\n"
  "-c <filename>         load <filename> instead of/lib/einit/einit.xml\n"
  "-h, --help            display this text\n"
  "-v                    print version, then exit\n"
  "-L                    print copyright notice, then exit\n"
  "--bootstrap-modules   use this path to load bootstrap-modules\n"
  "--ipc                 don't boot, only run specified ipc-command\n"
  "                      (may be used more than once)\n"
  "--force-init          ignore PID and start as init\n"
  "\n"
  "--sandbox             run einit in \"sandbox mode\"\n"
  "--metadaemon          run einit in \"metadaemon mode\"\n"
  "\n"
  "Environment Variables (or key=value kernel parametres):\n"
  "mode=<mode>[:<mode>] a colon-separated list of modes to switch to.\n", stderr);
 return -1;
}

#if 0
/* cleanups are only required to check for memory leaks, OS kernels will usually
   clean up after a program terminates -- especially with an init this shouldn't be much of
   a problem, since it's THE program that doesn't terminate. */
int cleanup () {
 mod_freemodules ();
 config_cleanup();

// bitch (BTCH_DL + BTCH_ERRNO);

 if (einit_startup_mode_switches != einit_default_startup_mode_switches) {
  efree (einit_startup_mode_switches);
 }

 return 0;
}
#endif

void core_timer_event_handler_tick (struct einit_event *ev) {
 if (ev->integer == event_snooze_time) {
  struct lmodule *lm = mlist;
  int ok = 0;

  while (lm) {
   ok += (mod (einit_module_suspend, lm, NULL) == status_ok) ? 1 : 0;

   lm = lm->next;
  }

  if (ok)
   notice (4, "%i modules suspended", ok);

  event_snooze_time = 0;

  fprintf (stderr, "pruning thread pool...\n");
  fflush (stderr);

  ethread_prune_thread_pool ();
 }
}

void core_einit_event_handler_configuration_update (struct einit_event *ev) {
 struct cfgnode *node;
 char *str;

 ev->chain_type = einit_core_update_modules;

 if ((node = cfg_getnode ("core-mortality-bad-malloc", NULL)))
  mortality[bitch_emalloc] = node->value;

 if ((node = cfg_getnode ("core-mortality-bad-stdio", NULL)))
  mortality[bitch_stdio] = node->value;

 if ((node = cfg_getnode ("core-mortality-bad-regex", NULL)))
  mortality[bitch_regex] = node->value;

 if ((node = cfg_getnode ("core-mortality-bad-expat", NULL)))
  mortality[bitch_expat] = node->value;

 if ((node = cfg_getnode ("core-mortality-bad-dl", NULL)))
  mortality[bitch_dl] = node->value;

 if ((node = cfg_getnode ("core-mortality-bad-lookup", NULL)))
  mortality[bitch_lookup] = node->value;

 if ((node = cfg_getnode ("core-mortality-bad-pthreads", NULL)))
  mortality[bitch_epthreads] = node->value;

 if ((node = cfg_getnode ("core-settings-allow-code-unloading", NULL)))
  einit_allow_code_unloading = node->flag;

 if ((str = cfg_getstring ("core-scheduler-niceness/core", NULL)))
  einit_core_niceness_increment = parse_integer (str);

 if ((str = cfg_getstring ("core-scheduler-niceness/tasks", NULL)))
  einit_task_niceness_increment = parse_integer (str);
}

void core_einit_event_handler_update_modules (struct einit_event *ev) {
 struct lmodule *lm;

 repeat:

 lm = mlist;
 einit_new_node = 0;

 while (lm) {
  if (lm->source && strmatch(lm->source, "core")) {
   lm = mod_update (lm);

// tell module to scan for changes if it's a module-loader
   if (lm->module && (lm->module->mode & einit_module_loader) && (lm->scanmodules != NULL)) {
    notice (8, "updating modules (%s)", lm->module->rid ? lm->module->rid : "unknown");

    lm->scanmodules (mlist);

/* if an actual new node has been added to the configuration,
   repeat this step */
    if (einit_new_node) goto repeat;
   }

  }
  lm = lm->next;
 }

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

void core_einit_event_handler_suspend_all (struct einit_event *ev) {
 struct lmodule *lm = mlist;
 int ok = 0;

 while (lm) {
  ok += (mod (einit_module_suspend, lm, NULL) == status_ok) ? 1 : 0;

  lm = lm->next;
 }

 if (ok)
  notice (4, "%i modules suspended", ok);

 event_snooze_time = event_timer_register_timeout(60);
}

void core_einit_event_handler_resume_all (struct einit_event *ev) {
 struct lmodule *lm = mlist;
 int ok = 0;

 while (lm) {
  ok += (mod (einit_module_resume, lm, NULL) == status_ok) ? 1 : 0;

  lm = lm->next;
 }

 if (ok)
  notice (4, "%i available", ok);
}

/* t3h m41n l00ps0rzZzzz!!!11!!!1!1111oneeleven11oneone11!!11 */
int main(int argc, char **argv, char **environ) {
 int i, ret = EXIT_SUCCESS;
 char **ipccommands = NULL;
 int pthread_errno;
 FILE *commandpipe_in = NULL;
 char need_recovery = 0;
 char debug = 0;
 int command_pipe = 0;
 int crash_pipe = 0;
// char crash_threshold = 5;
 char *einit_crash_data = NULL;
 char suppress_version = 0;

#if defined(LINUX) && defined(PR_SET_NAME)
 prctl (PR_SET_NAME, "einit [core]", 0, 0, 0);
#endif

 boottime = time(NULL);

 uname (&osinfo);
 config_configure();

// initialise subsystems
 ipc_configure(NULL);

// is this the system's init-process?
 isinit = getpid() == 1;

 event_listen (einit_timer_tick, core_timer_event_handler_tick);
 event_listen (einit_core_configuration_update, core_einit_event_handler_configuration_update);
 event_listen (einit_core_update_modules, core_einit_event_handler_update_modules);
 event_listen (einit_core_recover, core_einit_event_handler_recover);
 event_listen (einit_core_suspend_all, core_einit_event_handler_suspend_all);
 event_listen (einit_core_resume_all, core_einit_event_handler_resume_all);

 if (argv) einit_argv = (char **)setdup ((const void **)argv, SET_TYPE_STRING);

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
          "Copyright (c) 2006, 2007, Magnus Deininger\n"
          BSDLICENSE "\n", stdout);
     return 0;
    case '-':
     if (strmatch(argv[i], "--help"))
      return print_usage_info ();
     else if (strmatch(argv[i], "--ipc") && argv[i+1])
      ipccommands = set_str_add (ipccommands, (void *)argv[i+1]);
     else if (strmatch(argv[i], "--force-init"))
      initoverride = 1;
     else if (strmatch(argv[i], "--sandbox")) {
      einit_default_startup_configuration_files[0] = "lib/einit/einit.xml";
      coremode = einit_mode_sandbox;
      need_recovery = 1;
      initoverride = 1;
     } else if (strmatch(argv[i], "--recover")) {
      need_recovery = 1;
     } else if (strmatch(argv[i], "--metadaemon")) {
      coremode = einit_mode_metadaemon;
     } else if (strmatch(argv[i], "--bootstrap-modules")) {
      bootstrapmodulepath = argv[i+1];
     } else if (strmatch(argv[i], "--command-pipe")) {
      command_pipe = parse_integer (argv[i+1]);
      fcntl (command_pipe, F_SETFD, FD_CLOEXEC);

      i++;
      initoverride = 1;
     } else if (strmatch(argv[i], "--crash-pipe")) {
      crash_pipe = parse_integer (argv[i+1]);
      fcntl (crash_pipe, F_SETFD, FD_CLOEXEC);

      i++;
      initoverride = 1;
     } else if (strmatch(argv[i], "--crash-data")) {
      einit_crash_data = estrdup (argv[i+1]);
      i++;
      initoverride = 1;
     } else if (strmatch(argv[i], "--debug")) {
      debug = 1;
     }

     break;
   }
 }

/* check environment */
 if (environ) {
  uint32_t e = 0;
  for (e = 0; environ[e]; e++) {
   char *ed = estrdup (environ[e]);
   char *lp = strchr (ed, '=');

   *lp = 0;
   lp++;

   if (strmatch (ed, "softlevel")) {
    einit_startup_mode_switches = str2set (':', lp);
   } if (strmatch (ed, "mode")) {
/* override default mode-switches with the ones in the environment variable mode= */
    einit_startup_mode_switches = str2set (':', lp);
   } else if (strmatch (ed, "einit")) {
/* override default configuration files and/or mode-switches with the ones in the variable einit= */
    char **tmpstrset = str2set (',', lp);
    uint32_t rx = 0;

    for (rx = 0; tmpstrset[rx]; rx++) {
     char **atom = str2set (':', tmpstrset[rx]);

     if (strmatch (atom[0], "file")) {
/* specify configuration files */
      einit_startup_configuration_files = (char **)setdup ((const void **)atom, SET_TYPE_STRING);
      einit_startup_configuration_files = (char **)strsetdel (einit_startup_configuration_files, (void *)"file");
     } else if (strmatch (atom[0], "mode")) {
/* specify mode-switches */
      einit_startup_mode_switches = (char **)setdup ((const void **)atom, SET_TYPE_STRING);
      einit_startup_mode_switches = (char **)strsetdel (einit_startup_mode_switches, (void *)"mode");
     } else if (strmatch (atom[0], "stfu")) {
      einit_quietness = 3;
     } else if (strmatch (atom[0], "silent")) {
      einit_quietness = 2;
     } else if (strmatch (atom[0], "quiet")) {
      einit_quietness = 1;
     }

     efree (atom);
    }

    efree (tmpstrset);
   }

   efree (ed);
  }

  einit_initial_environment = (char **)setdup ((const void **)environ, SET_TYPE_STRING);
 }

 if (!einit_startup_mode_switches) einit_startup_mode_switches = einit_default_startup_mode_switches;
 if (!einit_startup_configuration_files) einit_startup_configuration_files = einit_default_startup_configuration_files;

 enable_core_dumps ();

 sched_trace_target = crash_pipe;

  if (command_pipe) { // commandpipe[0]
   commandpipe_in = fdopen (command_pipe, "r");
  }

/* actual system initialisation */
  struct einit_event cev = evstaticinit(einit_core_update_configuration);

  if (ipccommands && (coremode != einit_mode_sandbox)) {
   coremode = einit_mode_ipconly;
  }

  if (!suppress_version) {
   eprintf (stderr, "eINIT " EINIT_VERSION_LITERAL ": Initialising: %s\n", osinfo.sysname);
  }

  if ((pthread_errno = pthread_attr_init (&thread_attribute_detached))) {
   bitch(bitch_epthreads, pthread_errno, "pthread_attr_init() failed.");

   if (einit_initial_environment) efree (einit_initial_environment);
   return -1;
  } else {
   if ((pthread_errno = pthread_attr_setdetachstate (&thread_attribute_detached, PTHREAD_CREATE_DETACHED))) {
    bitch(bitch_epthreads, pthread_errno, "pthread_attr_setdetachstate() failed.");
   }
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
     eputs (" >> initialising in-core modules:", stderr);

    for (; coremodules[cp]; cp++) {
     struct lmodule *lmm;
     if (!suppress_version)
      eprintf (stderr, " [%s]", (*coremodules[cp])->rid);
     lmm = mod_add(NULL, (*coremodules[cp]));

     lmm->source = estrdup("core");
    }

    if (!suppress_version)
     eputs (" OK\n", stderr);
   }

/* emit events to read configuration files */
  if (einit_startup_configuration_files) {
   uint32_t rx = 0;
   for (; einit_startup_configuration_files[rx]; rx++) {
    cev.string = einit_startup_configuration_files[rx];
    event_emit (&cev, einit_event_flag_broadcast);
   }

   if (einit_startup_configuration_files != einit_default_startup_configuration_files) {
    efree (einit_startup_configuration_files);
   }
  }

  cev.string = NULL;
  cev.type = einit_core_configuration_update;

// make sure we keep updating until everything is sorted out
  while (cev.type == einit_core_configuration_update) {
//   notice (2, "stuff changed, updating configuration.");

   cev.type = einit_core_update_configuration;
   event_emit (&cev, einit_event_flag_broadcast);
  }
  evstaticdestroy(cev);

  if (ipccommands) {
   uint32_t rx = 0;
   for (; ipccommands[rx]; rx++) {
    ret = ipc_process (ipccommands[rx], stdout);
   }

//   if (gmode == EINIT_GMODE_SANDBOX)
//    cleanup ();

   efree (ipccommands);
   if (einit_initial_environment) efree (einit_initial_environment);
   return ret;
  } else if ((coremode == einit_mode_init) && !isinit && !initoverride) {
   eputs ("WARNING: eINIT is configured to run as init, but is not the init-process (pid=1) and the --override-init-check flag was not specified.\nexiting...\n\n", stderr);
   exit (EXIT_FAILURE);
  } else {
/* actual init code */
   uint32_t e = 0;

   nice (einit_core_niceness_increment);

   if (need_recovery) {
    notice (1, "need to recover from something...");

    struct einit_event eml = evstaticinit(einit_core_recover);
    event_emit (&eml, einit_event_flag_broadcast);
    evstaticdestroy(eml);
   }

   if (einit_crash_data) {
    notice (1, "submitting crash data...");

    struct einit_event eml = evstaticinit(einit_core_crash_data);
    eml.string = einit_crash_data;
    event_emit (&eml, einit_event_flag_broadcast);
    evstaticdestroy(eml);

    efree (einit_crash_data);
    einit_crash_data = NULL;
   }

   {
    notice (3, "running early bootup code...");

    struct einit_event eml = evstaticinit(einit_boot_early);
    event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
    evstaticdestroy(eml);
   }

   notice (2, "scheduling startup switches.\n");

   for (e = 0; einit_startup_mode_switches[e]; e++) {
    struct einit_event ee = evstaticinit(einit_core_switch_mode);

    ee.string = einit_startup_mode_switches[e];
    event_emit (&ee, einit_event_flag_broadcast | einit_event_flag_spawn_thread | einit_event_flag_duplicate);
    evstaticdestroy(ee);
   }

   struct einit_event eml = evstaticinit(einit_core_main_loop_reached);
   eml.file = commandpipe_in;
   event_emit (&eml, einit_event_flag_broadcast);
   evstaticdestroy(eml);
  }

  if (einit_initial_environment) efree (einit_initial_environment);
  return ret;

/* this should never be reached... */
 if (einit_initial_environment) efree (einit_initial_environment);
 return 0;
}
