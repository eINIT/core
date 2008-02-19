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
#include <sys/stat.h>

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

#include <regex.h>

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

gid_t einit_ipc_9p_einitgid = 0;

void einit_ipc_9p_boot_event_handler_root_device_ok (struct einit_event *);
void einit_ipc_9p_power_event_handler (struct einit_event *);

pthread_mutex_t
 einit_ipc_9p_event_queue_mutex = PTHREAD_MUTEX_INITIALIZER,
 einit_ipc_9p_event_update_listeners_mutex = PTHREAD_MUTEX_INITIALIZER,
 einit_ipc_9p_event_respond_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t
 einit_ipc_9p_ping_cond = PTHREAD_COND_INITIALIZER;

enum ipc_9p_filetype {
 i9_dir,
 i9_file,
 i9_events
};

struct msg_event_queue {
 char *event;
 struct msg_event_queue *next;
 struct msg_event_queue *previous;
};

struct msg_event_queue *einit_ipc_9p_event_queue = NULL;

Ixp9Req **einit_ipc_9p_event_update_listeners = NULL;

struct ipc_9p_filedata {
 char *data;
 enum ipc_9p_filetype type;
 struct stree *cur;
 struct ipc_fs_node **files;
 int c;
 char is_writable;
 struct msg_event_queue *event;
};

struct ipc_9p_fidaux {
 char **path;
 struct ipc_9p_filedata *fd;
};

Ixp9Req *ipc_9p_respond_serialise (Ixp9Req *r, const char *m) {
 emutex_lock (&einit_ipc_9p_event_respond_mutex);
 respond (r, (char *)m);
 emutex_unlock (&einit_ipc_9p_event_respond_mutex);
}

struct ipc_9p_filedata *ipc_9p_filedata_dup (struct ipc_9p_filedata *d) {
 if (!d) return NULL;

 struct ipc_9p_filedata *fd = emalloc (sizeof (struct ipc_9p_filedata));

 fd->data = d->data ? (char *)str_stabilise (d->data) : NULL;
 fd->type = d->type;
 fd->cur = d->cur;
 fd->files = (struct ipc_fs_node **)(d->files ? set_fix_dup ((const void **)d->files, sizeof(struct ipc_fs_node)) : NULL);
 fd->c = d->c;
 fd->is_writable = d->is_writable;
 fd->event = d->event;

 return fd;
}

struct ipc_9p_fidaux *einit_ipc_9p_fidaux_dup (struct ipc_9p_fidaux *d) {
 struct ipc_9p_fidaux *fa = emalloc (sizeof (struct ipc_9p_fidaux));

 fa->path = (char **)(d->path ? set_str_dup_stable (d->path) : NULL);
 fa->fd = ipc_9p_filedata_dup(d->fd);

 return fa;
}


void einit_ipc_9p_fs_open_spawn (Ixp9Req *r) {
// notice (1, "einit_ipc_9p_fs_open()");
 struct ipc_9p_fidaux *fa = r->fid->aux;

 if (r->ifcall.mode == P9_OREAD) {
  struct einit_event ev = evstaticinit(einit_ipc_read);

  ev.para = fa->path;

  event_emit(&ev, einit_event_flag_broadcast);

  if (ev.stringset) {
   struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));
   ev.stringset = set_str_add (ev.stringset, "");
   char *r = set2str ('\n', (const char **)ev.stringset);

   fd->data = (char *)str_stabilise(r);
   if (r) efree (r);

   fd->type = i9_file;
   fd->cur = NULL;

   fa->fd = fd;

   efree (ev.stringset);
   ev.stringset = NULL;
  } else if (ev.set) {
   struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));

   struct ipc_fs_node n = { .name = (char *)str_stabilise (".."), .is_file = 0 };
   ev.set = set_fix_add (ev.set, &n, sizeof (n));
   n.name = (char *)str_stabilise (".");
   ev.set = set_fix_add (ev.set, &n, sizeof (n));

   fd->data = (char *)str_stabilise ("unknown");
   fd->type = i9_dir;
   fd->files = (struct ipc_fs_node **)ev.set;
   fd->c = 0;

   fa->fd = fd;
  } else {
   evstaticdestroy (ev);
   ipc_9p_respond_serialise (r, "File not found.");
   return;
  }

  evstaticdestroy (ev);

  ipc_9p_respond_serialise(r, nil);
 } else /*if ((r->ifcall.mode == P9_OWRITE) || (r->ifcall.mode == (P9_OWRITE | P9_OTRUNC))) */{
  struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));
  fa->fd = fd;

  fd->is_writable = 1;
//  notice (1, "opened file for writing");

  ipc_9p_respond_serialise(r, nil);
 }/* else {
  ipc_9p_respond_serialise (r, "Access Mode not supported.");
 }*/
}

void einit_ipc_9p_fs_open (Ixp9Req *r) {
 struct ipc_9p_fidaux *fa = r->fid->aux;

 if (fa && fa->path && fa->path[0] && strmatch (fa->path[0], "events")) {
  if (r->ifcall.mode == P9_OREAD) {
   struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));

   fd->type = i9_events;
   fd->event = einit_ipc_9p_event_queue;

   fa->fd = fd;

   ipc_9p_respond_serialise(r, nil);
  }
 } else
//  ethread_spawn_detached_run ((void *(*)(void *))einit_ipc_9p_fs_open_spawn, r);
  einit_ipc_9p_fs_open_spawn(r);
}

void einit_ipc_9p_fs_walk(Ixp9Req *r) {
// notice (1, "einit_ipc_9p_fs_walk()");
 int i = 0;
 struct ipc_9p_fidaux *fa = r->fid->aux;

 r->newfid->aux = einit_ipc_9p_fidaux_dup (fa);
 fa = r->newfid->aux;

 for(; i < r->ifcall.nwname; i++) {
//  notice (1, "new path element: %s", r->ifcall.wname[i]);
  if (strmatch (r->ifcall.wname[i], "..")) {
   if (fa->path) {
    int y = 0;

    for (; fa->path[y]; y++);
    y--;
    if (fa->path[y])
     fa->path[y] = 0;

    if (!fa->path[0]) {
     efree (fa->path);
     fa->path = NULL;
    }
   }
  } else {
   fa->path = set_str_add_stable (fa->path, r->ifcall.wname[i]);
  }

  r->ofcall.wqid[i].type = P9_QTDIR;
  r->ofcall.wqid[i].path = 0;
//  r->ofcall.wqid[i].mode = 0660;
 }

 r->ofcall.wqid[i-1].type = P9_QTAPPEND;

 r->ofcall.nwqid = i;
 ipc_9p_respond_serialise(r, nil);
}

void einit_ipc_9p_fs_reply_event (Ixp9Req *r) {
 struct ipc_9p_fidaux *fa = r->fid->aux;
 struct ipc_9p_filedata *fd = fa->fd;

 if (fd->event->next != einit_ipc_9p_event_queue) {
//  fprintf (stdout, "printing event\n");
//  fflush (stderr);

  r->ofcall.data = estrdup (fd->event->event);
  r->ofcall.count = strlen (r->ofcall.data);

  fd->event = fd->event->next;

  ipc_9p_respond_serialise(r, nil);
 } else {
//  fprintf (stdout, "no more events right now\n");
//  fflush (stdout);

  emutex_lock(&einit_ipc_9p_event_update_listeners_mutex);
  einit_ipc_9p_event_update_listeners = (Ixp9Req **)set_noa_add ((void **)einit_ipc_9p_event_update_listeners, r);
  emutex_unlock(&einit_ipc_9p_event_update_listeners_mutex);
 }
}

void einit_ipc_9p_fs_read (Ixp9Req *r) {
// notice (1, "einit_ipc_9p_fs_read()");
 struct ipc_9p_fidaux *fa = r->fid->aux;
 struct ipc_9p_filedata *fd = fa->fd;

 if (fd->type == i9_events) {
  einit_ipc_9p_fs_reply_event (r);
 } else if (fd->type == i9_file) {
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

    if (y > size) {
     y = size;
    }

    r->ofcall.count = y;

    ipc_9p_respond_serialise(r, nil);
   } else {
    r->ofcall.count = 0;
    ipc_9p_respond_serialise(r, nil);
   }
  } else {
   r->ofcall.count = 0;
   ipc_9p_respond_serialise(r, nil);
  }
 } else if (fd->type == i9_dir) {
  if (fd->files && (fd->files[fd->c])) {
   Stat s;
   memset (&s, 0, sizeof (Stat));

//   notice (1, "submitting: %s", fd->files[fd->c]->name);

   s.name = fd->files[fd->c]->name;

   s.uid = "root";
   s.gid = "einit";
   s.muid = "unknown";

   s.extension = "";

   if (!einit_ipc_9p_einitgid) {
    lookupuidgid (NULL, &einit_ipc_9p_einitgid, NULL, "einit");
   }

   s.mode = 0666;

   if (!(fd->files[fd->c]->is_file)) {
    s.mode |= 0770 | P9_DMDIR;
    s.qid.type |= QTDIR; 
   }

   size_t size = r->ifcall.count;
   void *buf = ecalloc(1, size);
   IxpMsg m = ixp_message((uchar*)buf, size, MsgPack); 
   if (r->dotu)
    ixp_pstat_dotu(&m, &s);
   else
    ixp_pstat(&m, &s);

   r->ofcall.count = r->dotu ? ixp_sizeof_stat_dotu(&s) : ixp_sizeof_stat(&s);
   r->ofcall.data = (char*)m.data;

   fd->c++;
   ipc_9p_respond_serialise(r, nil);
  } else {
   r->ofcall.count = 0;
   ipc_9p_respond_serialise(r, nil);
  }
 }
}

void einit_ipc_9p_fs_stat_spawn (Ixp9Req *r) {
 struct ipc_9p_fidaux *fa = r->fid->aux;
 char *path = set2str ('/', (const char **)fa->path);

 struct einit_event ev = evstaticinit(einit_ipc_stat);

 ev.para = fa->path;
 event_emit(&ev, einit_event_flag_broadcast);

 char is_file = ev.flag;

 evstaticdestroy (ev);

 if (!path) {
  path = estrdup ("/");
 }

 Stat s;
 memset (&s, 0, sizeof (Stat));

 s.mode = P9_OAPPEND | 0660;

 if (!is_file) {
//  notice (1, "directory: %s", path);
  s.mode = 0770 | P9_DMDIR;
  s.qid.type |= QTDIR; 
 }

 s.name = path;

 s.uid = "root";
 s.gid = "einit";
 s.muid = "unknown";

 s.extension = "";

 if (!einit_ipc_9p_einitgid) {
  lookupuidgid (NULL, &einit_ipc_9p_einitgid, NULL, "einit");
 }
 s.n_gid = einit_ipc_9p_einitgid;

 r->fid->qid = s.qid;
 size_t size = r->dotu ? ixp_sizeof_stat_dotu(&s) : ixp_sizeof_stat(&s);
 r->ofcall.nstat = size;

 void *buf = ecalloc (1, size);

 IxpMsg m = ixp_message(buf, size, MsgPack);
 if (r->dotu)
  ixp_pstat_dotu(&m, &s);
 else
  ixp_pstat(&m, &s);

 r->ofcall.stat = m.data; 

 efree (path);

 ipc_9p_respond_serialise(r, nil);
}

void einit_ipc_9p_fs_stat (Ixp9Req *r) {
 einit_ipc_9p_fs_stat_spawn(r);
// ethread_spawn_detached_run ((void *(*)(void *))einit_ipc_9p_fs_stat_spawn, r);
}

void einit_ipc_9p_fs_write (Ixp9Req *r) {
 struct ipc_9p_fidaux *fa = r->fid->aux;

// notice (1, "einit_ipc_9p_fs_write(%i, %i)", r->ifcall.count, r->ofcall.count);

 if (r->ifcall.count == 0) {
//  notice (1, "einit_ipc_9p_fs_write()");

  ipc_9p_respond_serialise(r, nil);
  return;
 } else {
  struct ipc_9p_filedata *fd = fa->fd;
  int len = 1 + (fd->data ? strlen (fd->data) : 0);

  r->ofcall.count = r->ifcall.count;
  fd->data = fd->data ? erealloc (fd->data, r->ifcall.count + len) : emalloc (r->ifcall.count + len);

  memcpy (fd->data + (len-1), r->ifcall.data, r->ifcall.count);
  fd->data[r->ifcall.count + len -1] = 0;

//  notice (1, "einit_ipc_9p_fs_write(%s)", fd->data);

  ipc_9p_respond_serialise(r, nil);
 }
}

void einit_ipc_9p_fs_clunk_spawn (Ixp9Req *r) {
// notice (1, "einit_ipc_9p_fs_clunk()");
 struct ipc_9p_fidaux *fa = r->fid->aux;

 if (fa && fa->fd) {
  emutex_lock (&einit_ipc_9p_event_update_listeners_mutex);
  repeat:
  if (einit_ipc_9p_event_update_listeners) {
   int i = 0;
   for (; einit_ipc_9p_event_update_listeners[i]; i++) {
    struct ipc_9p_fidaux *sfa = einit_ipc_9p_event_update_listeners[i]->fid->aux;
    if (sfa->fd == fa->fd) {
     einit_ipc_9p_event_update_listeners = (Ixp9Req **)setdel ((void **)einit_ipc_9p_event_update_listeners, einit_ipc_9p_event_update_listeners[i]);
     goto repeat;
    }
   }
  }
  emutex_unlock (&einit_ipc_9p_event_update_listeners_mutex);

  struct ipc_9p_filedata *fd = fa->fd;

  if (fd->is_writable && fd->data) {
   strtrim (fd->data);

   if (fd->data[0]) {
    struct einit_event ev = evstaticinit(einit_ipc_write);

    ev.para = fa->path;
    ev.set = set_noa_add (ev.set, fd->data);
    event_emit(&ev, einit_event_flag_broadcast);

    evstaticdestroy (ev);
   }
  }
 }

 ipc_9p_respond_serialise(r, nil);
}

void einit_ipc_9p_fs_clunk (Ixp9Req *r) {
 einit_ipc_9p_fs_clunk_spawn(r);
// ethread_spawn_detached_run ((void *(*)(void *))einit_ipc_9p_fs_clunk_spawn, r);
}

void einit_ipc_9p_fs_flush(Ixp9Req *r) {
// notice (1, "einit_ipc_9p_fs_flush()");
// ipc_9p_respond_serialise (r, nil);
 emutex_lock (&einit_ipc_9p_event_update_listeners_mutex);
 repeat:
  if (einit_ipc_9p_event_update_listeners) {
  int i = 0;
  for (; einit_ipc_9p_event_update_listeners[i]; i++) {
   if (r->ifcall.oldtag == einit_ipc_9p_event_update_listeners[i]->ifcall.tag) {
    einit_ipc_9p_event_update_listeners = (Ixp9Req **)setdel ((void **)einit_ipc_9p_event_update_listeners, einit_ipc_9p_event_update_listeners[i]);
    goto repeat;
   }
  }
 }
 emutex_unlock (&einit_ipc_9p_event_update_listeners_mutex);

 ipc_9p_respond_serialise (r, nil);
}

void einit_ipc_9p_fs_attach(Ixp9Req *r) {
 r->fid->qid.type = QTDIR;
 r->fid->qid.path = (uintptr_t)r->fid;
 r->fid->aux = ecalloc (1, sizeof (struct ipc_9p_fidaux));

 r->ofcall.qid = r->fid->qid;
 ipc_9p_respond_serialise(r, nil);
}

void einit_ipc_9p_fs_create(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_create()");
 ipc_9p_respond_serialise (r, "not implemented.");
}

void einit_ipc_9p_fs_remove(Ixp9Req *r) {
 notice (1, "einit_ipc_9p_fs_remove()");
 ipc_9p_respond_serialise (r, "not implemented.");
}

void einit_ipc_9p_fs_freefid(Fid *f) {
 if (f->aux) {
  struct ipc_9p_fidaux *fa = f->aux;

  if (fa->fd) {
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

void *einit_ipc_9p_listen (void *param) {
 intptr_t fdp = (intptr_t)param;
 int fd = fdp;

 fcntl (fd, F_SETFD, FD_CLOEXEC);

 IxpConn* connection = ixp_listen(&einit_ipc_9p_server, fd, &einit_ipc_9p_srv, serve_9pcon, NULL); 

 if (connection) {
// ixp_pthread_init();

  notice (1, "9p server initialised");
  /* server loop nao */

  ixp_serverloop(&einit_ipc_9p_server);

  notice (1, "9p server loop has terminated: %s", ixp_errbuf());

  einit_ipc_9p_running = 0;
 } else {
  notice (1, "could not initialise 9p server");
 }

 return NULL;
}

void einit_ipc_9p_generic_event_handler (struct einit_event *ev) {
 struct msg_event_queue *e = emalloc (sizeof (struct msg_event_queue));

 char **data = NULL;
 char buffer[BUFFERSIZE];

 esprintf (buffer, BUFFERSIZE, "event=%i", ev->seqid);
 data = set_str_add (data, buffer);

 esprintf (buffer, BUFFERSIZE, "type=%s", event_code_to_string(ev->type));
 data = set_str_add (data, buffer);

 if (ev->integer) {
  esprintf (buffer, BUFFERSIZE, "integer=%i", ev->integer);
  data = set_str_add (data, buffer);
 }

 if (ev->task) {
  esprintf (buffer, BUFFERSIZE, "task=%i", ev->task);
  data = set_str_add (data, buffer);
 }

 if (ev->status) {
  esprintf (buffer, BUFFERSIZE, "status=%i", ev->status);
  data = set_str_add (data, buffer);
 }


 if (ev->flag) {
  esprintf (buffer, BUFFERSIZE, "flag=%i", ev->flag);
  data = set_str_add (data, buffer);
 }

 if ((ev->type == einit_feedback_module_status) && ev->para) {
  struct lmodule *m = (struct lmodule *)ev->para;
  if (m->module->rid) {
   char *msg_string;
   size_t i = strlen (m->module->rid) + 1 + 9; /* "module=\n"*/
   msg_string = emalloc (i);
   esprintf (msg_string, i, "module=%s", m->module->rid);

   data = set_str_add (data, msg_string);
  }
 }

 if (ev->string) {
  char *msg_string;
  size_t i = strlen (ev->string) + 1 + 9; /* "string=\n"*/
  msg_string = emalloc (i);
  esprintf (msg_string, i, "string=%s", ev->string);

  data = set_str_add (data, msg_string);
 }

 if (ev->stringset) {
  int y = 0;
  for (; ev->stringset[y]; y++) {
   char *msg_string;
   size_t i = strlen (ev->stringset[y]) + 1 + 12; /* "stringset=\n"*/
   msg_string = emalloc (i);
   esprintf (msg_string, i, "stringset=%s", ev->stringset[y]);

   data = set_str_add (data, msg_string);
  }
 }

 data = set_str_add (data, "\n");

 e->event = set2str ('\n', (const char **)data);
 efree (data);

 emutex_lock (&einit_ipc_9p_event_queue_mutex);

 if (einit_ipc_9p_event_queue) {
  e->previous = einit_ipc_9p_event_queue->previous;
  einit_ipc_9p_event_queue->previous->next = e;
  einit_ipc_9p_event_queue->previous = e;

  e->next = einit_ipc_9p_event_queue;
 } else {
  e->previous = e;
  e->next = e;
  einit_ipc_9p_event_queue = e;
 }

 emutex_unlock (&einit_ipc_9p_event_queue_mutex);

 emutex_lock (&einit_ipc_9p_event_update_listeners_mutex);
 if (einit_ipc_9p_event_update_listeners) {
  int i = 0;
  for (; einit_ipc_9p_event_update_listeners[i]; i++) {
   ethread_spawn_detached_run ((void *(*)(void *))einit_ipc_9p_fs_reply_event, einit_ipc_9p_event_update_listeners[i]);
  }
  efree (einit_ipc_9p_event_update_listeners);
  einit_ipc_9p_event_update_listeners = NULL;
 }
 emutex_unlock (&einit_ipc_9p_event_update_listeners_mutex);
}

void *einit_ipc_9p_thread_function (void *unused_parameter) {
 einit_ipc_9p_running = 1;

 char *address = cfg_getstring ("subsystem-ipc-9p/socket", NULL);
 char *group = cfg_getstring ("subsystem-ipc-9p/group", NULL);
 char *chmod_i = cfg_getstring ("subsystem-ipc-9p/chmod", NULL);

 if (!group) group = "einit";
 if (!chmod_i) chmod_i = "0660";
 mode_t smode = parse_integer (chmod_i);

 if (!address) address = "unix!/dev/einit-9p";

 if (coremode & einit_mode_sandbox) {
  address = "unix!dev/einit-9p";
 }

 intptr_t fd = ixp_announce (address);

 if (!fd) {
  notice (1, "cannot initialise 9p server");
  return NULL;
 }

 char **sp = str2set ('!', address);
 if (sp && sp[0] && sp[1]) {
  gid_t g;
  lookupuidgid(NULL, &g, NULL, group);

  chown (sp[1], 0, g);
  chmod (sp[1], smode);
 }

/* add environment var for synchronisation */

 struct cfgnode newnode;

 memset (&newnode, 0, sizeof(struct cfgnode));

 newnode.id = (char *)str_stabilise ("configuration-environment-global");
 newnode.type = einit_node_regular;

 newnode.arbattrs = set_str_add_stable(newnode.arbattrs, "id");
 newnode.arbattrs = set_str_add_stable(newnode.arbattrs, "EINIT_9P_ADDRESS");

 newnode.arbattrs = set_str_add_stable(newnode.arbattrs, "s");
 newnode.arbattrs = set_str_add_stable(newnode.arbattrs, address);

 newnode.svalue = newnode.arbattrs[3];

 cfg_addnode (&newnode);

 einit_global_environment = straddtoenviron (einit_global_environment, "EINIT_9P_ADDRESS", address);

 einit_ipc_9p_listen((void *)fd);

 return NULL;
}

void *einit_ipc_9p_thread_function_address (char *address) {
 einit_ipc_9p_running = 1;

 intptr_t fd = ixp_announce (address);

 if (!fd) {
  notice (1, "cannot initialise 9p server");
  return NULL;
 }

 einit_ipc_9p_listen((void *)fd);

 return NULL;
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

void einit_ipc_9p_ipc_read (struct einit_event *ev) {
 char **path = ev->para;

 struct ipc_fs_node n;

 if (!path) {
  n.is_file = 0;
  n.name = (char *)str_stabilise ("issues");
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
  n.is_file = 1;
  n.name = (char *)str_stabilise ("events");
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
 }
}

void einit_ipc_9p_ipc_stat (struct einit_event *ev) {
 char **path = ev->para;

 if (path && path[0]) {
  if (strmatch (path[0], "issues")) {
   ev->flag = (path[1] ? 1 : 0);
  } else if (strmatch (path[0], "events")) {
   ev->flag = 1;
  }
 }
}


/*int einit_ipc_9p_cleanup (struct lmodule *this) {
 ipc_cleanup(irr);}*/

const char *einit_ipc_9p_cl_address = NULL;

void einit_ipc_9p_secondary_main_loop (struct einit_event *ev) {
 einit_ipc_9p_thread_function_address ((char *)einit_ipc_9p_cl_address);
}

int einit_ipc_9p_cleanup (struct lmodule *this) {
 event_ignore (einit_boot_devices_available, einit_ipc_9p_boot_event_handler_root_device_ok);
 event_ignore (einit_power_down_imminent, einit_ipc_9p_power_event_handler);
 event_ignore (einit_power_reset_imminent, einit_ipc_9p_power_event_handler);
 event_ignore (einit_ipc_read, einit_ipc_9p_ipc_read);
 event_ignore (einit_ipc_stat, einit_ipc_9p_ipc_stat);

 event_ignore (einit_event_subsystem_any, einit_ipc_9p_generic_event_handler);

 if (einit_ipc_9p_cl_address) {
  event_listen (einit_core_secondary_main_loop, einit_ipc_9p_secondary_main_loop);
 }

 return 0;
}

int einit_ipc_9p_configure (struct lmodule *irr) {
 module_init(irr);

 irr->cleanup = einit_ipc_9p_cleanup;

 event_listen (einit_boot_devices_available, einit_ipc_9p_boot_event_handler_root_device_ok);
 event_listen (einit_power_down_imminent, einit_ipc_9p_power_event_handler);
 event_listen (einit_power_reset_imminent, einit_ipc_9p_power_event_handler);
 event_listen (einit_ipc_read, einit_ipc_9p_ipc_read);
 event_listen (einit_ipc_stat, einit_ipc_9p_ipc_stat);

 event_listen (einit_event_subsystem_any, einit_ipc_9p_generic_event_handler);

 if (einit_argv) {
  char *address = NULL;
  int y = 0;
  for (; einit_argv[y] && einit_argv[y+1]; y++) {
   if (strmatch (einit_argv[y], "--ipc-socket")) {
    address = einit_argv[y+1];
    coremode = einit_mode_ipconly;
   }
  }

  if (address) {
   einit_ipc_9p_cl_address = str_stabilise (address);
   event_listen (einit_core_secondary_main_loop, einit_ipc_9p_secondary_main_loop);
  }
 }

 return 0;
}
