/*
 *  libeinit-ipc-socket.c
 *  einit
 *
 *  Created by Magnus Deininger on 28/10/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

#include <einit/einit.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <expat.h>

#include <sys/socket.h>
#include <sys/un.h>

#ifdef DARWIN
/* dammit, what's wrong with macos!? */

struct exported_function *cfg_addnode_fs = NULL;
struct exported_function *cfg_findnode_fs = NULL;
struct exported_function *cfg_getstring_fs = NULL;
struct exported_function *cfg_getnode_fs = NULL;
struct exported_function *cfg_filter_fs = NULL;
struct exported_function *cfg_getpath_fs = NULL;
struct exported_function *cfg_prefix_fs = NULL;

struct cfgnode *cmode = NULL, *amode = NULL;
char *bootstrapmodulepath = NULL;
time_t boottime = 0;
enum einit_mode coremode = 0;
const struct smodule **coremodules[MAXMODULES] = { NULL };
char **einit_initial_environment = NULL;
char **einit_global_environment = NULL;
struct spidcb *cpids = NULL;
int einit_have_feedback = 1;
struct stree *service_aliases = NULL;
struct stree *service_usage = NULL;
char einit_new_node = 0;
struct event_function *event_functions = NULL;
struct stree *exported_functions = NULL;
unsigned char *gdebug = 0;
struct stree *hconfiguration = NULL;
struct utsname osinfo = {};
pthread_attr_t thread_attribute_detached = {};
struct spidcb *sched_deadorphans = NULL;
sched_watch_pid_t sched_watch_pid_fp = NULL;
char einit_quietness = 0;

#endif

char *ctrlsocket = "/dev/einit-control";

struct remote_event_function {
 uint32_t type;                                 /*!< type of function */
 void (*handler)(struct einit_remote_event *);  /*!< handler function */
 struct remote_event_function *next;            /*!< next function */
};

void *einit_event_emit_remote_dispatch (struct einit_remote_event *ev) {
 return NULL;
}

void einit_event_emit_remote (struct einit_remote_event *ev, enum einit_event_emit_flags flags) {
}

char einit_connect(int *argc, char **argv) {
 if (argc && argv) {
  int i = 0;
  for (i = 1; i < *argc; i++) {
   if (argv[i][0] == '-')
    switch (argv[i][1]) {
     case 's':
      if ((++i) < (*argc))
       ctrlsocket = argv[i];
      break;
    }
  }
 }

 return 1;
}

char einit_disconnect() {
 return 1;
}

void einit_receive_events() {
}

char *einit_ipc_i (const char *cmd, const char *interface) {
 int sock = socket (AF_UNIX, SOCK_STREAM, 0), ret = 0;
 char buffer[BUFFERSIZE];
 char **rvx = NULL;
 struct sockaddr_un saddr;
 int len = strlen (cmd);
 char *c;
 FILE *esocket;
 if (sock == -1) {
  bitch (bitch_stdio, 0, "socket not open");
  return NULL;
 }

 saddr.sun_family = AF_UNIX;
 strncpy (saddr.sun_path, ctrlsocket, sizeof(saddr.sun_path) - 1);

 if (connect(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
  eclose (sock);
  bitch (bitch_stdio, 0, "connect() failed.");
  return NULL;
 }

 len = strlen(cmd);
 c = emalloc ((len+2)*sizeof (char));
 esprintf (c, (len+2), "%s\n", cmd);

 if (!(esocket = fdopen (sock, "w+"))) {
  bitch(bitch_stdio, 0, "fdopen() failed.");
  eclose (sock);
  return 0;
 }

 if (fputs(c, esocket) == EOF) {
  bitch(bitch_stdio, 0, "fputs() failed.");
 }

 if (fflush (esocket) == EOF)
  bitch(bitch_stdio, errno, "couldn't flush IPC buffer");

 while ((!feof(esocket)) && fgets (buffer, BUFFERSIZE, esocket)) {
  if (strmatch("IPC//processed.\n", buffer)) {
   char retval[BUFFERSIZE];
   *retval = 0;

   fgets (retval, BUFFERSIZE, esocket);

   ret = atoi(retval);

   break;
  }

//  eputs (buffer, stdout);
  rvx = (char **)setadd ((void **)rvx, buffer, SET_TYPE_STRING);
 }

 errno = 0;

 efclose (esocket);

 if (rvx) {
  char *rv = set2str ('\n', (const char **)rvx);
  return rv;
 } else return NULL;
// return ret;
}

char *einit_ipc(const char *command) {
 return einit_ipc_i (command, NULL);
}

char *einit_ipc_safe(const char *command) {
 return einit_ipc_i (command, NULL);
}

/* the socket version doesn't precisely need to connect... */
char *einit_ipc_request(const char *command) {
 return einit_ipc(command);
}

void einit_remote_event_emit_dispatch (struct einit_remote_event *ev) {
 return;
}

void einit_remote_event_emit (struct einit_remote_event *ev, enum einit_event_emit_flags flags) {
 return;
}
