/*
 *  ipc.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/04/2006.
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
#include <unistd.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit-modules/scheduler.h>
#include <einit/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <einit/bitch.h>

#include <einit-modules/ipc.h>

#ifdef POSIXREGEX
#include <regex.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_ipc_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * einit_ipc_provides[] = {"ipc", NULL};
char * einit_ipc_requires[] = {"mount-system", NULL};
char * einit_ipc_before[] = {"mount-local", NULL};
const struct smodule einit_ipc_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT IPC module",
 .rid       = "einit-ipc",
 .si        = {
  .provides = einit_ipc_provides,
  .requires = einit_ipc_requires,
  .after    = NULL,
  .before   = einit_ipc_before
 },
 .configure = einit_ipc_configure
};

module_register(einit_ipc_self);

#endif

pthread_t ipc_thread;
char einit_ipc_running = 0;

int ipc_process_f (const char *cmd, FILE *f) {
 if (!cmd) return 0;

// setvbuf (f, NULL, _IONBF, 0);

 struct einit_event *event = evinit (einit_event_subsystem_ipc);
 uint32_t ic;
 int ret = 0;

 event->command = (char *)cmd;
 event->argv = str2set (' ', cmd);
 event->output = f;
 event->implemented = 0;

 event->argc = setcount ((const void **)event->argv);

 for (ic = 0; ic < event->argc; ic++) {
  if (strmatch (event->argv[ic], "--xml")) event->ipc_options |= einit_ipc_output_xml;
  else if (strmatch (event->argv[ic], "--ansi")) event->ipc_options |= einit_ipc_output_ansi;
  else if (strmatch (event->argv[ic], "--only-relevant")) event->ipc_options |= einit_ipc_only_relevant;
  else if (strmatch (event->argv[ic], "--help")) event->ipc_options |= einit_ipc_help;
  else if (strmatch (event->argv[ic], "--detach")) event->ipc_options |= einit_ipc_detach;
 }

 if (event->ipc_options & einit_ipc_output_xml) {
  eputs ("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<einit-ipc>\n", f);
  event->argv = strsetdel (event->argv, "--xml");
 }
 if (event->ipc_options & einit_ipc_only_relevant) event->argv = strsetdel (event->argv, "--only-relevant");
 if (event->ipc_options & einit_ipc_output_ansi) event->argv = strsetdel (event->argv, "--ansi");
 if (event->ipc_options & einit_ipc_help) {
  if (event->ipc_options & einit_ipc_output_xml) {
   eputs (" <einit version=\"" EINIT_VERSION_LITERAL "\" />\n <subsystem id=\"einit-ipc\">\n  <supports option=\"--help\" description-en=\"display help\" />\n  <supports option=\"--xml\" description-en=\"request XML output\" />\n  <supports option=\"--only-relevant\" description-en=\"limit manipulation to relevant items\" />\n </subsystem>\n", f);
  } else {
   eputs ("eINIT " EINIT_VERSION_LITERAL ": IPC Help\nGeneric Syntax:\n [function] ([subcommands]|[options])\nGeneric Options (where applicable):\n --help          display help only\n --only-relevant limit the items to be manipulated to relevant ones\n --xml           caller wishes to receive XML-formatted output\nSubsystem-Specific Help:\n", f);
  }

  event->argv = strsetdel ((char**)event->argv, "--help");
 }

 event_emit (event, einit_event_flag_broadcast);

 if (event->argv) free (event->argv);

 if (!event->implemented) {
  if (event->ipc_options & einit_ipc_output_xml) {
   eprintf (f, " <einit-ipc-error code=\"err-not-implemented\" command=\"%s\" verbose-en=\"command not implemented\" />\n", cmd);
  } else {
   eprintf (f, "einit-ipc: %s: command not implemented.\n", cmd);
  }

  ret = 1;
 } else
  ret = event->ipc_return;

 if (event->ipc_options & einit_ipc_output_xml) {
  eputs ("</einit-ipc>\n", f);
 }

 evdestroy (event);

#ifdef POSIXREGEX
 struct cfgnode *n = NULL;

 while ((n = cfg_findnode ("configuration-ipc-chain-command", 0, n))) {
  if (n->arbattrs) {
   uint32_t u = 0;
   regex_t pattern;
   char have_pattern = 0, *new_command = NULL;

   for (u = 0; n->arbattrs[u]; u+=2) {
    if (strmatch(n->arbattrs[u], "for")) {
     have_pattern = !eregcomp (&pattern, n->arbattrs[u+1]);
    } else if (strmatch(n->arbattrs[u], "do")) {
     new_command = n->arbattrs[u+1];
    }
   }

   if (have_pattern && new_command) {
    if (!regexec (&pattern, cmd, 0, NULL, 0))
     ipc_process_f (new_command, f);
    regfree (&pattern);
   }
  }
 }
#endif

 return ret;
}

void einit_ipc_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "examine") && strmatch(ev->argv[1], "configuration")) {
  if (!cfg_getnode("configuration-ipc-control-socket", NULL)) {
   eputs (" * configuration variable \"configuration-ipc-control-socket\" not found.\n", ev->output);
   ev->ipc_return++;
  }

  ev->implemented = 1;
 }

 if (ev->argv[0] && ev->argv[1] && ev->argv[2] && strmatch (ev->argv[0], "emit-event")) {
  struct einit_event nev = evstaticinit(event_string_to_code(ev->argv[1]));
  nev.string = ev->argv[2];
  nev.set = (void **)(ev->argv+2);

  event_emit (&nev, einit_event_flag_broadcast);

  evstaticdestroy(nev);

  ev->implemented = 1;
 }
}

int einit_ipc_cleanup (struct lmodule *this) {
 event_ignore (einit_event_subsystem_ipc, einit_ipc_ipc_event_handler);
 function_unregister ("einit-ipc-process-string", 1, ipc_process_f);

 return 0;
}

int ipc_read (int *nfd) {
 FILE *f, *r;

 int nfdc = dup(*nfd);

 if ((r = fdopen (nfdc, "r"))) {
  if ((f = fdopen (*nfd, "w")))  {
   char buffer[BUFFERSIZE];

   while ((!feof(r)) && fgets (buffer, BUFFERSIZE, r)) {
    int ret = 0;
    strtrim(buffer);

    ret = ipc_process_f (buffer, f);

    eprintf (f, "\nIPC//processed.\n%i\n", ret);
    if (fflush (f) == EOF)
     bitch(bitch_stdio, errno, "couldn't flush IPC buffer");
   }

   efclose (f);
  }
  efclose (r);
  return 0;
 }

 eclose (*nfd);
 return 0;
}

void * ipc_wait (void *unused_parameter) {
 struct cfgnode *node = cfg_getnode ("configuration-ipc-control-socket", NULL);
 int nfd;
 int sock = socket (AF_UNIX, SOCK_STREAM, 0);
 mode_t socketmode = (node && node->value ? node->value : 0600);
 struct sockaddr_un saddr;

 einit_ipc_running = 1;

 if (sock == -1) {
  perror ("einit-ipc: initialising socket");

  einit_ipc_running = 0;
  return NULL;
 }

 saddr.sun_family = AF_UNIX;
 strncpy (saddr.sun_path, (node && node->svalue ? node->svalue : "/dev/einit-control"), sizeof(saddr.sun_path) - 1);

 if (bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
  unlink (saddr.sun_path);
  if (bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
   eclose (sock);
   perror ("einit-ipc: binding socket");

   einit_ipc_running = 0;
   return NULL;
  }
 }

 if (chmod (saddr.sun_path, socketmode)) {
  perror ("einit-ipc: chmod on socket");
 }

 if (listen (sock, 5)) {
  eclose (sock);
  perror ("einit-ipc: listening on socket");

  einit_ipc_running = 0;
  return NULL;
 }

/* accept connections and spawn (detached) subthreads. */
 while ((nfd = accept (sock, NULL, NULL))) {
  if (nfd == -1) {
   if (errno == EAGAIN) continue;
   if (errno == EINTR) continue;
   if (errno == ECONNABORTED) continue;
  } else {
   pthread_t thread;
   ethread_create (&thread, &thread_attribute_detached, (void *(*)(void *))ipc_read, (void *)&nfd);
  }
 }

 if (nfd == -1)
  perror ("einit-ipc: accepting connections");

 eclose (sock);
 if (unlink (saddr.sun_path)) perror ("einit-ipc: removing socket");

 einit_ipc_running = 0;
 return NULL;
}

int einit_ipc_enable (void *pa, struct einit_event *status) {
 ethread_create (&ipc_thread, NULL, ipc_wait, NULL);
 return status_ok;
}

int einit_ipc_disable (void *pa, struct einit_event *status) {
 if (einit_ipc_running)
  ethread_cancel (ipc_thread);

 return status_ok;
}

int einit_ipc_configure (struct lmodule *irr) {
 module_init(irr);

 irr->cleanup = einit_ipc_cleanup;
 irr->enable = einit_ipc_enable;
 irr->disable = einit_ipc_disable;

 event_listen (einit_event_subsystem_ipc, einit_ipc_ipc_event_handler);
 function_register ("einit-ipc-process-string", 1, ipc_process_f);

 return 0;
}
