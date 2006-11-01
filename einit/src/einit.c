/*
 *  einit.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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

#ifndef NONIXENVIRON
int main(int, char **, char **);
#else
int main(int, char **);
#endif
int print_usage_info ();
int ipc_process (char *);
int ipc_wait ();
int cleanup ();

pid_t einit_sub = 0;
uint32_t check_configuration = 0;

struct cfgnode *cmode = NULL, *amode = NULL;

/* some more variables that are only of relevance to main() */
char **einit_startup_mode_switches = NULL;
char **einit_startup_configuration_files = NULL;

char einit_do_feedback_switch = 1; // whether or not to initalise the feedback mode first
char *einit_default_startup_mode_switches[] = { "default", NULL };  // the list of modes to activate by default

// the list of files to  parse by default
char *einit_default_startup_configuration_files[] =
#ifndef SANDBOX
 { "/etc/einit/einit.xml", "/etc/einit/local.xml", NULL };
#else
 { "etc/einit/einit.xml", "etc/einit/sandbox.xml", NULL };
#endif

#ifdef NONIXENVIRON
char ** environ;
#endif

int print_usage_info () {
 fputs ("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006, Magnus Deininger\n"
  "Usage:\n"
  " einit [-c <filename>] [options]\n"
  "\n"
  "Options:\n"
  "-c <filename>        load <filename> instead of/etc/einit/local.xml\n"
  "-h, --help           display this text\n"
  "-v                   print version and copyright notice, then exit\n"
  "--no-feedback-switch disable the first switch to the feedback mode\n"
  "--feedback-switch    enable the mode-switch to the feedback mode (default)\n"
  "--ipc-command        don't boot, only run specified ipc-command\n"
  "                     (you can use this more than once)\n"
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
 int i, stime;
 pid_t pid = getpid(), wpid = 0;
 char **ipccommands = NULL;

 uname (&osinfo);

/* check command line arguments */
 for (i = 1; i < argc; i++) {
  if (argv[i][0] == '-')
   switch (argv[i][1]) {
    case 'c':
     if ((++i) < argc)
      einit_default_startup_configuration_files[1] = argv[i];
     else
      return print_usage_info ();
     break;
    case 'h':
     return print_usage_info ();
     break;
    case 'v':
     puts("eINIT " EINIT_VERSION_LITERAL
          "\nThis Program is Free Software, released under the terms of this (BSD) License:\n"
          "--------------------------------------------------------------------------------\n"
          "Copyright (c) 2006, Magnus Deininger\n"
          BSDLICENSE);
     return 0;
    case '-':
     if (!strcmp(argv[i], "--check-configuration")) {
      if (pid != 1)
       check_configuration = 1;
     } else if (!strcmp(argv[i], "--no-feedback-switch"))
      einit_do_feedback_switch = 0;
     else if (!strcmp(argv[i], "--feedback-switch"))
      einit_do_feedback_switch = 1;
     else if (!strcmp(argv[i], "--help"))
      return print_usage_info ();
     else if (!strcmp(argv[i], "--ipc-command") && argv[i+1])
      ipccommands = (char **)setadd ((void **)ipccommands, (void *)argv[i+1], SET_TYPE_STRING);

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

   if (!strcmp (ed, "mode")) {
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

 if (pid == 1) einit_sub = fork();

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
     puts ("eINIT terminated by a signal...");
     exit (EXIT_FAILURE);
    }
   }
  }
 } else {
/* actual system initialisation */
  struct einit_event cev = evstaticinit(EVE_UPDATE_CONFIGURATION);

  stime = time(NULL);
  printf ("eINIT " EINIT_VERSION_LITERAL ": Initialising: %s\n", osinfo.sysname);

  if (pthread_attr_init (&thread_attribute_detached)) {
   fputs ("pthread initialisation failed.\n", stderr);
   return -1;
  } else
   pthread_attr_setdetachstate (&thread_attribute_detached, PTHREAD_CREATE_DETACHED);

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

  evstaticdestroy(cev);

  mod_scanmodules ();
//  cleanup(); return 0;
  if (ipccommands) {
   uint32_t rx = 0;
   for (; ipccommands[rx]; rx++) {
    struct einit_event *event = evinit (EVENT_SUBSYSTEM_IPC);
    uint32_t ic, ec;

    event->set = (void **)str2set (' ', ipccommands[rx]);
    event->integer = STDOUT_FILENO;
    event->flag = 0;

    ec = setcount (event->set);

    for (ic = 0; ic < ec; ic++) {
     if (!strcmp (event->set[ic], "--xml")) event->status |= EIPC_OUTPUT_XML;
     else if (!strcmp (event->set[ic], "--only-relevant")) event->status |= EIPC_ONLY_RELEVANT;
     else if (!strcmp (event->set[ic], "--help")) event->status |= EIPC_HELP;
    }

    if (event->status & EIPC_OUTPUT_XML) {
     write (STDOUT_FILENO, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<einit-ipc>\n", 52);
     event->set = (void**)strsetdel ((char**)event->set, "--xml");
    }
    if (event->status & EIPC_ONLY_RELEVANT) event->set = (void**)strsetdel ((char**)event->set, "--only-relevant");
    if (event->status & EIPC_HELP) {
     char buffer[2048];

     if (event->status & EIPC_OUTPUT_XML)
      snprintf (buffer, 2048, " <einit version=\"" EINIT_VERSION_LITERAL "\" />\n <subsystem id=\"einit-ipc\">\n  <supports option=\"--help\" description-en=\"display help\" />\n  <supports option=\"--xml\" description-en=\"request XML output\" />\n  <supports option=\"--only-relevant\" description-en=\"limit manipulation to relevant items\" />\n </subsystem>\n");
     else
      snprintf (buffer, 2048, "eINIT " EINIT_VERSION_LITERAL ": IPC Help\nGeneric Syntax:\n [function] ([subcommands]|[options])\nGeneric Options (where applicable):\n --help          display help only\n --only-relevant limit the items to be manipulated to relevant ones\n --xml           caller wishes to receive XML-formatted output\nSubsystem-Specific Help:\n");
     write (STDOUT_FILENO, buffer, strlen (buffer));

     event->set = (void**)strsetdel ((char**)event->set, "--help");
    }

    event_emit (event, EINIT_EVENT_FLAG_BROADCAST);

    if (event->set) free (event->set);

    if (!event->flag) {
     char buffer[2048];
     if (event->status & EIPC_OUTPUT_XML)
      snprintf (buffer, 2048, " <einit-ipc-error code=\"err-not-implemented\" command=\"%s\" verbose-en=\"command not implemented\" />\n", ipccommands[rx]);
     else
      snprintf (buffer, 2048, "einit: %s: command not implemented.\n", ipccommands[rx]);
     write (STDOUT_FILENO, buffer, strlen (buffer));
    }
    if (event->status & EIPC_OUTPUT_XML) {
     write (STDOUT_FILENO, "</einit-ipc>\n", 13);
    }

    evdestroy (event);
   }
  } else if (!check_configuration) {
   uint32_t e = 0;
   sched_init ();

/* queue default mode-switches */
   if (einit_do_feedback_switch)
    sched_queue (SCHEDULER_SWITCH_MODE, "feedback");

   for (e = 0; einit_startup_mode_switches[e]; e++) {
    sched_queue (SCHEDULER_SWITCH_MODE, einit_startup_mode_switches[e]);
   }

   printf ("[+%is] Done. The scheduler will now take over.\n", time(NULL)-stime);
   sched_run (NULL);
  } else {
   uint32_t errors = check_configuration -1;
   switch (errors) {
    case 0:
     puts ("\neINIT: no problems reported."); break;
    case 1:
     puts ("\neINIT: one problem reported."); break;
    default:
     printf ("\neINIT: %i problems reported.\n", errors); break;
   }

   cleanup();

   return errors; // return number of (potential) errors
  }

#ifdef SANDBOX
  cleanup ();
#endif

  return 0;
 }
}
