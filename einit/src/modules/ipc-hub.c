/*
 *  ipc-hub.c
 *  einit
 *
 *  Created by Magnus Deininger on 31/10/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <errno.h>

#include <einit/module.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <einit-modules/ipc.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_ipc_hub_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_ipc_hub_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT IPC Hub",
 .rid       = "einit-ipc-hub",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_ipc_hub_configure
};

module_register(einit_ipc_hub_self);

#endif

#ifdef LINUX
#define HUB_SOCKET_NAME_TEMPLATE " /einit/hub/%i"
#else
#define HUB_SOCKET_NAME_TEMPLATE "/tmp/einit/hub/%i"
#endif

#define MAX_PACKET_SIZE 4096

enum hub_connection_type {
 hct_socket,
 hct_client
};

struct hub_connection {
 int connection;
 enum hub_connection_type type;
};

struct hub_connection **einit_ipc_hub_connections_server = NULL;
struct hub_connection **einit_ipc_hub_connections_client = NULL;

pthread_mutex_t einit_ipc_hub_connections_server_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t einit_ipc_hub_connections_client_mutex = PTHREAD_MUTEX_INITIALIZER;

char einit_ipc_hub_connection_updated = 1;

void einit_ipc_hub_schedule_connection_update() {
 einit_ipc_hub_connection_updated = 1;
}

char *einit_ipc_hub_generate_socket_name() {
 char t[BUFFERSIZE];

 esprintf (t, BUFFERSIZE, HUB_SOCKET_NAME_TEMPLATE, geteuid());

 return estrdup (t);
}

char einit_ipc_hub_connect_socket (char *name) {
 int sock = socket (AF_UNIX, SOCK_STREAM, 0);
 struct sockaddr_un saddr;

 /* tag the fd as close-on-exec, just in case */
 fcntl (sock, F_SETFD, FD_CLOEXEC);
// fcntl (sock, F_SETFD, O_NONBLOCK);

 if (sock == -1) {
  perror ("einit-ipc-hub: initialising socket");
  return 0;
 }

 memset (&saddr, 0, sizeof(saddr));
 saddr.sun_family = AF_UNIX;
 strncpy (saddr.sun_path, name, sizeof(saddr.sun_path) - 1);

#ifdef LINUX
/* on linux, let's request an abstract socket */
 saddr.sun_path[0] = 0;
#endif

 if (connect (sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
  eclose (sock);
  perror ("einit-ipc-hub: connecting socket");
  return 0;
 }

 emutex_lock (&einit_ipc_hub_connections_client_mutex);
 struct hub_connection hc;
 hc.connection = sock;
 hc.type = hct_socket;
 einit_ipc_hub_connections_client = (struct hub_connection **)setadd ((void **)einit_ipc_hub_connections_client, &hc, sizeof (hc));
 emutex_unlock (&einit_ipc_hub_connections_client_mutex);

 fprintf (stderr, "\nconnection to running server OK\n");
 return 1;
}

void einit_ipc_hub_create_socket (char *name) {
 int sock = socket (AF_UNIX, SOCK_STREAM, 0);
 struct sockaddr_un saddr;
 struct stat st;

/* tag the fd as close-on-exec, just in case */
 fcntl (sock, F_SETFD, FD_CLOEXEC);
// fcntl (sock, F_SETFD, O_NONBLOCK);

 if (sock == -1) {
  perror ("einit-ipc-hub: initialising socket");

  return;
 }

#ifndef LINUX
 if (stat ("/tmp/einit", &st)) { mkdir ("/tmp/einit", 0770); }
 if (stat ("/tmp/einit/hub", &st)) { mkdir ("/tmp/einit/hub", 0770); }
 if (unlink (name));
#endif

 memset (&saddr, 0, sizeof(saddr));
 saddr.sun_family = AF_UNIX;
 strncpy (saddr.sun_path, name, sizeof(saddr.sun_path) - 1);

#ifdef LINUX
/* on linux, let's request an abstract socket */
 saddr.sun_path[0] = 0;
#endif

 if (bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
#ifdef LINUX
  unlink (saddr.sun_path);
  if (bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
#endif
   eclose (sock);
   perror ("einit-ipc-hub: binding socket");

   return;
#ifdef LINUX
  }
#endif
 }

 if (listen (sock, 5)) {
  eclose (sock);
  perror ("einit-ipc-hub: listening on socket");

  return;
 }

 emutex_lock (&einit_ipc_hub_connections_server_mutex);
 struct hub_connection hc;
 hc.connection = sock;
 hc.type = hct_socket;
 einit_ipc_hub_connections_server = (struct hub_connection **)setadd ((void **)einit_ipc_hub_connections_server, &hc, sizeof (hc));
 emutex_unlock (&einit_ipc_hub_connections_server_mutex);

 fprintf (stderr, "new server socket created\n");

 return;
}

void einit_ipc_hub_connect () {
 char have_socket = 0;
 int i;

 emutex_lock (&einit_ipc_hub_connections_server_mutex);
 if (einit_ipc_hub_connections_server) {
  for (i = 0; einit_ipc_hub_connections_server[i]; i++) {
   if (einit_ipc_hub_connections_server[i]->type == hct_socket) {
    have_socket = 1;
   }
  }
 }
 emutex_unlock (&einit_ipc_hub_connections_server_mutex);

 emutex_lock (&einit_ipc_hub_connections_client_mutex);
 if (einit_ipc_hub_connections_client) {
  for (i = 0; einit_ipc_hub_connections_client[i]; i++) {
   if (einit_ipc_hub_connections_client[i]->type == hct_socket) {
    have_socket = 1;
   }
  }
 }
 emutex_unlock (&einit_ipc_hub_connections_client_mutex);

 if (!have_socket) {
  char *socket_name;

  if ((socket_name = einit_ipc_hub_generate_socket_name())) {
   if (!einit_ipc_hub_connect_socket (socket_name)) {
    einit_ipc_hub_create_socket (socket_name);
   }

   free (socket_name);
  }
 }
}

void einit_ipc_hub_handle_block (char *cbuf, ssize_t tlen) {
 char **messages = str2set ('\x4', cbuf);
 ssize_t i;

 free (cbuf);

 if (messages) {
  for (; messages[i]; i++) {
   char **fields = str2set ('\x4', messages[i]);
   ssize_t j;
   struct einit_event ev = evstaticinit (0);

   if (fields) {
    for (j = 0; fields[j]; j++) {
/* this crashes us... presumably because we're never transmitting values for stringsets */
     if (strstr (fields[j], "event:") == fields[j]) {
      ev.type = parse_integer (fields[j] + 6); // add "event:"
     } else if (strstr (fields[j], "int:") == fields[j]) {
      ev.integer = parse_integer (fields[j] + 4); // add "int:"
     } else if (strstr (fields[j], "task:") == fields[j]) {
      ev.task = parse_integer (fields[j] + 5); // add "task:"
     } else if (strstr (fields[j], "status:") == fields[j]) {
      ev.status = parse_integer (fields[j] + 7); // add "status:"
     } else if (strstr (fields[j], "command:") == fields[j]) {
      ev.command = estrdup (fields[j] + 8); // add "command:"
     } else if (strstr (fields[j], "string:") == fields[j]) {
      ev.string = estrdup (fields[j] + 7); // add "string:"
     } else if (strstr (fields[j], "argv:") == fields[j]) {
      ev.argv = str2set ('\x1d', (const char *)(fields[j] + 5)); // add "argv:"
     } else if (strstr (fields[j], "stringset:") == fields[j]) {
      ev.stringset = str2set ('\x1d', (const char *)(fields[j] + 10)); // add "stringset:"
     }
    }
    free (fields);
   }

   if (ev.type == einit_event_subsystem_ipc) {
    if (ev.argv) {
	 ev.argc = setcount ((const void **)ev.argv);
     event_emit (&ev, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
	}
   } else {
    event_emit (&ev, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
   }
  }

  free (messages);
 }
}

void *einit_ipc_hub_thread (void *irr) {
 struct pollfd *pfd = NULL;
 nfds_t nfds = 0;

 while (1) {
  if (einit_ipc_hub_connection_updated) {
   struct pollfd **pfdx = NULL;

   if (pfd) {
    free (pfd);
   }
   nfds = 0;

   einit_ipc_hub_connect();

   emutex_lock (&einit_ipc_hub_connections_server_mutex);
   if (einit_ipc_hub_connections_server) {
    int i;
    for (i = 0; einit_ipc_hub_connections_server[i]; i++) {
     struct pollfd p;

     memset (&p, 0, sizeof(p));
     p.fd = einit_ipc_hub_connections_server[i]->connection;
     p.events = POLLIN;

     pfdx = (struct pollfd **)setadd ((void **)pfdx, &p, sizeof (p));
    }
   }
   emutex_unlock (&einit_ipc_hub_connections_server_mutex);

   emutex_lock (&einit_ipc_hub_connections_client_mutex);
   if (einit_ipc_hub_connections_client) {
    int i;
    for (i = 0; einit_ipc_hub_connections_client[i]; i++) {
     struct pollfd p;

     memset (&p, 0, sizeof(p));
     p.fd = einit_ipc_hub_connections_client[i]->connection;
     p.events = POLLIN;

     pfdx = (struct pollfd **)setadd ((void **)pfdx, &p, sizeof (p));
    }
   }
   emutex_unlock (&einit_ipc_hub_connections_client_mutex);

   if (pfdx) {
    int i;
    nfds = setcount ((const void **)pfdx);
    pfd = ecalloc (sizeof (struct pollfd), nfds);

    for (i = 0; pfdx[i]; i++) {
     memcpy (pfd + i, pfdx[i], sizeof (struct pollfd));
    }

    free (pfdx);
   }
  }

  if (pfd) {
   int j, c;

   for (j = 0; j < nfds; j++) {
    pfd[j].revents = 0;
   }

   if ((c = poll (pfd, nfds, 5))) {
    emutex_lock (&einit_ipc_hub_connections_server_mutex);
    rescan_server:
    if (einit_ipc_hub_connections_server) {
     int i;
     for (i = 0; einit_ipc_hub_connections_server[i]; i++) {
      for (j = 0; j < nfds; j++) {
       if (einit_ipc_hub_connections_server[i]->connection == pfd[j].fd) {
        if (pfd[j].revents & POLLIN) {
         int nf = accept (pfd[j].fd, NULL, NULL);

         if (nf) {
          emutex_lock (&einit_ipc_hub_connections_client_mutex);
          struct hub_connection hc;
          hc.connection = nf;
          hc.type = hct_client;
          einit_ipc_hub_connections_client = (struct hub_connection **)setadd ((void **)einit_ipc_hub_connections_client, &hc, sizeof (hc));
          emutex_unlock (&einit_ipc_hub_connections_client_mutex);

          fprintf (stderr, "new client connected.\n");

          einit_ipc_hub_schedule_connection_update();

          fcntl (nf, F_SETFD, FD_CLOEXEC);
//          fcntl (nf, F_SETFD, O_NONBLOCK);
         }
        }
        if (pfd[j].revents & (POLLERR | POLLHUP | POLLNVAL)) {
         einit_ipc_hub_connections_server = (struct hub_connection **)setdel ((void **)einit_ipc_hub_connections_server, einit_ipc_hub_connections_server[i]);

         eclose (pfd[j].fd);
         fprintf (stderr, "server connection killed.\n");

         goto rescan_server;
        }
       }
      }
     }
    }
    emutex_unlock (&einit_ipc_hub_connections_server_mutex);

    emutex_lock (&einit_ipc_hub_connections_client_mutex);
    rescan_client:
    if (einit_ipc_hub_connections_client) {
     int i;
     for (i = 0; einit_ipc_hub_connections_client[i]; i++) {
      for (j = 0; j < nfds; j++) {
       if (einit_ipc_hub_connections_client[i]->connection == pfd[j].fd) {
        if ((einit_ipc_hub_connections_client[i]->type == hct_client) && (pfd[j].revents & POLLIN)) {
         char buf[MAX_PACKET_SIZE];
		 char *cbuf = NULL;
         ssize_t l = 0;
         ssize_t tlen = 1;
         ssize_t cp = 0;

         memset (buf, 0, MAX_PACKET_SIZE);

         errno = 0;
		 do {
          if ((l = recv (pfd[j].fd, buf, MAX_PACKET_SIZE-1, MSG_DONTWAIT)) > 0) {
		   tlen += l;
		   if (!cbuf) {
		    cbuf = emalloc (tlen);
		   } else {
		    cbuf = erealloc (cbuf, tlen);
		   }

		   memcpy (cbuf + cp, buf, l);

		   cp += l;
          }
		 } while (!errno && (l > 0));

	     if (cbuf) {
		  cbuf [tlen-1] = 0;
		  
		  einit_ipc_hub_handle_block (cbuf, tlen);
		 }
//         if ((l == -1) && (errno != EAGAIN)) {
         if (errno != EAGAIN) {
          einit_ipc_hub_connections_client = (struct hub_connection **)setdel ((void **)einit_ipc_hub_connections_client, einit_ipc_hub_connections_client[i]);

          eclose (pfd[j].fd);
          perror ("client connection closed");

          goto rescan_client;
         }
        }
        if (pfd[j].revents & (POLLERR | POLLHUP | POLLNVAL)) {
         einit_ipc_hub_connections_client = (struct hub_connection **)setdel ((void **)einit_ipc_hub_connections_client, einit_ipc_hub_connections_client[i]);

         eclose (pfd[j].fd);
         fprintf (stderr, "client connection closed.\n");

         goto rescan_client;
        }
       }
      }
     }
    }
    emutex_unlock (&einit_ipc_hub_connections_client_mutex);
   }
  } else
   sleep (2);
 }

/* accept connections and spawn (detached) subthreads. */
/* while ((nfd = accept (sock, NULL, NULL))) {
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
  perror ("einit-ipc-hub: accepting connections");

 eclose (sock);
 if (unlink (saddr.sun_path)) perror ("einit-ipc-hub: removing socket");*/

 return NULL;
}

void einit_ipc_hub_send_event (struct einit_event *ev) {
 int *targets = NULL;
 int c = 0;
 int i;

 emutex_lock (&einit_ipc_hub_connections_client_mutex);
 if (einit_ipc_hub_connections_client) {
  c = setcount ((const void **)einit_ipc_hub_connections_client);
  targets = malloc (c*sizeof(int));
  for (i = 0; einit_ipc_hub_connections_client[i]; i++) {
   if (einit_ipc_hub_connections_client[i]->type == hct_socket) {
    targets[i] = einit_ipc_hub_connections_client[i]->connection;
   }
  }
 }
 emutex_unlock (&einit_ipc_hub_connections_client_mutex);

 if (targets) {
  for (i = 0; i < c; i++) {
   char buffer[MAX_PACKET_SIZE];
   char buffer2[MAX_PACKET_SIZE];

/* we're using ascii record separators here... */
   esprintf (buffer, MAX_PACKET_SIZE, "event:%i" "\x1e", ev->type);

   if (ev->integer) {
    esprintf (buffer2, MAX_PACKET_SIZE, "int:%i" "\x1e", ev->integer);
    if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
      notice (1, "ipc-hub: event packet too big.");
     continue;
    } else {
     strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
 	}
   }
   if (ev->task) {
    esprintf (buffer2, MAX_PACKET_SIZE, "task:%i" "\x1e", ev->task);
/*    if (strlcat(buffer, buffer2, MAX_PACKET_SIZE) >= MAX_PACKET_SIZE) {
      notice (1, "ipc-hub: event packet too big.");
     continue;
    }*/
    if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
      notice (1, "ipc-hub: event packet too big.");
     continue;
    } else {
     strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
 	}
   }
   if (ev->status) {
    esprintf (buffer2, MAX_PACKET_SIZE, "status:%i" "\x1e", ev->status);
/*    if (strlcat(buffer, buffer2, MAX_PACKET_SIZE) >= MAX_PACKET_SIZE) {
      notice (1, "ipc-hub: event packet too big.");
     continue;
    }*/
    if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
      notice (1, "ipc-hub: event packet too big.");
     continue;
    } else {
     strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
 	}
   }

   if (ev->task == einit_event_subsystem_ipc) {
    if (ev->command) {
     esprintf (buffer2, MAX_PACKET_SIZE, "command:%s" "\x1e", ev->command);
     if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
       notice (1, "ipc-hub: event packet too big.");
      continue;
     } else {
      strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
     }
    } else if (ev->argv) {
/* ASCII group separators here... */
	 char *r = set2str ('\x1d', (const char **)ev->argv);
     esprintf (buffer2, MAX_PACKET_SIZE, "argv:%s" "\x1e", r);
	 free (r);
     if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
       notice (1, "ipc-hub: event packet too big.");
      continue;
     } else {
      strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
     }
    }
   } else {
    if (ev->string) {
     esprintf (buffer2, MAX_PACKET_SIZE, "string:%s" "\x1e", ev->string);
     if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
       notice (1, "ipc-hub: event packet too big.");
      continue;
     } else {
      strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
     }
    } else if (ev->stringset) {
/* ASCII group separators here... */
	 char *r = set2str ('\x1d', (const char **)ev->stringset);
     esprintf (buffer2, MAX_PACKET_SIZE, "stringset:%s" "\x1e", r);
	 free (r);
     if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
       notice (1, "ipc-hub: event packet too big.");
      continue;
     } else {
      strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
     }
    }
   }

/* last byte in the message is an ascii end-of-transmission char */
   snprintf (buffer2, MAX_PACKET_SIZE, "\x4");
/*    if (strlcat(buffer, buffer2, MAX_PACKET_SIZE) >= MAX_PACKET_SIZE) {
      notice (1, "ipc-hub: event packet too big.");
     continue;
    }*/
    if ((strlen (buffer) + strlen(buffer2) + 1) >= MAX_PACKET_SIZE) {
      notice (1, "ipc-hub: event packet too big.");
     continue;
    } else {
     strncat(buffer, buffer2, MAX_PACKET_SIZE - strlen (buffer) - 1);
 	}

   send (targets[i], buffer, strlen(buffer), 0);
  }

  free (targets);
 }
}

void einit_ipc_hub_generic_event_handler (struct einit_event *ev) {
 einit_ipc_hub_send_event (ev);
}

int einit_ipc_hub_cleanup (struct lmodule *tm) {
 ipc_cleanup (tm);

 event_listen (einit_event_subsystem_any, einit_ipc_hub_generic_event_handler);

 return 0;
}

int einit_ipc_hub_configure (struct lmodule *tm) {
// pthread_t th;

 module_init (tm);
 ipc_configure (tm);

 event_listen (einit_event_subsystem_any, einit_ipc_hub_generic_event_handler);

 tm->cleanup = einit_ipc_hub_cleanup;

// ethread_create (&th, &thread_attribute_detached, einit_ipc_hub_thread, NULL);

 return 1;
}
