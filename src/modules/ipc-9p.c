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
 enum ipc_9p_filetype type;
 struct stree *cur;
 struct ipc_fs_node **files;
 int c;
};

struct ipc_9p_fidaux {
 char **path;
 struct ipc_9p_filedata *fd;
};

struct ipc_9p_filedata *ipc_9p_filedata_dup (struct ipc_9p_filedata *d) {
 if (!d) return NULL;

 struct ipc_9p_filedata *fd = emalloc (sizeof (struct ipc_9p_filedata));

 fd->data = d->data ? estrdup (d->data) : NULL;
 fd->type = d->type;
 fd->cur = d->cur;
 fd->files = (struct ipc_fs_node **)(d->files ? setdup ((const void **)d->files, sizeof(struct ipc_fs_node)) : NULL);
 fd->c = d->c;

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
 struct einit_event ev = evstaticinit(einit_ipc_read);

 ev.para = fa->path;
 event_emit(&ev, einit_event_flag_broadcast);

 if (ev.stringset) {
  struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));

  fd->data = set2str ('\n', (const char **)ev.stringset);
  fd->type = i9_file;
  fd->cur = NULL;

  fa->fd = fd;

  efree (ev.stringset);
  ev.stringset = NULL;
 } else if (ev.set) {
  struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));

  fd->data = estrdup ("unknown");
  fd->type = i9_dir;
  fd->files = (struct ipc_fs_node **)ev.set;
  fd->c = 0;

  fa->fd = fd;
 } else {
  respond (r, "file not found");
 }

 evstaticdestroy (ev);

#if 0
 if (fa->fs_pointer && (fa->fs_pointer->is_file == 1)) {
  struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));
  char *req = set2str (' ', (const char **)fa->path);

  fd->data = einit_ipc_9p_request (req);
  fd->type = i9_file;
  fd->fs_pointer = fa->fs_pointer;
  fd->cur = NULL;

  fa->fd = fd;
 } else {
  struct ipc_9p_filedata *fd = ecalloc (1, sizeof (struct ipc_9p_filedata));

  fd->data = estrdup ("unknown");
  fd->type = i9_dir;
  fd->fs_pointer = fa->fs_pointer;
  fd->cur = streelinear_prepare (fd->fs_pointer->nodes);

  fa->fd = fd;
 }
#endif

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
   fa->path = set_str_add((void **)fa->path, r->ifcall.wname[i]);
  }

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

int einit_ipc_9p_process (char *cmd, FILE *f) {
 if (!cmd || !cmd[0]) {
  return 0;
 }

 struct einit_event *event = evinit (einit_ipc_request_generic);
 uint32_t ic;
 int ret = 0;
 int len = strlen (cmd);

 if ((len > 4) && (cmd[len-4] == '.') && (cmd[len-3] == 'x') && (cmd[len-2] == 'm') && (cmd[len-1] == 'l')) {
  cmd[len-4] = 0;
  event->ipc_options |= einit_ipc_output_xml;
 }

 event->command = (char *)cmd;
 event->argv = str2set (' ', cmd);
 event->output = f;
 event->implemented = 0;

 event->argc = setcount ((const void **)event->argv);

 for (ic = 0; event->argv[ic]; ic++) {
  if (strmatch (event->argv[ic], "--ansi")) event->ipc_options |= einit_ipc_output_ansi;
  else if (strmatch (event->argv[ic], "--only-relevant")) event->ipc_options |= einit_ipc_only_relevant;
  else if (strmatch (event->argv[ic], "--help")) event->ipc_options |= einit_ipc_help;
  else if (strmatch (event->argv[ic], "--detach")) event->ipc_options |= einit_ipc_detach;
 }

 if (event->ipc_options & einit_ipc_output_xml) {
  eputs ("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<einit-ipc>\n", f);
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

 if (!event->implemented) {
  if (event->ipc_options & einit_ipc_output_xml) {
   eprintf (f, " <einit-ipc-error code=\"err-not-implemented\" command=\"%s\" verbose-en=\"command not implemented\" />\n", cmd);
  } else {
   eprintf (f, "einit-ipc: %s: command not implemented.\n", cmd);
  }

  ret = 1;
 } else
  ret = event->ipc_return;

 if (event->argv) efree (event->argv);

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
     einit_ipc_9p_process (new_command, f);
    eregfree (&pattern);
   }
  }
 }
#endif

 return ret;
}

void einit_ipc_9p_request_thread (struct einit_ipc_9p_request_data *d) {
 einit_ipc_9p_process(d->command, d->output);
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

  einit_ipc_9p_request_thread (d);

  errno = 0;
  if (internalpipe[0] != -1) {
   returnvalue = readfd_l (internalpipe[0], NULL);

   eclose (internalpipe[0]);
  }
 }

 if (!returnvalue) returnvalue = estrdup("<einit-ipc><warning type=\"no-return-value\" /></einit-ipc>\n");

 efree (command);

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

    if (y > size) {
     y = size;
    }

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
#if 0
  if (fd->cur) {
   Stat s;
   memset (&s, 0, sizeof (Stat));

   notice (1, "submitting: %s", fd->cur->key);

   s.name = fd->cur->key;

   s.uid = "root";
   s.gid = "einit";
   s.muid = "unknown";

   struct ipc_9p_fs_entry *v = fd->cur->value;

   s.mode = 0660;

   if (!v->is_file) {
    s.mode |= 0770 | P9_DMDIR;
    s.qid.type |= QTDIR; 
   }

   size_t size = r->ifcall.count;
   void *buf = ecalloc(1, size);
   IxpMsg m = ixp_message((uchar*)buf, size, MsgPack); 
   ixp_pstat(&m, &s);

   r->ofcall.count = ixp_sizeof_stat(&s);
   r->ofcall.data = (char*)m.data;

   fd->cur = streenext (fd->cur);

   respond(r, nil);
  } else {
   r->ofcall.count = 0;
   respond(r, nil);
  }
#endif
  if (fd->files && (fd->files[fd->c])) {
   Stat s;
   memset (&s, 0, sizeof (Stat));

   notice (1, "submitting: %s", fd->files[fd->c]->name);

   s.name = fd->files[fd->c]->name;

   s.uid = "root";
   s.gid = "einit";
   s.muid = "unknown";

   s.mode = 0660;

   if (!(fd->files[fd->c]->is_file)) {
    s.mode |= 0770 | P9_DMDIR;
    s.qid.type |= QTDIR; 
   }

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
 char *path = set2str ('/', (const char **)fa->path);

 struct einit_event ev = evstaticinit(einit_ipc_stat);

 ev.para = fa->path;
 event_emit(&ev, einit_event_flag_broadcast);

 char is_file = ev.flag;

 evstaticdestroy (ev);

 if (path) {
  notice (1, "einit_ipc_9p_fs_stat(%s)", path);
 } else {
  notice (1, "einit_ipc_9p_fs_stat()");
  path = estrdup ("/");
 }

 Stat s;
 memset (&s, 0, sizeof (Stat));

 s.mode = 0660;

 if (!is_file) {
  notice (1, "directory: %s", path);
  s.mode |= 0770 | P9_DMDIR;
  s.qid.type |= QTDIR; 
 }

 s.name = path;

 s.uid = "root";
 s.gid = "einit";
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
 char *group = cfg_getstring ("subsystem-ipc-9p/group", NULL);
 char *chmod_i = cfg_getstring ("subsystem-ipc-9p/chmod", NULL);

 if (!group) group = "einit";
 if (!chmod_i) chmod_i = "0660";
 mode_t smode = parse_integer (chmod_i);

 if (!address) address = "unix!/dev/einit-9p";

 if (coremode & einit_mode_sandbox) {
  address = "unix!dev/einit-9p";
 }

 int fd = ixp_announce (address);

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

 IxpConn* connection = ixp_listen(&einit_ipc_9p_server, fd, &einit_ipc_9p_srv, serve_9pcon, NULL); 

// ixp_pthread_init();

 notice (1, "9p server initialised");

/* add environment var for synchronisation */

 struct cfgnode newnode;

 memset (&newnode, 0, sizeof(struct cfgnode));

 newnode.id = estrdup ("configuration-environment-global");
 newnode.type = einit_node_regular;

 newnode.arbattrs = set_str_add(newnode.arbattrs, (void *)"id");
 newnode.arbattrs = set_str_add(newnode.arbattrs, (void *)"EINIT_9P_ADDRESS");

 newnode.arbattrs = set_str_add(newnode.arbattrs, (void *)"s");
 newnode.arbattrs = set_str_add(newnode.arbattrs, (void *)address);

 newnode.svalue = newnode.arbattrs[3];

 cfg_addnode (&newnode);

 einit_global_environment = straddtoenviron (einit_global_environment, "EINIT_9P_ADDRESS", address);

/* server loop nao */

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

void einit_ipc_9p_ipc_read (struct einit_event *ev) {
 char **path = ev->para;

 struct ipc_fs_node n = { .name = estrdup (".."), .is_file = 0 };
 ev->set = set_fix_add (ev->set, &n, sizeof (n));
 n.name = estrdup (".");
 ev->set = set_fix_add (ev->set, &n, sizeof (n));

 if (!path) {
  n.name = estrdup ("ipc");
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
 } if (path && path[0] && strmatch (path[0], "ipc")) {
  if (!path[1]) {
   n.is_file = 1;

   n.name = estrdup ("list modules.xml");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("list services.xml");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("list configuration.xml");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("list modules");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("list services");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("list configuration");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("update configuration");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("examine configuration");
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
  } else {
   char *res = einit_ipc_9p_request (estrdup(path[1]));

   ev->stringset = set_str_add(ev->stringset, res);

   efree (res);
  }
 }
}

void einit_ipc_9p_ipc_stat (struct einit_event *ev) {
 char **path = ev->para;

 if (path && path[0] && strmatch (path[0], "ipc")) {
  ev->flag = (path[1] ? 1 : 0);
 }
}

int einit_ipc_9p_cleanup (struct lmodule *this) {
 ipc_cleanup(irr);

 event_ignore (einit_boot_root_device_ok, einit_ipc_9p_boot_event_handler_root_device_ok);
 event_ignore (einit_power_down_imminent, einit_ipc_9p_power_event_handler);
 event_ignore (einit_power_reset_imminent, einit_ipc_9p_power_event_handler);
 event_ignore (einit_ipc_read, einit_ipc_9p_ipc_read);
 event_ignore (einit_ipc_stat, einit_ipc_9p_ipc_stat);

 return 0;
}

int einit_ipc_9p_configure (struct lmodule *irr) {
 module_init(irr);
 ipc_configure(irr);

 irr->cleanup = einit_ipc_9p_cleanup;

 event_listen (einit_boot_root_device_ok, einit_ipc_9p_boot_event_handler_root_device_ok);
 event_listen (einit_power_down_imminent, einit_ipc_9p_power_event_handler);
 event_listen (einit_power_reset_imminent, einit_ipc_9p_power_event_handler);
 event_listen (einit_ipc_read, einit_ipc_9p_ipc_read);
 event_listen (einit_ipc_stat, einit_ipc_9p_ipc_stat);

 return 0;
}
