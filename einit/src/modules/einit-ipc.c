/*
 *  einit-ipc.c
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

#define _MODULE

#include <stdio.h>
#include <unistd.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/scheduler.h>
#include <einit/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <einit-modules/ipc.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"ipc", NULL};
char * requires[] = {"mount/system", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= 0,
	.options	= 0,
	.name		= "eINIT IPC module",
	.rid		= "einit-ipc",
    .si           = {
        .provides = provides,
        .requires = requires,
        .after    = NULL,
        .before   = NULL
    }
};

pthread_t ipc_thread;

int __ipc_process (char *cmd, uint32_t fd) {
 if (!cmd) return 0;
 else if (!strcmp (cmd, "IPC//out")) {
  return 1;
 } else {
  struct einit_event *event = evinit (EVENT_SUBSYSTEM_IPC);
  uint32_t ic, ec;

  event->string = cmd;
  event->set = (void **)str2set (' ', cmd);
  event->integer = fd;
  event->flag = 0;

  ec = setcount (event->set);

  for (ic = 0; ic < ec; ic++) {
   if (!strcmp (event->set[ic], "--xml")) event->status |= EIPC_OUTPUT_XML;
   else if (!strcmp (event->set[ic], "--only-relevant")) event->status |= EIPC_ONLY_RELEVANT;
   else if (!strcmp (event->set[ic], "--help")) event->status |= EIPC_HELP;
   else if (!strcmp (event->set[ic], "--detach")) event->status |= EIPC_DETACH;
  }

  if (event->status & EIPC_OUTPUT_XML) {
   write (fd, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<einit-ipc>\n", 52);
   event->set = (void**)strsetdel ((char**)event->set, "--xml");
  }
  if (event->status & EIPC_ONLY_RELEVANT) event->set = (void**)strsetdel ((char**)event->set, "--only-relevant");
  if (event->status & EIPC_HELP) {
   char buffer[2048];

   if (event->status & EIPC_OUTPUT_XML)
    snprintf (buffer, 2048, " <einit version=\"" EINIT_VERSION_LITERAL "\" />\n <subsystem id=\"einit-ipc\">\n  <supports option=\"--help\" description-en=\"display help\" />\n  <supports option=\"--xml\" description-en=\"request XML output\" />\n  <supports option=\"--only-relevant\" description-en=\"limit manipulation to relevant items\" />\n </subsystem>\n");
   else
    snprintf (buffer, 2048, "eINIT " EINIT_VERSION_LITERAL ": IPC Help\nGeneric Syntax:\n [function] ([subcommands]|[options])\nGeneric Options (where applicable):\n --help          display help only\n --only-relevant limit the items to be manipulated to relevant ones\n --xml           caller wishes to receive XML-formatted output\nSubsystem-Specific Help:\n");
   write (fd, buffer, strlen (buffer));

   event->set = (void**)strsetdel ((char**)event->set, "--help");
  }

  event_emit (event, EINIT_EVENT_FLAG_BROADCAST);

  if (event->set) free (event->set);

  if (!event->flag) {
   char buffer[2048];
   if (event->status & EIPC_OUTPUT_XML)
    snprintf (buffer, 2048, " <einit-ipc-error code=\"err-not-implemented\" command=\"%s\" verbose-en=\"command not implemented\" />\n", cmd);
   else
    snprintf (buffer, 2048, "einit-ipc: %s: command not implemented.\n", cmd);
   write (fd, buffer, strlen (buffer));
  }
  if (event->status & EIPC_OUTPUT_XML) {
   write (fd, "</einit-ipc>\n", 13);
  }

  evdestroy (event);
  return 0;
 }
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-ipc-control-socket", NULL)) {
   fdputs (" * configuration variable \"configuration-ipc-control-socket\" not found.\n", ev->integer);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int configure (struct lmodule *irr) {
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 function_register ("einit-ipc-process-string", 1, __ipc_process);
}

int cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 function_unregister ("einit-ipc-process-string", 1, __ipc_process);
}

int ipc_read (int *nfd) {
 ssize_t br;
 ssize_t ic = 0;
 ssize_t i;
 char buf[BUFFERSIZE+1];
 char lbuf[BUFFERSIZE+1];

 while (br = read (*nfd, buf, BUFFERSIZE)) {
  if ((br < 0) && (errno != EAGAIN) && (errno != EINTR)) {
   bitch (BTCH_ERRNO);
   break;
  }
  for (i = 0; i < br; i++) {
   if ((buf[i] == '\n') || (buf[i] == '\0')) {
    lbuf[ic] = 0;
    if (lbuf[0] && __ipc_process (lbuf, *nfd)) goto close_connection;
    ic = -1;
    lbuf[0] = 0;
   } else {
    if (ic >= BUFFERSIZE) {
     lbuf[ic] = 0;
     if (lbuf[0] && __ipc_process (lbuf, *nfd)) goto close_connection;
     ic = 0;
    }
    lbuf[ic] = buf[i];
   }
   ic++;
  }
 }
 lbuf[ic] = 0;
 if (lbuf[0])
  __ipc_process (lbuf, *nfd);

 close_connection:
 close (*nfd);
}

void * ipc_wait (void *unused_parameter) {
 struct cfgnode *node = cfg_getnode ("configuration-ipc-control-socket", NULL);
 int nfd;
 pthread_t **cthreads;
 int sock = socket (AF_UNIX, SOCK_STREAM, 0);
 mode_t socketmode = (node && node->value ? node->value : 0600);
 struct sockaddr_un saddr;
 if (sock == -1) {
  bitch (BTCH_ERRNO);
  return NULL;
 }

 saddr.sun_family = AF_UNIX;
 strncpy (saddr.sun_path, (node && node->svalue ? node->svalue : "/etc/einit-control"), sizeof(saddr.sun_path) - 1);

 if (bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
  unlink (saddr.sun_path);
  if (bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
   close (sock);
   bitch (BTCH_ERRNO);
   return NULL;
  }
 }

 if (chmod (saddr.sun_path, socketmode)) {
  bitch (BTCH_ERRNO);
 }

 if (listen (sock, 5)) {
  close (sock);
  bitch (BTCH_ERRNO);
  return NULL;
 }

/* accept connections and spawn (detached) subthreads. */
 while (nfd = accept (sock, NULL, NULL)) {
  if (nfd == -1) {
   if (errno == EAGAIN) continue;
   if (errno == EINTR) continue;
   if (errno == ECONNABORTED) continue;
  } else {
   pthread_t thread;
   pthread_create (&thread, &thread_attribute_detached, (void *(*)(void *))ipc_read, (void *)&nfd);
//   puts ("new thread created.");
  }
 }

 if (nfd == -1)
  bitch (BTCH_ERRNO);

 close (sock);
 if (unlink (saddr.sun_path)) bitch (BTCH_ERRNO);
 return NULL;
}

int enable (void *pa, struct einit_event *status) {
 pthread_create (&ipc_thread, NULL, ipc_wait, NULL);
 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 pthread_cancel (ipc_thread);
 return STATUS_OK;
}
