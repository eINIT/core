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

 if (sock == -1) {
  perror ("einit-ipc-hub: initialising socket");

  fprintf (stderr, "\nno connection to any running server...\n");
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

  fprintf (stderr, "\nno connection to any running server...\n");
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

/* tag the fd as close-on-exec, just in case */
 fcntl (sock, F_SETFD, FD_CLOEXEC);

 if (sock == -1) {
  perror ("einit-ipc-hub: initialising socket");

  return;
 }

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
   perror ("einit-ipc-hub: connecting socket");

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
         ssize_t l = 0;

         if ((l = recv (pfd[j].fd, buf, MAX_PACKET_SIZE, MSG_DONTWAIT)) > 0) {
         }
         if ((l == -1) && (errno != EAGAIN)) {
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

int einit_ipc_hub_cleanup (struct lmodule *tm) {
 ipc_cleanup (tm);

 return 0;
}

int einit_ipc_hub_configure (struct lmodule *tm) {
// pthread_t th;

 module_init (tm);
 ipc_configure (tm);

 tm->cleanup = einit_ipc_hub_cleanup;

/* ethread_create (&th, &thread_attribute_detached, einit_ipc_hub_thread, NULL);

 sleep (50);*/
 return 1;
}
