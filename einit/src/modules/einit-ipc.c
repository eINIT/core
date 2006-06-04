/*
 *  einit-ipc.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/04/2006.
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

#define _MODULE

#include <stdio.h>
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

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"ipc", NULL};
char * requires[] = {"/", NULL};

struct smodule self = {
 EINIT_VERSION, 1, 0, 0, "eINIT IPC module", "einit-ipc", provides, requires, NULL
};

pthread_t ipc_thread;

int ipc_process (char *cmd) {
 char **argv = str2set (' ', cmd);
 int argc = setcount ((void **)argv);
 if (!argv) return bitch (BTCH_ERRNO);
 if (!argv[0]) {
  free (argv);
  return bitch (BTCH_ERRNO);
 }

 if (!strcmp (argv[0], "power") && (argc > 1)) {
  if (!strcmp (argv[1], "off")) {
   sched_queue (SCHEDULER_SWITCH_MODE, "power-off");
   sched_queue (SCHEDULER_POWER_OFF, NULL);
  }
  if (!strcmp (argv[1], "reset")) {
   sched_queue (SCHEDULER_SWITCH_MODE, "power-reset");
   sched_queue (SCHEDULER_POWER_RESET, NULL);
  }
 }

 if (!strcmp (argv[0], "rc") && (argc > 2)) {
  if (!strcmp (argv[1], "switch-mode")) {
   sched_queue (SCHEDULER_SWITCH_MODE, argv[2]);
  } else {
   sched_queue (SCHEDULER_MOD_ACTION, (void *)strsetdup (argv+1));
  }
 }

 free (argv);
}

void * ipc_wait (void *unused_parameter) {
 struct cfgnode *node = cfg_findnode ("control-socket", 0, NULL);
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
 if (!node || !node->svalue) strncpy (saddr.sun_path, "/etc/einit-control", sizeof(saddr.sun_path) - 1);
 else strncpy (saddr.sun_path, node->svalue, sizeof(saddr.sun_path) - 1);

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

/* i was originally intending to create one thread per connection, but i think one thread in total should
   be sufficient */
 while (nfd = accept (sock, NULL, NULL)) {
  if (nfd == -1) {
   if (errno == EAGAIN) continue;
   if (errno == EINTR) continue;
   if (errno == ECONNABORTED) continue;
   break;
  }
//  pthread_t *thread = ecalloc (1, sizeof (pthread_t));
//  pthread_create (thread, &threadattr, (void *(*)(void *))ipc_process, (void *)&nfd);
//  pthread_detach (*thread);
  ssize_t br;
  ssize_t ic = 0;
  ssize_t i;
  char buf[BUFFERSIZE+1];
  char lbuf[BUFFERSIZE+1];

  while (br = read (nfd, buf, BUFFERSIZE)) {
   if ((br < 0) && (errno != EAGAIN) && (errno != EINTR)) {
    bitch (BTCH_ERRNO);
    break;
   }
   for (i = 0; i < br; i++) {
    if ((buf[i] == '\n') || (buf[i] == '\0')) {
     lbuf[ic] = 0;
     if (lbuf[0])
      ipc_process (lbuf);
     ic = -1;
     lbuf[0] = 0;
	} else {
     if (ic >= BUFFERSIZE) {
      lbuf[ic] = 0;
      if (lbuf[0])
       ipc_process (lbuf);
      ic = 0;
     }
     lbuf[ic] = buf[i];
    }
    ic++;
   }
  }
  lbuf[ic] = 0;
  if (lbuf[0])
   ipc_process (lbuf);

  close (nfd);
//  close (nfd);
 }

 if (nfd == -1)
  bitch (BTCH_ERRNO);

 close (sock);
 if (unlink (saddr.sun_path)) bitch (BTCH_ERRNO);
 return NULL;
}

int enable (void *pa, struct mfeedback *status) {
 pthread_create (&ipc_thread, NULL, ipc_wait, NULL);
 return STATUS_OK;
}

int disable (void *pa, struct mfeedback *status) {
 return STATUS_OK;
}
