/*
 *  ipc-dbus.c
 *  einit
 *
 *  Created by Magnus Deininger on 05/09/2006.
 *  Converted from C++ to C on 31/10/2007
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>
#include <dbus/dbus.h>

#include <einit-modules/ipc.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_dbus_cleanup (struct lmodule *);
int einit_ipc_dbus_enable (void *pa, struct einit_event *status);
int einit_ipc_dbus_disable (void *pa, struct einit_event *status);

int einit_dbus_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * einit_dbus_provides[] = {"ipc-dbus", NULL};
char * einit_dbus_requires[] = {"dbus", NULL};
const struct smodule einit_dbus_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "eINIT <-> DBUS connector",
 .rid       = "einit-ipc-dbus",
 .si        = {
  .provides = einit_dbus_provides,
  .requires = einit_dbus_requires,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_dbus_configure
};

module_register(einit_dbus_self);

#endif

DBusError einit_dbus_error;
DBusConnection *einit_dbus_connection;
dbus_uint32_t einit_dbus_sequence = 1;
char einit_dbus_terminate_thread;
char einit_dbus_active = 0;
pthread_mutex_t einit_dbus_sequence_mutex = PTHREAD_MUTEX_INITIALIZER;

const char * einit_dbus_introspection_data =
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
  "<node name=\"/org/einit/einit\">"
   "<interface name=\"org.einit.Einit\">"
    "<method name=\"Configure\">"
    "</method>"
    "<method name=\"EventEmit\">"
     "<arg type=\"s\" direction=\"in\" />"
     "<arg type=\"s\" direction=\"out\" />"
    "</method>"
   "</interface>"
  "</node>";

void *einit_dbus_handle_event_bootstrap (DBusMessage *);
int einit_ipc_dbus_enable (void *, struct einit_event *);
int einit_ipc_dbus_disable (void *, struct einit_event *);
int einit_dbus_cleanup (struct lmodule *);
void einit_dbus_signal_dbus (const char *);
void einit_dbus_generic_event_handler (struct einit_event *);
DBusMessage *einit_dbus_create_event_message (DBusMessage *, struct einit_event *);
void *einit_dbus_message_thread(void *);
void *einit_dbus_handle_event (DBusMessage *);
void *einit_dbus_ipc_spawn_safe(DBusMessage *);
void *einit_dbus_ipc_spawn(DBusMessage *);

int einit_dbus_configure(struct lmodule *irr) {
 module_init (irr);

 thismodule->enable = einit_ipc_dbus_enable;
 thismodule->disable = einit_ipc_dbus_disable;
 thismodule->cleanup = einit_dbus_cleanup;

 event_listen (einit_event_subsystem_any, einit_dbus_generic_event_handler);

 dbus_threads_init_default();

 dbus_error_init(&(einit_dbus_error));

 return 0;
}

void einit_dbus_signal_dbus (const char *IN_string) {
 DBusMessage *message;
 DBusMessageIter argv;

 message = dbus_message_new_signal("/org/einit/einit", "org.einit.Einit.Information", "EventString");
 if (!message) { return; }

 dbus_message_iter_init_append(message, &argv);
 if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &IN_string)) { return; }

 emutex_lock (&einit_dbus_sequence_mutex);
 einit_dbus_sequence++;
 /* if (!dbus_connection_send(einit_dbus_connection, message, &einit_dbus_sequence)) { emutex_unlock (&einit_dbus_sequence_mutex); return; }*/
 dbus_connection_send(einit_dbus_connection, message, &einit_dbus_sequence);
 emutex_unlock (&einit_dbus_sequence_mutex);
// dbus_connection_flush(einit_dbus_connection);
 dbus_message_unref(message);
}

void einit_dbus_broadcast_event (struct einit_event *ev) {
 DBusMessage *message;

 message = dbus_message_new_signal("/org/einit/einit", "org.einit.Einit.Information", "EventSignal");
 if (!message) { return; }

 message = einit_dbus_create_event_message (message, ev);
 if (!message) { return; }

 emutex_lock (&einit_dbus_sequence_mutex);
 einit_dbus_sequence++;
 dbus_connection_send(einit_dbus_connection, message, &einit_dbus_sequence);
 emutex_unlock (&einit_dbus_sequence_mutex);

 dbus_message_unref(message);
}

void einit_dbus_generic_event_handler (struct einit_event *ev) {
 if (einit_dbus_active) {
  uint32_t subsystem = ev->type & EVENT_SUBSYSTEM_MASK;

  if (subsystem != einit_event_subsystem_timer) // don't broadcast timer events.
   einit_dbus_broadcast_event (ev);
 }

 return;
}

int einit_dbus_dbus_connect () {
 const char *dbusname;
 const char *dbusaddress;
 int ret = 0;

#if 0
 if (!(dbusaddress = cfg_getstring("configuration-ipc-dbus-connection/address", NULL))) dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 if (!(dbusname = cfg_getstring("configuration-ipc-dbus-connection/name", NULL))) dbusname = "org.einit.Einit";
#else
 dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 dbusname = "org.einit.Einit";
#endif

 if (dbus_error_is_set(&(einit_dbus_error))) { 
  notice (2, "DBUS: Error (%s)\n", einit_dbus_error.message); 
  dbus_error_free(&(einit_dbus_error));
 }

// einit_dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &(einit_dbus_error));
 einit_dbus_connection = dbus_connection_open_private (dbusaddress, &(einit_dbus_error));
 if (dbus_error_is_set(&(einit_dbus_error))) { 
  notice (2, "DBUS: Connection Error (%s)\n", einit_dbus_error.message); 
  dbus_error_free(&(einit_dbus_error));
 }
 if (!einit_dbus_connection) return status_failed;

 if (dbus_bus_register (einit_dbus_connection, &(einit_dbus_error)) != TRUE) {
  if (dbus_error_is_set(&(einit_dbus_error))) { 
   notice(2, "DBUS: Registration Error (%s)\n", einit_dbus_error.message); 
   dbus_error_free(&(einit_dbus_error));
  }

  return status_failed;
 }

 ret = dbus_bus_request_name(einit_dbus_connection, dbusname, DBUS_NAME_FLAG_REPLACE_EXISTING, &(einit_dbus_error));
 if (dbus_error_is_set(&(einit_dbus_error))) { 
  notice(2, "DBUS: Name Error (%s)\n", einit_dbus_error.message); 
  dbus_error_free(&(einit_dbus_error));
 }
 if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) return status_failed;

 return status_ok;
}

int einit_dbus_enable (struct einit_event *status) {
 const char *dbusname;
 const char *dbusaddress;
 int ret = 0;

#if 0
 if (!(dbusaddress = cfg_getstring("configuration-ipc-dbus-connection/address", NULL))) dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 if (!(dbusname = cfg_getstring("configuration-ipc-dbus-connection/name", NULL))) dbusname = "org.einit.Einit";
#else
 dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 dbusname = "org.einit.Einit";
#endif

// einit_dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &(einit_dbus_error));
 einit_dbus_connection = dbus_connection_open_private (dbusaddress, &(einit_dbus_error));
 if (dbus_error_is_set(&(einit_dbus_error))) { 
  fbprintf(status, "DBUS: Connection Error (%s)\n", einit_dbus_error.message); 
  dbus_error_free(&(einit_dbus_error));
 }
 if (!einit_dbus_connection) return status_failed;

 if (dbus_bus_register (einit_dbus_connection, &(einit_dbus_error)) != TRUE) {
  if (dbus_error_is_set(&(einit_dbus_error))) { 
   fbprintf(status, "DBUS: Registration Error (%s)\n", einit_dbus_error.message); 
   dbus_error_free(&(einit_dbus_error));
  }

  return status_failed;
 }

 ret = dbus_bus_request_name(einit_dbus_connection, dbusname, DBUS_NAME_FLAG_REPLACE_EXISTING, &(einit_dbus_error));
 if (dbus_error_is_set(&(einit_dbus_error))) { 
  fbprintf(status, "DBUS: Name Error (%s)\n", einit_dbus_error.message); 
  dbus_error_free(&(einit_dbus_error));
 }
 if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) return status_failed;

 einit_dbus_terminate_thread = 0;
 einit_dbus_active = 1;

 ethread_spawn_detached (einit_dbus_message_thread, NULL);

 return status_ok;
}

int einit_dbus_disable (struct einit_event *status) {

 /* dbus_connection_flush(einit_dbus_connection);*/
 einit_dbus_terminate_thread = 1;
 einit_dbus_active = 0;

 return status_ok;
}

void *einit_dbus_message_thread(void *ign) {
 DBusMessage *message;

 while (1) {

  while (einit_dbus_connection && dbus_connection_read_write_dispatch(einit_dbus_connection, 100)) {
   if (!dbus_connection_get_is_connected (einit_dbus_connection)) break;
   message = dbus_connection_pop_message(einit_dbus_connection);

  if (message) {
// introspection support
   if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Introspectable", "Introspect")) {
    DBusMessage *reply;
    DBusMessageIter args;

    reply = dbus_message_new_method_return(message);

    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &einit_dbus_introspection_data)) { continue; }

    emutex_lock (&einit_dbus_sequence_mutex);
    einit_dbus_sequence++;
    if (!dbus_connection_send(einit_dbus_connection, reply, &(einit_dbus_sequence))) {
     emutex_unlock (&einit_dbus_sequence_mutex);
     continue; }
     emutex_unlock (&einit_dbus_sequence_mutex);

    dbus_message_unref(reply);
// 'old fashioned' ipc via dbus
   } else if (dbus_message_is_method_call(message, "org.einit.Einit.Information", "IPC")) {
    ethread_spawn_detached_run ((void *(*)(void *))einit_dbus_ipc_spawn_safe, message);
   } else if (dbus_message_is_method_call(message, "org.einit.Einit.Command", "IPC")) {
    ethread_spawn_detached_run ((void *(*)(void *))einit_dbus_ipc_spawn, message);
   } else if (dbus_message_is_method_call(message, "org.einit.Einit.Command", "EmitEvent")) {
    ethread_spawn_detached_run ((void *(*)(void *))einit_dbus_handle_event, message);
   }
  }

  if (einit_dbus_terminate_thread) {
   einit_dbus_active = 0;

   int r = 1;
   while ((r = sleep(r))) ; // wait a second before disconnecting

   dbus_connection_close(einit_dbus_connection); // close the connection after looping

   einit_dbus_connection = NULL;
   return NULL;
  }
 }

 einit_dbus_active = 0;

 notice (1, "dbus connection was dropped, suspending signals until we're reconnected!\n");
 fprintf (stderr, "dbus connection was dropped, suspending signals until we're reconnected!\n");
 fflush (stderr);

 int r = 5;
 while ((r = sleep(r))) ; // wait a couple of seconds before reconnecting to have everyone calm down a little

// einit_dbus_connection = NULL;

 einit_dbus_dbus_connect();

 if (einit_dbus_connection) einit_dbus_active = 1;
 }

 return NULL;
}

char *einit_dbus_ipc_request (char *command) {
 if (!command) return NULL;
  int internalpipe[2];
  char *returnvalue = NULL;

/*  if ((rv = pipe (internalpipe)) > 0) {*/
  if (socketpair (AF_UNIX, SOCK_STREAM, 0, internalpipe)) {
   ipc_process(command, stderr);
  } else /*if (rv == 0)*/ {
   char *buf = NULL;
   uint32_t blen = 0;
   int rn = 0;
   char *data = NULL;

// c'mon, don't tell me you're going to send data fragments > 40kb using the IPC interface!
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
//   FILE *r = fdopen (internalpipe[0], "r");

   ipc_process(command, w);

   fclose (w);

   errno = 0;
   if (internalpipe[0] != -1) {
    buf = (char *)emalloc (BUFFERSIZE*sizeof(char));
    blen = 0;
    do {
     buf = (char *)erealloc (buf, blen + BUFFERSIZE);
     if (buf == NULL) break;
     rn = read (internalpipe[0], (char *)(buf + blen), BUFFERSIZE);
     blen = blen + rn;
    } while (rn > 0);

    if (errno && (errno != EAGAIN))
     bitch(bitch_stdio, errno, "reading pipe failed.");

    eclose (internalpipe[0]);
    data = (char *)erealloc (buf, blen+1);
    data[blen] = 0;
    if (blen > 0) {
     *(data+blen-1) = 0;

     returnvalue = data;
    } else {
     free (data);
     data = NULL;
    }
   }
  }

  if (!returnvalue) returnvalue = estrdup("<einit-ipc><warning type=\"no-return-value\" /></einit-ipc>\n");

  return returnvalue;
}

void *einit_dbus_ipc_spawn(DBusMessage *message) {
 DBusMessage *reply;
 DBusMessageIter args;
 const char *command = "";

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n");
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) 
  fprintf(stderr, "Argument is not string!\n"); 
 else {
  char *returnvalue = NULL;
  dbus_message_iter_get_basic(&args, &command);

  returnvalue = einit_dbus_ipc_request ((char*)command);

  if (einit_dbus_connection) { /* make sure we're still connected after the ipc command */

   reply = dbus_message_new_method_return(message);

   dbus_message_iter_init_append(reply, &args);
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { free (returnvalue); return NULL; }

   free (returnvalue);

   emutex_lock (&einit_dbus_sequence_mutex);
   einit_dbus_sequence++;
   if (!dbus_connection_send(einit_dbus_connection, reply, &(einit_dbus_sequence))) { emutex_unlock (&einit_dbus_sequence_mutex); return NULL; }
   emutex_unlock (&einit_dbus_sequence_mutex);

   dbus_message_unref(reply);
  }
 }

 return NULL;
}

void *einit_dbus_ipc_spawn_safe(DBusMessage *message) {
 DBusMessage *reply;
 DBusMessageIter args;
 const char *command = "";

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n");
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) 
  fprintf(stderr, "Argument is not string!\n"); 
 else {
  char *returnvalue = NULL;
  dbus_message_iter_get_basic(&args, &command);

  if (!strmatch (command, "list modules --xml") && !strmatch (command, "list services --xml")) {
   reply = dbus_message_new_method_return(message);

   if (!returnvalue) returnvalue = (char *)"<einit-ipc><error type=\"unsafe-request\" /></einit-ipc>\n";

   dbus_message_iter_init_append(reply, &args);
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { return NULL; }

   emutex_lock (&einit_dbus_sequence_mutex);
   einit_dbus_sequence++;
   if (!dbus_connection_send(einit_dbus_connection, reply, &(einit_dbus_sequence))) { emutex_unlock (&einit_dbus_sequence_mutex); return NULL; }
   emutex_unlock (&einit_dbus_sequence_mutex);

   dbus_message_unref(reply);

   return NULL;
  }

  reply = dbus_message_new_method_return(message);

  returnvalue = einit_dbus_ipc_request ((char*)command);

  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { free (returnvalue); return NULL; }

  free (returnvalue);

  emutex_lock (&einit_dbus_sequence_mutex);
  einit_dbus_sequence++;
  if (!dbus_connection_send(einit_dbus_connection, reply, &(einit_dbus_sequence))) { emutex_unlock (&einit_dbus_sequence_mutex); return NULL; }
  emutex_unlock (&einit_dbus_sequence_mutex);

  dbus_message_unref(reply);
 }

 return NULL;
}

struct einit_event *einit_dbus_read_event (DBusMessage *message) {
 struct einit_event *ev = evinit (0);
 DBusMessageIter args;
 DBusMessageIter sub;

 if (!dbus_message_iter_init(message, &args)) { fprintf(stderr, "Message has no arguments!\n"); evdestroy(ev); return NULL; }
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32) { fprintf(stderr, "Argument has incorrect type (event type).\n"); evdestroy(ev); return NULL; }
 else dbus_message_iter_get_basic(&args, &(ev->type));

 if ((ev->type & EVENT_SUBSYSTEM_MASK) != einit_event_subsystem_ipc) {
  if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)) { fprintf(stderr, "Argument has incorrect type (integer).\n"); evdestroy(ev); return NULL; }
  else dbus_message_iter_get_basic(&args, &(ev->integer));

  if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)) { fprintf(stderr, "Argument has incorrect type (status).\n"); evdestroy(ev); return NULL; }
  else dbus_message_iter_get_basic(&args, &(ev->status));

  if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)) { fprintf(stderr, "Argument has incorrect type (task).\n"); evdestroy(ev); return NULL; }
  else dbus_message_iter_get_basic(&args, &(ev->task));

  if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT16)) { fprintf(stderr, "Argument has incorrect type (flag).\n"); evdestroy(ev); return NULL; }
  else { uint16_t r; dbus_message_iter_get_basic(&args, &(r)); ev->flag = r; }

  if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }

  if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
   dbus_message_iter_get_basic(&args, &(ev->string));
   ev->string = estrdup (ev->string);

   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }
  }
  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY) { fprintf(stderr, "Argument has incorrect type (string set).\n"); evdestroy(ev); return NULL; }

  dbus_message_iter_recurse (&args, &sub);
  while (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_INVALID) {
   char *value;

   if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING) { fprintf(stderr, "Argument has incorrect type (string set member).\n"); evdestroy(ev); return NULL; }
   dbus_message_iter_get_basic(&sub, &(value));

   ev->stringset = (char **)setadd ((void **)ev->stringset, (void *)value, SET_TYPE_STRING);

   dbus_message_iter_next (&sub);
  }
 } else {
  if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32)) { fprintf(stderr, "Argument has incorrect type (ipc-options).\n"); evdestroy(ev); return NULL; }
  else dbus_message_iter_get_basic(&args, &(ev->integer));

  if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }

  if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
   dbus_message_iter_get_basic(&args, &(ev->command));
   ev->command = estrdup (ev->command);

   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }
  }

  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY) { fprintf(stderr, "Argument has incorrect type (ipc argv).\n"); evdestroy(ev); return NULL; }
  dbus_message_iter_recurse (&args, &sub);
  while (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_INVALID) {
   char *value;

   if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING) { fprintf(stderr, "Argument has incorrect type (ipc argv member).\n"); evdestroy(ev); return NULL; }
   dbus_message_iter_get_basic(&sub, &(value));

   ev->argv = (char **)setadd ((void **)ev->argv, (void *)value, SET_TYPE_STRING);

   dbus_message_iter_next (&sub);
  }

  ev->argc = setcount ((const void **)ev->argv);
 }

 message_read:

 return ev;
}

DBusMessage *einit_dbus_create_event_message (DBusMessage *message, struct einit_event *ev) {
 DBusMessageIter argv;

 if (!ev) return NULL;

 dbus_message_iter_init_append(message, &argv);
 if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_UINT32, &(ev->type))) { return NULL; }
 if ((ev->type & EVENT_SUBSYSTEM_MASK) != einit_event_subsystem_ipc) {
  if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_INT32, &(ev->integer))) { return NULL; }
  if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_INT32, &(ev->status))) { return NULL; }
  if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_INT32, &(ev->task))) { return NULL; }
  uint16_t f = ev->flag; // make sure to not get any "static" in this one...
  if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_UINT16, &(f))) { return NULL; }
  if (ev->string && !dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &(ev->string))) { return NULL; }
  if (ev->stringset) {
   DBusMessageIter sub;
   uint32_t i = 0;
   if (!dbus_message_iter_open_container (&argv, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &sub)) return NULL;

   for (; ev->stringset[i]; i++) {
    if (!dbus_message_iter_append_basic (&sub, DBUS_TYPE_STRING, &(ev->stringset[i]))) { return NULL; }
   }
   if (!dbus_message_iter_close_container (&argv, &sub)) return NULL;
  }
 } else {
  if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_UINT32, &(ev->ipc_options))) { return NULL; }
  if (ev->command && !dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &(ev->command))) { return NULL; }
  if (ev->argv) {
   DBusMessageIter sub;
   uint32_t i = 0;
   if (!dbus_message_iter_open_container (&argv, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &sub)) return NULL;

   for (; ev->argv[i]; i++) {
    if (!dbus_message_iter_append_basic (&sub, DBUS_TYPE_STRING, &(ev->argv[i]))) { return NULL; }
   }
   if (!dbus_message_iter_close_container (&argv, &sub)) return NULL;
  }
 }

 return message;
}

void *einit_dbus_handle_event (DBusMessage *message) {
 DBusMessage *reply;

 if (!message) return NULL;

 struct einit_event *ev = einit_dbus_read_event (message);

 if (!ev) return NULL;

 event_emit (ev, einit_event_flag_broadcast);

 reply = dbus_message_new_method_return(message);
 if (!reply) { return NULL; }

 reply = einit_dbus_create_event_message (reply, ev);
 if (!reply) { evpurge (ev); return NULL; }

 evpurge (ev);

 emutex_lock (&einit_dbus_sequence_mutex);
 einit_dbus_sequence++;
 if (!dbus_connection_send(einit_dbus_connection, reply, &einit_dbus_sequence)) { emutex_unlock (&einit_dbus_sequence_mutex); return NULL; }
 emutex_unlock (&einit_dbus_sequence_mutex);

 dbus_message_unref(message);
 dbus_message_unref(reply);

 return NULL;
}

int einit_ipc_dbus_enable (void *pa, struct einit_event *status) {
 return einit_dbus_enable(status);
}

int einit_ipc_dbus_disable (void *pa, struct einit_event *status) {
 return einit_dbus_disable(status);
}

int einit_dbus_cleanup (struct lmodule *lm) {
 event_ignore (einit_event_subsystem_any, einit_dbus_generic_event_handler);
 dbus_error_free(&(einit_dbus_error));

 return 0;
}
