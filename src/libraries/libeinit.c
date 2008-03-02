/*
 *  libeinit.c
 *  einit
 *
 *  Created by Magnus Deininger on 24/07/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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

#include <ixp_local.h>

#include <einit/event.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <fcntl.h>

#ifdef estrdup
#undef estrdup
#endif
#ifdef emalloc
#undef emalloc
#endif
#ifdef ecalloc
#undef ecalloc
#endif

#ifdef __APPLE__
/* dammit, what's wrong with macos!? */

struct exported_function *cfg_addnode_fs = NULL;
struct exported_function *cfg_findnode_fs = NULL;
struct exported_function *cfg_getstring_fs = NULL;
struct exported_function *cfg_getnode_fs = NULL;
struct exported_function *cfg_filter_fs = NULL;
struct exported_function *cfg_getpath_fs = NULL;
struct exported_function *cfg_prefix_fs = NULL;

char einit_new_node = 0;
struct utsname osinfo = {};
#endif

char **einit_initial_environment = NULL;
char **einit_global_environment = NULL;
char **einit_argv = NULL;

struct stree *exported_functions = NULL;

struct cfgnode *cmode = NULL, *amode = NULL;
enum einit_mode coremode = einit_mode_init;

void einit_power_down () {
 einit_switch_mode ("power-down");
}

void einit_power_reset () {
 einit_switch_mode ("power-reset");
}

void einit_switch_mode (const char *mode) { // think "runlevel"
 char *path[2];
 path[0] = "mode";
 path[1] = NULL;

 einit_write (path, mode);
}

/* client */

char *einit_ipc_address = "unix!/dev/einit-9p";
IxpClient *einit_ipc_9p_client = NULL;
pid_t einit_ipc_9p_client_pid = 0;

char einit_connect(int *argc, char **argv) {
 char *envvar = getenv ("EINIT_9P_ADDRESS");
 char priv = 0;
 if (envvar)
  einit_ipc_address = envvar;

 if (argc && argv) {
  int i = 0;
  for (i = 1; i < *argc; i++) {
   if (argv[i][0] == '-')
    switch (argv[i][1]) {
     case 'p':
      priv = 1;
      break;
     case 'a':
      if ((++i) < (*argc))
       einit_ipc_address = argv[i];
      break;
    }
  }
 }

 if (!einit_ipc_address || !einit_ipc_address[0])
  einit_ipc_address = "unix!/dev/einit-9p";

// einit_ipc_9p_fd = ixp_dial (einit_ipc_address);
 if (priv) {
  return einit_connect_spawn(argc, argv);
 } else {
  einit_ipc_9p_client = ixp_mount (einit_ipc_address);
 }

 return (einit_ipc_9p_client ? 1 : 0);
}

char einit_connect_spawn(int *argc, char **argv) {
 char sandbox = 0;

 if (argc && argv) {
  int i = 0;
  for (i = 1; i < *argc; i++) {
   if (argv[i][0] == '-')
    switch (argv[i][1]) {
     case 'p':
      if (argv[i][2] == 's') sandbox = 1;
      break;
    }
  }
 }

 char address[BUFFERSIZE];
 char filename[BUFFERSIZE];
 struct stat st;

 snprintf (address, BUFFERSIZE, "unix!/tmp/einit.9p.%i", getpid());
 snprintf (filename, BUFFERSIZE, "/tmp/einit.9p.%i", getpid());

 int fd = 0;

 einit_ipc_9p_client_pid = fork();

 switch (einit_ipc_9p_client_pid) {
  case -1:
   return 0;
   break;
  case 0:
   fd = open ("/dev/null", O_RDWR);
   if (fd) {
    close (0);
    close (1);
    close (2);

    dup2 (fd, 0);
    dup2 (fd, 1);
    dup2 (fd, 2);

    close (fd);
   }

   execl (EINIT_LIB_BASE "/bin/einit-core", "einit-core", "--ipc-socket", address, "--do-wait", (sandbox ? "--sandbox" : NULL), NULL);

   exit (EXIT_FAILURE);
   break;
  default:
   while (stat (filename, &st)) sched_yield();

   einit_ipc_9p_client = ixp_mount (address);

   unlink (filename);

   return (einit_ipc_9p_client ? 1 : 0);
   break;
 }
}

char einit_disconnect() {
 if (einit_ipc_9p_client_pid > 0) {
/* we really gotta do this in a cleaner way... */
  kill (einit_ipc_9p_client_pid, SIGKILL);

  waitpid (einit_ipc_9p_client_pid, NULL, 0);
 }

 ixp_unmount (einit_ipc_9p_client);
 return 1;
}

char *einit_render_path (char **path) {
 if (path) {
  char *rv = NULL;
  char *r = set2str ('/', (const char **)path);

  rv = emalloc (strlen (r) + 2);
  rv[0] = '/';
  rv[1] = 0;

  strcat (rv, r);

  efree (r);

  return rv;
 } else {
  return estrdup ("/");
 }
}

char **einit_ls (char **path) {
 char **rv = NULL;

 IxpMsg m;
 Stat *stat;
 IxpCFid *fid;
 char *file, *buf;
 int count, nstat, mstat, i;

 file = einit_render_path(path);

 stat = ixp_stat(einit_ipc_9p_client, file);

 if ((stat->mode&P9_DMDIR) == 0) {
  return NULL;
 }
 ixp_freestat(stat);

 fid = ixp_open(einit_ipc_9p_client, file, P9_OREAD);

 if (!fid) return NULL;

 nstat = 0;
 mstat = 16;
 stat = emalloc(sizeof(*stat) * mstat);
 buf = emalloc(fid->iounit);
 while((count = ixp_read(fid, buf, fid->iounit)) > 0) {
  m = ixp_message((void *)buf, count, MsgUnpack);
  while(m.pos < m.end) {
   if(nstat == mstat) {
    mstat <<= 1;
    stat = erealloc(stat, sizeof(*stat) * mstat);
   }
   ixp_pstat(&m, &stat[nstat++]);
  }
 }

 for(i = 0; i < nstat; i++) {
  if ((stat[i].mode&P9_DMDIR) == 0) {
   rv = set_str_add (rv, stat[i].name);
  } else {
   size_t len = strlen (stat[i].name) + 2;
   char *x = emalloc (len);
   snprintf (x, len, "%s/", stat[i].name);

   rv = set_str_add (rv, x);
   efree (x);
  }
  ixp_freestat(&stat[i]);
 }

 efree(stat);
 efree (file);

 if (rv) {
  rv = strsetdel (rv, "./");
  rv = strsetdel (rv, "../");
 }

 return rv;
}

char *einit_read (char **path) {
 char *buffer = einit_render_path (path);
 char *data = NULL;

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

  do {
//   fprintf (stderr, "reading.\n");
   buf = realloc (buf, blen + f->iounit);
   if (buf == NULL) {
    ixp_close (f);
    return NULL;
   }
//   fprintf (stderr, ".\n");

   rn = ixp_read (f, (char *)(buf + blen), f->iounit);
   if (rn > 0) {
//    write (1, buf + blen, rn);
    blen = blen + rn;
   }
  } while (rn > 0);

//  fprintf (stderr, "done.\n");

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

  ixp_close (f);
 }

 efree (buffer);

 return data;
}

int einit_read_callback (char **path, int (*callback)(char *, size_t, void *), void *cdata) {
 char *buffer = einit_render_path (path);

 IxpCFid *f = ixp_open (einit_ipc_9p_client, buffer, P9_OREAD);

 if (f) {
  intptr_t rn = 0;
  void *buf = NULL;
  intptr_t blen = 0;

  buf = malloc (f->iounit);
  if (!buf) {
   ixp_close (f);
   return 0;
  }

  do {
   buf = realloc (buf, blen + f->iounit);
   if (buf == NULL) {
    ixp_close (f);
    return 0;
   }

   rn = ixp_read (f, (char *)(buf + blen), f->iounit);
   if (rn > 0) {
    blen = blen + rn;
   }

   if ((rn < f->iounit) && blen) {
    *(((char *)buf) + blen) = 0;
    callback (buf, blen, cdata);
    blen = 0;
   }
  } while (rn > 0);

  ixp_close (f);
 }

 efree (buffer);

 return 0;
}

int einit_read_callback_limited (char **path, int (*callback)(char *, size_t, void *), void *cdata, int fragments) {
 char *buffer = einit_render_path (path);

 IxpCFid *f = ixp_open (einit_ipc_9p_client, buffer, P9_OREAD);

 if (f) {
  intptr_t rn = 0;
  void *buf = NULL;
  intptr_t blen = 0;

  buf = malloc (f->iounit);
  if (!buf) {
   ixp_close (f);
   return 0;
  }

  do {
   buf = realloc (buf, blen + f->iounit);
   if (buf == NULL) {
    ixp_close (f);
    return 0;
   }

   rn = ixp_read (f, (char *)(buf + blen), f->iounit);
   if (rn > 0) {
    blen = blen + rn;
   }

   if ((rn < f->iounit) && blen) {
    *(((char *)buf) + blen) = 0;
    callback (buf, blen, cdata);
    blen = 0;
    fragments--;

    if (fragments <= 0) {
     ixp_close (f);
     efree (buffer);
     efree (buf);
     return 0;
    }
   }
  } while (rn > 0);

  ixp_close (f);
 }

 efree (buffer);

 return 0;
}

int einit_write (char **path, const char *data) {
 if (!data) return 0;

 char *buffer = einit_render_path (path);

 IxpCFid *f = ixp_open (einit_ipc_9p_client, buffer, P9_OWRITE);

 if (f) {
  ixp_write(f, (char *)data, strlen(data));

  ixp_close (f);
 }

 efree (buffer);
 return 0;
}

void einit_service_call (const char *service, const char *action) {
 const char *path[5];
 path[0] = "services";
 path[1] = "all";
 path[2] = service;
 path[3] = "status";
 path[4] = NULL;

 einit_write ((char **)path, action);
}

void einit_module_call (const char *rid, const char *action) {
 const char *path[5];
 path[0] = "modules";
 path[1] = "all";
 path[2] = rid;
 path[3] = "status";
 path[4] = NULL;

 einit_write ((char **)path, action);
}

char * einit_module_get_attribute (const char *rid, const char *attribute) {
 const char *path[5];
 path[0] = "modules";
 path[1] = "all";
 path[2] = rid;
 path[3] = attribute;
 path[4] = NULL;

 char *rv = einit_read ((char **)path);
 if (rv)
  strtrim (rv);

 return rv;
}

char * einit_module_get_name (const char *rid) {
 char *data = einit_module_get_attribute (rid, "name");
 if (data) {
  char *rv = (char *)str_stabilise(data);
  efree (data);
  return rv;
 }

 return NULL;
}

char ** einit_module_get_provides (const char *rid) {
 char *data = einit_module_get_attribute (rid, "provides");
 if (data) {
  char **rv = str2set ('\n', data);
  char **nrv = set_str_dup_stable (rv);
  efree (rv);
  efree (data);
  return nrv;
 }

 return NULL;
}

char ** einit_module_get_requires (const char *rid) {
 char *data = einit_module_get_attribute (rid, "requires");
 if (data) {
  char **rv = str2set ('\n', data);
  char **nrv = set_str_dup_stable (rv);
  efree (rv);
  efree (data);
  return nrv;
 }

 return NULL;
}

char ** einit_module_get_after (const char *rid) {
 char *data = einit_module_get_attribute (rid, "after");
 if (data) {
  char **rv = str2set ('\n', data);
  char **nrv = set_str_dup_stable (rv);
  efree (rv);
  efree (data);
  return nrv;
 }

 return NULL;
}

char ** einit_module_get_before (const char *rid) {
 char *data = einit_module_get_attribute (rid, "before");
 if (data) {
  char **rv = str2set ('\n', data);
  char **nrv = set_str_dup_stable (rv);
  efree (rv);
  efree (data);
  return nrv;
 }

 return NULL;
}

char ** einit_module_get_status (const char *rid) {
 char *data = einit_module_get_attribute (rid, "status");
 if (data) {
  char **rv = str2set ('\n', data);
  char **nrv = set_str_dup_stable (rv);
  efree (rv);
  efree (data);
  return nrv;
 }

 return NULL;
}

char ** einit_module_get_options (const char *rid) {
 char *data = einit_module_get_attribute (rid, "options");
 if (data) {
  char **rv = str2set ('\n', data);
  char **nrv = set_str_dup_stable (rv);
  efree (rv);
  efree (data);
  return nrv;
 }

 return NULL;
}

int einit_event_loop_decoder (char *fragment, size_t size, void *data) {
 char **buffer = str2set ('\n', fragment);
 int i = 0;
 struct einit_event *ev = evinit(0);

 for (; buffer[i]; i++) {
  if (strprefix (buffer[i], "event-type=")) {
   if (strcmp ((buffer[i])+11, "unknown/custom")) {
    ev->type = event_string_to_code((buffer[i])+11);
   } else {
    goto cleanup_exit;
   }
  } else if (strprefix (buffer[i], "integer=")) {
   ev->integer = parse_integer ((buffer[i])+8);
  } else if (strprefix (buffer[i], "task=")) {
   ev->task = parse_integer ((buffer[i])+5);
  } else if (strprefix (buffer[i], "status=")) {
   ev->status = parse_integer ((buffer[i])+7);
  } else if (strprefix (buffer[i], "flag=")) {
   ev->flag = parse_integer ((buffer[i])+5);
  } else if (strprefix (buffer[i], "module=")) {
   ev->rid = (char *)str_stabilise ((buffer[i])+7);
  } else if (strprefix (buffer[i], "string=")) {
   ev->string = (char *)str_stabilise ((buffer[i])+7);
  } else if (strprefix (buffer[i], "stringset=")) {
   ev->stringset = set_str_add_stable (ev->stringset, (buffer[i])+10);
  }
 }

// fprintf (stderr, "new fragment: %s\n", fragment);

 if (ev->type) {
  event_emit (ev, einit_event_flag_broadcast);
 }

 cleanup_exit:

 if (ev->stringset)
  efree(ev->stringset);

 evdestroy (ev);
 efree (buffer);

 return 0;
}

void einit_event_loop () {
 char *path[3] = { "events", "feed", NULL };
 einit_read_callback (path, einit_event_loop_decoder, NULL);
}

void einit_replay_events() {
 char *path[3] = { "events", "count", NULL };
 char *count_s = einit_read (path);

 if (count_s) {
  int count = parse_integer (count_s);

  path[1] = "feed";
  einit_read_callback_limited (path, einit_event_loop_decoder, NULL, count);

  efree (count_s);
 }
}
