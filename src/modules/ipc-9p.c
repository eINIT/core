/*
 *  ipc-9p.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/01/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <einit/bitch.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <einit-modules/ipc.h>

#include <ixp_local.h>

#ifdef estrdup
#undef estrdup
#endif
#ifdef emalloc
#undef emalloc
#endif
#ifdef ecalloc
#undef ecalloc
#endif

#ifdef POSIXREGEX
#include <regex.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_ipc_9p_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_ipc_9p_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT IPC module (9p)",
 .rid       = "einit-ipc-9p",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_ipc_9p_configure
};

module_register(einit_ipc_9p_self);

#endif

pthread_t einit_ipc_9p_thread;
char einit_ipc_9p_running = 0;

void einit_ipc_9p_boot_event_handler_root_device_ok (struct einit_event *);
void einit_ipc_9p_power_event_handler (struct einit_event *);
char *einit_ipc_9p_request (char *);

enum ipc_9p_filetype {
 i9_dir,
 i9_file
};

struct ipc_9p_filedata {
 char *data;
 int c;
 enum ipc_9p_filetype type;
};

struct ipc_9p_fidaux {
 char **path;
 struct ipc_9p_filedata *fd;
};

struct ipc_9p_filedata *ipc_9p_filedata_dup (struct ipc_9p_filedata *d) {
 if (!d) return NULL;

 struct ipc_9p_filedata *fd = emalloc (sizeof (struct ipc_9p_filedata));

 fd->data = d->data ? estrdup (d->data) : NULL;
 fd->c = d->c;
 fd->type = d->type;

 return fd;
}

struct ipc_9p_fidaux *einit_ipc_9p_fidaux_dup (struct ipc_9p_fidaux *d) {
 struct ipc_9p_fidaux *fa = emalloc (sizeof (struct ipc_9p_fidaux));

 fa->path = (char **)(d->path ? setdup ((const void **)d->path, SET_TYPE_STRING) : NULL);
 fa->fd = ipc_9p_filedata_dup(d->fd);

 return fa;
}

void einit_ipc_9p_fs_open(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_open()");
 struct ipc_9p_fidaux *fa = r->fid->aux;

 if (fa->path) {
  struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));
  char *req = set2str (' ', (const char **)fa->path);
  
  fd->data = einit_ipc_9p_request (req);
  fd->c = 0;
  fd->type = i9_file;

  fa->fd = fd;

  efree (req);
 } else {
  struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));

  fd->data = estrdup ("unknown");
  fd->c = 0;
  fd->type = i9_dir;

  fa->fd = fd;
 }

 respond(r, nil);
}

void einit_ipc_9p_fs_walk(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_walk()");
 int i = 0;
 struct ipc_9p_fidaux *fa = r->fid->aux;

 r->newfid->aux = einit_ipc_9p_fidaux_dup (fa);
 fa = r->newfid->aux;

 for(; i < r->ifcall.nwname; i++) {
//  notice (1, "new path element: %s", r->ifcall.wname[i]);
  fa->path = (char **)setadd ((void **)fa->path, r->ifcall.wname[i], SET_TYPE_STRING);

  r->ofcall.wqid[i].type = 0;
  r->ofcall.wqid[i].path = 0;
 }

 r->ofcall.nwqid = i;
 respond(r, nil);
}

struct einit_ipc_9p_request_data {
 char *command;
 FILE *output;
};

void einit_ipc_9p_request_thread (struct einit_ipc_9p_request_data *d) {
 ipc_process(d->command, d->output);
 fclose (d->output);
 efree (d);
}

char *einit_ipc_9p_request (char *command) {
 if (!command) return NULL;

 int internalpipe[2];
 char *returnvalue = NULL;

 if (!socketpair (AF_UNIX, SOCK_STREAM, 0, internalpipe)) {
// c'mon, don't tell me you're going to send data fragments > 400kb using the IPC interface!
  int socket_buffer_size = 40960;

  fcntl (internalpipe[0], F_SETFL, O_NONBLOCK);
  fcntl (internalpipe[1], F_SETFL, O_NONBLOCK);
/* tag the fds as close-on-exec, just in case */
  fcntl (internalpipe[0], F_SETFD, FD_CLOEXEC);
  fcntl (internalpipe[1], F_SETFD, FD_CLOEXEC);

  setsockopt (internalpipe[0], SOL_SOCKET, SO_SNDBUF, &socket_buffer_size, sizeof (int));
  setsockopt (internalpipe[1], SOL_SOCKET, SO_SNDBUF, &socket_buffer_size, sizeof (int));
  setsockopt (internalpipe[0], SOL_SOCKET, SO_RCVBUF, &socket_buffer_size, sizeof (int));
  setsockopt (internalpipe[1], SOL_SOCKET, SO_RCVBUF, &socket_buffer_size, sizeof (int));

  FILE *w = fdopen (internalpipe[1], "w");
//  FILE *r = fdopen (internalpipe[0], "r");

//  ipc_process(command, w);
//  fclose (w);
  struct einit_ipc_9p_request_data *d = emalloc (sizeof (struct einit_ipc_9p_request_data));
  d->command = command;
  d->output = w;

  ethread_spawn_detached_run ((void *(*)(void *))einit_ipc_9p_request_thread, d);

  errno = 0;
  if (internalpipe[0] != -1) {
   returnvalue = readfd_l (internalpipe[0], NULL);

   if (!returnvalue) {
    notice (1, "lolwut?");
   }

   eclose (internalpipe[0]);
  }
 }

 if (!returnvalue) returnvalue = estrdup("<einit-ipc><warning type=\"no-return-value\" /></einit-ipc>\n");

 return returnvalue;
}

void einit_ipc_9p_fs_read(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_read()");
 struct ipc_9p_fidaux *fa = r->fid->aux;
 struct ipc_9p_filedata *fd = fa->fd;

 if (fd->type == i9_file) {
  if (fd->data) {
   fflush (stderr);

   size_t size = r->ifcall.count;
   r->ofcall.data = ecalloc(1, size+1);

   int y = 0;
   ssize_t tsize = strlen (fd->data);
   ssize_t offs = r->ifcall.offset;

   if (tsize >= offs) {
    for (y = 0; ((offs+y) < tsize) && (y <= size); y++) {
     r->ofcall.data[y] = fd->data[offs+y];
    }

    if (y == (size+1)) y = size;

    fflush (stderr);

    r->ofcall.count = y;

    respond(r, nil);
   } else {
    r->ofcall.count = 0;
    respond(r, nil);
   }
  } else {
   r->ofcall.count = 0;
   respond(r, nil);
  }
 } else if (fd->type == i9_dir) {
  if ((fd->c) < 10) {
   Stat s;
   memset (&s, 0, sizeof (Stat));

   s.mode |= P9_DMDIR;
   s.qid.type |= QTDIR; 
   s.name = "mhhh";

   s.uid = "unknown";
   s.gid = "unknown";
   s.muid = "unknown";

   size_t size = r->ifcall.count;
   void *buf = ecalloc(1, size);
   IxpMsg m = ixp_message((uchar*)buf, size, MsgPack); 
   ixp_pstat(&m, &s);

   r->ofcall.count = ixp_sizeof_stat(&s);
   r->ofcall.data = (char*)m.data;

   fd->c++;

   respond(r, nil);
  } else {
   r->ofcall.count = 0;
   respond(r, nil);
  }
 }
}

void einit_ipc_9p_fs_stat(Ixp9Req *r) {
 struct ipc_9p_fidaux *fa = r->fid->aux;
 char *path = set2str (':', (const char **)fa->path);

 if (path) {
  notice (1, "einit_ipc_9p_fs_stat(%s)", path);
 } else {
  notice (1, "einit_ipc_9p_fs_stat()");
  path = estrdup ("/");
 }

 Stat s;
 memset (&s, 0, sizeof (Stat));

 s.mode |= P9_DMDIR;
 s.qid.type |= QTDIR; 
 s.name = path;

 s.uid = "unknown";
 s.gid = "unknown";
 s.muid = "unknown";
 
 r->fid->qid = s.qid;
 size_t size = ixp_sizeof_stat(&s);
 r->ofcall.nstat = size;

 void *buf = ecalloc (1, size);

 IxpMsg m = ixp_message(buf, size, MsgPack);
 ixp_pstat(&m, &s);

 r->ofcall.stat = m.data; 

 efree (path);

 respond(r, nil);
}

void einit_ipc_9p_fs_write(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_write()");
}

void einit_ipc_9p_fs_clunk(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_clunk()");

 respond(r, nil);
}

void einit_ipc_9p_fs_flush(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_flush()");
}

void einit_ipc_9p_fs_attach(Ixp9Req *r) {
 r->fid->qid.type = QTDIR;
 r->fid->qid.path = (uintptr_t)r->fid;
 r->fid->aux = ecalloc (1, sizeof (struct ipc_9p_fidaux));

 r->ofcall.qid = r->fid->qid;
 respond(r, nil);
}

void einit_ipc_9p_fs_create(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_create()");
}

void einit_ipc_9p_fs_remove(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_remove()");
}

void einit_ipc_9p_fs_freefid(Fid *f) {
 if (f->aux) {
  struct ipc_9p_fidaux *fa = f->aux;
  if (fa->path) efree (fa->path);

  if (fa->fd) {
   if (fa->fd->data) efree (fa->fd->data);
   efree (fa->fd);
  }

  efree (f->aux);
 }
}

Ixp9Srv einit_ipc_9p_srv = {
 .open    = einit_ipc_9p_fs_open,
 .walk    = einit_ipc_9p_fs_walk,
 .read    = einit_ipc_9p_fs_read,
 .stat    = einit_ipc_9p_fs_stat,
 .write   = einit_ipc_9p_fs_write,
 .clunk   = einit_ipc_9p_fs_clunk,
 .flush   = einit_ipc_9p_fs_flush,
 .attach  = einit_ipc_9p_fs_attach,
 .create  = einit_ipc_9p_fs_create,
 .remove  = einit_ipc_9p_fs_remove,
 .freefid = einit_ipc_9p_fs_freefid
};

static IxpServer einit_ipc_9p_server; 
 
void *einit_ipc_9p_thread_function (void *unused_parameter) {
 einit_ipc_9p_running = 1;

 char *address = cfg_getstring ("subsystem-ipc-9p/socket", NULL);

 if (!address) address = "unix!/dev/einit-9p";

 if (coremode & einit_mode_sandbox) {
  address = "unix!dev/einit-9p";
 }

 int fd = ixp_announce (address);

 if (!fd) {
  notice (1, "cannot initialise 9p server");
 }

 IxpConn* connection = ixp_listen(&einit_ipc_9p_server, fd, &einit_ipc_9p_srv, serve_9pcon, NULL); 

// ixp_pthread_init();

 notice (1, "9p server initialised");

 ixp_serverloop(&einit_ipc_9p_server);

 notice (1, "9p server loop has terminated: %s", ixp_errbuf());

 einit_ipc_9p_running = 0;
}

void einit_ipc_9p_boot_event_handler_root_device_ok (struct einit_event *ev) {
 notice (6, "enabling IPC (9p)");
 ethread_create (&einit_ipc_9p_thread, NULL, einit_ipc_9p_thread_function, NULL);
}

void einit_ipc_9p_power_event_handler (struct einit_event *ev) {
 notice (4, "disabling IPC (9p)");
 if (einit_ipc_9p_running) {
  einit_ipc_9p_running = 0;
  ixp_server_close (&einit_ipc_9p_server);
//  ethread_cancel (einit_ipc_9p_thread);
 }
}

int einit_ipc_9p_cleanup (struct lmodule *this) {
 ipc_cleanup(irr);

 event_ignore (einit_boot_root_device_ok, einit_ipc_9p_boot_event_handler_root_device_ok);
 event_ignore (einit_power_down_scheduled, einit_ipc_9p_power_event_handler);
 event_ignore (einit_power_reset_scheduled, einit_ipc_9p_power_event_handler);

 return 0;
}

int einit_ipc_9p_configure (struct lmodule *irr) {
 module_init(irr);
 ipc_configure(irr);

 irr->cleanup = einit_ipc_9p_cleanup;

 event_listen (einit_boot_root_device_ok, einit_ipc_9p_boot_event_handler_root_device_ok);
 event_listen (einit_power_down_scheduled, einit_ipc_9p_power_event_handler);
 event_listen (einit_power_reset_scheduled, einit_ipc_9p_power_event_handler);

 return 0;
}
