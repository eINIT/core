/*
 *  einit.c
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
#include <einit/scheduler.h>
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

#ifndef NONIXENVIRON
int main(int, char **, char **);
#else
int main(int, char **);
#endif
int print_usage_info ();
int cleanup ();

pid_t einit_sub = 0;
char isinit = 1, initoverride = 0;

struct cfgnode *cmode = NULL, *amode = NULL;
uint32_t gmode = EINIT_GMODE_INIT;
unsigned char *gdebug = 0;

/* some more variables that are only of relevance to main() */
char **einit_startup_mode_switches = NULL;
char **einit_startup_configuration_files = NULL;

char einit_do_feedback_switch = 1; // whether or not to initalise the feedback mode first
char *einit_default_startup_mode_switches[] = { "default", NULL };  // the list of modes to activate by default

// the list of files to  parse by default
char *einit_default_startup_configuration_files[] = { "/lib/einit/einit.xml", NULL };

#ifdef NONIXENVIRON
char ** environ;
#endif

char *bootstrapmodulepath = BOOTSTRAP_MODULE_PATH;

int print_usage_info () {
 eputs ("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006, 2007, Magnus Deininger\n"
  "Usage:\n"
  " einit [-c <filename>] [options]\n"
  "\n"
  "Options:\n"
  "-c <filename>         load <filename> instead of/lib/einit/einit.xml\n"
  "-h, --help            display this text\n"
  "-v                    print version and copyright notice, then exit\n"
  "--no-feedback-switch  disable the first switch to the feedback mode\n"
  "--feedback-switch     enable the mode-switch to the feedback mode (default)\n"
  "--bootstrap-modules   use this path to load bootstrap-modules\n"
  "--ipc-command         don't boot, only run specified ipc-command\n"
  "                      (you can use this more than once)\n"
  "--override-init-check einit will check if it's pid=1, override with this flag\n"
  "--check-configuration tell all modules to check for configuration errors. use this!\n"
  "--checkup, --wtf      synonymous to --check-configuration\n"
  "\n"
  "--sandbox             run einit in \"sandbox mode\"\n"
  "--metadaemon          run einit in \"metadaemon mode\"\n"
  "\n"
  "Environment Variables (or key=value kernel parametres):\n"
  "mode=<mode>[:<mode>] a colon-separated list of modes to switch to.\n", stderr);
 return -1;
}

/* cleanups are only required to check for memory leaks, OS kernels will usually
   clean up after a program terminates -- especially with an init this shouldn't be much of
   a problem, since it's THE program that doesn't terminate. */
int cleanup () {
 mod_freemodules ();
 cfg_free ();

// bitch (BTCH_DL + BTCH_ERRNO);

 if (einit_startup_mode_switches != einit_default_startup_mode_switches) {
  free (einit_startup_mode_switches);
 }

 return 0;
}

void einit_sigint (int signal, siginfo_t *siginfo, void *context) {
 kill (einit_sub, SIGINT);
}

/* t3h m41n l00ps0rzZzzz!!!11!!!1!1111oneeleven11oneone11!!11 */
#ifndef NONIXENVIRON
int main(int argc, char **argv, char **environ) {
#else
int main(int argc, char **argv) {
#endif
 int i, stime, ret = EXIT_SUCCESS;
 pid_t pid = getpid(), wpid = 0;
 char **ipccommands = NULL;
 int pthread_errno;

 uname (&osinfo);

// initialise subsystems
 ipc_configure(NULL);

// is this the system's init-process?
 isinit = getpid() == 1;

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
    case 'v':
     eputs("eINIT " EINIT_VERSION_LITERAL
          "\nThis Program is Free Software, released under the terms of this (BSD) License:\n"
          "--------------------------------------------------------------------------------\n"
          "Copyright (c) 2006, 2007, Magnus Deininger\n"
          BSDLICENSE "\n", stdout);
     return 0;
    case '-':
     if (!strcmp(argv[i], "--check-configuration") || !strcmp(argv[i], "--checkup") || !strcmp(argv[i], "--wtf")) {
      ipccommands = (char **)setadd ((void **)ipccommands, "examine configuration", SET_TYPE_STRING);
     } else if (!strcmp(argv[i], "--no-feedback-switch"))
      einit_do_feedback_switch = 0;
     else if (!strcmp(argv[i], "--feedback-switch"))
      einit_do_feedback_switch = 1;
     else if (!strcmp(argv[i], "--help"))
      return print_usage_info ();
     else if (!strcmp(argv[i], "--ipc-command") && argv[i+1])
      ipccommands = (char **)setadd ((void **)ipccommands, (void *)argv[i+1], SET_TYPE_STRING);
     else if (!strcmp(argv[i], "--override-init-check"))
      initoverride = 1;
     else if (!strcmp(argv[i], "--sandbox")) {
      einit_default_startup_configuration_files[0] = "lib/einit/einit.xml";
      gmode = EINIT_GMODE_SANDBOX;
     } else if (!strcmp(argv[i], "--metadaemon")) {
      gmode = EINIT_GMODE_METADAEMON;
     } else if (!strcmp(argv[i], "--bootstrap-modules")) {
      bootstrapmodulepath = argv[i+1];
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

   if (!strcmp (ed, "softlevel")) {
    einit_startup_mode_switches = str2set (':', lp);
   } if (!strcmp (ed, "mode")) {
/* override default mode-switches with the ones in the environment variable mode= */
    einit_startup_mode_switches = str2set (':', lp);
   } else if (!strcmp (ed, "einit")) {
/* override default configuration files and/or mode-switches with the ones in the variable einit= */
    char **tmpstrset = str2set (',', lp);
    uint32_t rx = 0;

    for (rx = 0; tmpstrset[rx]; rx++) {
     char **atom = str2set (':', tmpstrset[rx]);

     if (!strcmp (atom[0], "file")) {
/* specify configuration files */
      einit_startup_configuration_files = (char **)setdup ((void **)atom, SET_TYPE_STRING);
      einit_startup_configuration_files = (char **)strsetdel (einit_startup_configuration_files, (void *)"file");
     } else if (!strcmp (atom[0], "mode")) {
/* specify mode-switches */
      einit_startup_mode_switches = (char **)setdup ((void **)atom, SET_TYPE_STRING);
      einit_startup_mode_switches = (char **)strsetdel (einit_startup_mode_switches, (void *)"mode");
     }

     free (atom);
    }

    free (tmpstrset);
   }

   free (ed);
  }
 }

 if (!einit_startup_mode_switches) einit_startup_mode_switches = einit_default_startup_mode_switches;
 if (!einit_startup_configuration_files) einit_startup_configuration_files = einit_default_startup_configuration_files;

 if (pid == 1) {
  initoverride = 1;
  einit_sub = fork();
 }

 if (einit_sub) {
/* PID==1 part */
  int rstatus;
  struct sigaction action;

/* signal handlers */
  action.sa_sigaction = einit_sigint;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
  if ( sigaction (SIGINT, &action, NULL) ) bitch (BTCH_ERRNO);

  while (1) {
   wpid = waitpid(-1, &rstatus, 0); /* this ought to wait for ANY process */

   if (wpid == einit_sub) {
    if (WIFEXITED(rstatus))
     exit (EXIT_SUCCESS);
    if (WIFSIGNALED(rstatus)) {
     eputs ("eINIT terminated by a signal...\n", stdout);
     exit (EXIT_FAILURE);
    }
   }
  }
 } else {
/* actual system initialisation */
  struct einit_event cev = evstaticinit(EVE_UPDATE_CONFIGURATION);

  if (ipccommands && (gmode != EINIT_GMODE_SANDBOX)) {
   gmode = EINIT_GMODE_IPCONLY;
  }

  stime = time(NULL);
  eprintf (stdout, "eINIT " EINIT_VERSION_LITERAL ": Initialising: %s\n", osinfo.sysname);

  if ((pthread_errno = pthread_attr_init (&thread_attribute_detached))) {
   eputs ("pthread initialisation failed.\n", stderr);
   return -1;
  } else {
   if ((pthread_errno = pthread_attr_setdetachstate (&thread_attribute_detached, PTHREAD_CREATE_DETACHED))) {
    bitch2(BITCH_EPTHREADS, "main()", pthread_errno, "pthread_attr_setdetachstate() failed.");
   }
  }

#ifdef DO_BOOTSTRAP
   cev.type = EVE_CONFIGURATION_UPDATE;
   cev.string = NULL;
   event_emit (&cev, EINIT_EVENT_FLAG_BROADCAST);

   cev.type = EVE_UPDATE_CONFIGURATION;
#endif

/* emit events to read configuration files */
  if (einit_startup_configuration_files) {
   uint32_t rx = 0;
   for (; einit_startup_configuration_files[rx]; rx++) {
    cev.string = einit_startup_configuration_files[rx];
    event_emit (&cev, EINIT_EVENT_FLAG_BROADCAST);
   }

   if (einit_startup_configuration_files != einit_default_startup_configuration_files) {
    free (einit_startup_configuration_files);
   }
  }

  cev.string = NULL;
  cev.type = EVE_CONFIGURATION_UPDATE;

// make sure we keep updating until everything is sorted out
  while (cev.type == EVE_CONFIGURATION_UPDATE) {
   cev.type = EVE_UPDATE_CONFIGURATION;
   event_emit (&cev, EINIT_EVENT_FLAG_BROADCAST);
  }
  evstaticdestroy(cev);

  if (ipccommands) {
   uint32_t rx = 0;
   for (; ipccommands[rx]; rx++) {
    ret = ipc_process (ipccommands[rx], stdout);
   }

//   if (gmode == EINIT_GMODE_SANDBOX)
//    cleanup ();

   return ret;
  } else if ((gmode == EINIT_GMODE_INIT) && !isinit && !initoverride) {
   eputs ("WARNING: eINIT is configured to run as init, but is not the init-process (pid=1) and the --override-init-check flag was not spcified.\nexiting...\n\n", stdout);
   exit (EXIT_FAILURE);
  } else {
   uint32_t e = 0;
   sched_init ();

/* queue default mode-switches */
   if (einit_do_feedback_switch) {
    struct einit_event ee = evstaticinit(EVE_SWITCH_MODE);

    ee.string = "feedback";
//    event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST | EINIT_EVENT_FLAG_SPAWN_THREAD | EINIT_EVENT_FLAG_DUPLICATE);
    event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
    evstaticdestroy(ee);
   }

   eprintf (stderr, " >> [+%is] scheduling startup switches.\n", (int)(time(NULL)-stime));

   for (e = 0; einit_startup_mode_switches[e]; e++) {
    struct einit_event ee = evstaticinit(EVE_SWITCH_MODE);

    ee.string = einit_startup_mode_switches[e];
    event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST | EINIT_EVENT_FLAG_SPAWN_THREAD | EINIT_EVENT_FLAG_DUPLICATE);
    evstaticdestroy(ee);
   }

   sched_run_sigchild (NULL);
  }

  return ret;
 }
}
