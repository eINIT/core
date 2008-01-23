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

#include <ixp_local.h>

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
struct stree *exported_functions = NULL;
unsigned char *gdebug = 0;
struct stree *hconfiguration = NULL;
struct utsname osinfo = {};
pthread_attr_t thread_attribute_detached = {};
struct spidcb *sched_deadorphans = NULL;
sched_watch_pid_t sched_watch_pid_fp = NULL;
char einit_quietness = 0;

#endif

char *einit_ipc_address = "unix!/dev/einit-9p";
IxpClient *einit_ipc_9p_client = NULL;

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
 char *envvar = getenv ("EINIT_9P_ADDRESS");
 if (envvar)
  einit_ipc_address = envvar;

 if (argc && argv) {
  int i = 0;
  for (i = 1; i < *argc; i++) {
   if (argv[i][0] == '-')
    switch (argv[i][1]) {
     case 'a':
      if ((++i) < (*argc))
       einit_ipc_address = argv[i];
      break;
    }
  }
 }

// einit_ipc_9p_fd = ixp_dial (einit_ipc_address);
 einit_ipc_9p_client = ixp_mount (einit_ipc_address);

 return (einit_ipc_9p_client ? 1 : 0);
}

char einit_disconnect() {
 ixp_unmount (einit_ipc_9p_client);
 return 1;
}

void einit_receive_events() {
}

char *einit_ipc_i (const char *cmd, const char *interface) {
 char buffer[BUFFERSIZE];
 char *data = NULL;

 esprintf (buffer, BUFFERSIZE, "/ipc/%s", cmd);
 IxpCFid *f = ixp_open (einit_ipc_9p_client, buffer, P9_OREAD);

 if (f) {
  intptr_t rn = 0;
  void *buf = NULL;
  intptr_t blen = 0;

  buf = malloc (f->iounit);
  if (!buf) {
   ixp_close (f);
   return NULL;
  }

#if 1
  do {
   fprintf (stderr, "reading.\n");
   buf = realloc (buf, blen + f->iounit);
   if (buf == NULL) {
    ixp_close (f);
    return NULL;
   }
   fprintf (stderr, ".\n");

   rn = ixp_read (f, (char *)(buf + blen), f->iounit);
   if (rn > 0) {
//    write (1, buf + blen, rn);
    blen = blen + rn;
   }
  } while (rn > 0);

  fprintf (stderr, "done.\n");

  if (rn > -1) {
   data = realloc (buf, blen+1);
   if (buf == NULL) return NULL;

   data[blen] = 0;
   if (blen > 0) {
    *(data+blen) = 0;
   } else {
    free (data);
    data = NULL;
   }

  }
#else
  while((rn = ixp_read(f, buf, f->iounit)) > 0) 
   write(1, buf, rn);
#endif
 
  ixp_close (f);
 }

 return data;
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
