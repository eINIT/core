/*
 *  ipc-dbus.c++
 *  einit
 *
 *  Created by Magnus Deininger on 05/09/2006.
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

#include <einit-modules/dbus.h>
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

class einit_dbus einit_main_dbus_class;

extern "C" {

int einit_dbus_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * einit_dbus_provides[] = {"ipc-dbus", NULL};
char * einit_dbus_requires[] = {"dbus", NULL};
const struct smodule einit_dbus_self = {
 EINIT_VERSION,
 BUILDNUMBER,
 1,
 einit_module_generic,
 "eINIT <-> DBUS connector",
 "einit-ipc-dbus",
 {
  einit_dbus_provides,
  einit_dbus_requires,
  NULL,
  NULL
 },
 einit_dbus_configure
};

module_register(einit_dbus_self);

#endif

pthread_attr_t einit_ipc_dbus_thread_attribute_detached;

int einit_dbus_configure (struct lmodule *irr) {
 int pthread_errno;
 module_init (irr);

 pthread_attr_init (&einit_ipc_dbus_thread_attribute_detached);
 if ((pthread_errno = pthread_attr_setdetachstate (&einit_ipc_dbus_thread_attribute_detached, PTHREAD_CREATE_DETACHED))) {
  bitch(bitch_epthreads, pthread_errno, "pthread_attr_setdetachstate() failed.");
 }

 thismodule->enable = einit_ipc_dbus_enable;
 thismodule->disable = einit_ipc_dbus_disable;
 thismodule->cleanup = einit_dbus_cleanup;

 return einit_main_dbus_class.configure();;
}

}

int einit_dbus::configure() {
 event_listen (einit_event_subsystem_any, this->generic_event_handler);

 return 0;
}

void einit_dbus::signal_dbus (const char *IN_string) {
 DBusMessage *message;
 DBusMessageIter argv;

 message = dbus_message_new_signal("/org/einit/einit", "org.einit.Einit.Information", "EventString");
 if (!message) { return; }

 dbus_message_iter_init_append(message, &argv);
 if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &IN_string)) { return; }

 emutex_lock (&this->sequence_mutex);
 this->sequence++;
/* if (!dbus_connection_send(this->connection, message, &this->sequence)) { emutex_unlock (&this->sequence_mutex); return; }*/
 dbus_connection_send(this->connection, message, &this->sequence);
 emutex_unlock (&this->sequence_mutex);
// dbus_connection_flush(this->connection);
 dbus_message_unref(message);
}

void einit_dbus::broadcast_event (struct einit_event *ev) {
 DBusMessage *message;

 message = dbus_message_new_signal("/org/einit/einit", "org.einit.Einit.Information", "EventSignal");
 if (!message) { return; }

 message = this->create_event_message (message, ev);
 if (!message) { return; }

 emutex_lock (&this->sequence_mutex);
 this->sequence++;
 dbus_connection_send(this->connection, message, &this->sequence);
 emutex_unlock (&this->sequence_mutex);

 dbus_message_unref(message);
}

void einit_dbus::generic_event_handler (struct einit_event *ev) {
 if (einit_main_dbus_class.active) {
  uint32_t subsystem = ev->type & EVENT_SUBSYSTEM_MASK;

  if (subsystem != einit_event_subsystem_timer) // don't broadcast timer events.
   einit_main_dbus_class.broadcast_event (ev);
 }

 return;
}

einit_dbus::einit_dbus() {
 dbus_error_init(&(this->error));
 this->sequence = 1;
 this->active = 0;

 pthread_mutex_init (&(this->sequence_mutex), NULL);

 this->introspection_data = einit_dbus_introspection_data;
// dbus_g_object_type_install_info (COM_FOO_TYPE_MY_OBJECT, &(this->introspection_data));
}

einit_dbus::~einit_dbus() {
 dbus_error_free(&(this->error));
}

int einit_dbus::dbus_connect () {
 char *dbusname;
 char *dbusaddress;
 int ret = 0;

 if (!(dbusaddress = cfg_getstring("configuration-ipc-dbus-connection/address", NULL))) dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 if (!(dbusname = cfg_getstring("configuration-ipc-dbus-connection/name", NULL))) dbusname = "org.einit.Einit";

 if (dbus_error_is_set(&(this->error))) { 
  notice (2, "DBUS: Error (%s)\n", this->error.message); 
  dbus_error_free(&(this->error));
 }

// this->connection = dbus_bus_get(DBUS_BUS_SESSION, &(this->error));
 this->connection = dbus_connection_open_private (dbusaddress, &(this->error));
 if (dbus_error_is_set(&(this->error))) { 
  notice (2, "DBUS: Connection Error (%s)\n", this->error.message); 
  dbus_error_free(&(this->error));
 }
 if (!this->connection) return status_failed;

 if (dbus_bus_register (this->connection, &(this->error)) != TRUE) {
  if (dbus_error_is_set(&(this->error))) { 
   notice(2, "DBUS: Registration Error (%s)\n", this->error.message); 
   dbus_error_free(&(this->error));
  }

  return status_failed;
 }

 ret = dbus_bus_request_name(this->connection, dbusname, DBUS_NAME_FLAG_REPLACE_EXISTING, &(this->error));
 if (dbus_error_is_set(&(this->error))) { 
  notice(2, "DBUS: Name Error (%s)\n", this->error.message); 
  dbus_error_free(&(this->error));
 }
 if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) return status_failed;

 return status_ok;
}

int einit_dbus::enable (struct einit_event *status) {
 char *dbusname;
 char *dbusaddress;
 int ret = 0;

 if (!(dbusaddress = cfg_getstring("configuration-ipc-dbus-connection/address", NULL))) dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 if (!(dbusname = cfg_getstring("configuration-ipc-dbus-connection/name", NULL))) dbusname = "org.einit.Einit";

// this->connection = dbus_bus_get(DBUS_BUS_SESSION, &(this->error));
 this->connection = dbus_connection_open_private (dbusaddress, &(this->error));
 if (dbus_error_is_set(&(this->error))) { 
  fbprintf(status, "DBUS: Connection Error (%s)\n", this->error.message); 
  dbus_error_free(&(this->error));
 }
 if (!this->connection) return status_failed;

 if (dbus_bus_register (this->connection, &(this->error)) != TRUE) {
  if (dbus_error_is_set(&(this->error))) { 
   fbprintf(status, "DBUS: Registration Error (%s)\n", this->error.message); 
   dbus_error_free(&(this->error));
  }

  return status_failed;
 }

 ret = dbus_bus_request_name(this->connection, dbusname, DBUS_NAME_FLAG_REPLACE_EXISTING, &(this->error));
 if (dbus_error_is_set(&(this->error))) { 
  fbprintf(status, "DBUS: Name Error (%s)\n", this->error.message); 
  dbus_error_free(&(this->error));
 }
 if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) return status_failed;

 this->terminate_thread = 0;
 this->active = 1;
 notice (2, "message thread creation initiated");

 if ((errno = pthread_create (&(this->message_thread_id), /*&einit_ipc_dbus_thread_attribute_detached*/ NULL, &(einit_dbus::message_thread_bootstrap), NULL))) {
  fbprintf (status, "could not create detached I/O thread, creating non-detached thread. (error = %s)", strerror(errno));

  ethread_create (&(this->message_thread_id), NULL, &(einit_dbus::message_thread_bootstrap), NULL);
 }

 return status_ok;
}

int einit_dbus::disable (struct einit_event *status) {

/* dbus_connection_flush(this->connection);*/
 this->terminate_thread = 1;
 this->active = 0;

 pthread_join (this->message_thread_id, NULL);

 return status_ok;
}

void *einit_dbus::message_thread_bootstrap(void *e) {
 notice (2, "message thread creation spawning");
 einit_main_dbus_class.message_thread();

 return NULL;
}

void einit_dbus::message_thread() {
 DBusMessage *message;

 while (1) {

 while (this->connection && dbus_connection_read_write_dispatch(this->connection, 500)) {
  if (!dbus_connection_get_is_connected (this->connection)) break;
  message = dbus_connection_pop_message(this->connection);

  if (message) {
// introspection support
   if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Introspectable", "Introspect")) {
    DBusMessage *reply;
    DBusMessageIter args;

    reply = dbus_message_new_method_return(message);

    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &this->introspection_data)) { continue; }

    emutex_lock (&this->sequence_mutex);
    this->sequence++;
    if (!dbus_connection_send(this->connection, reply, &(this->sequence))) {
	 emutex_unlock (&this->sequence_mutex);
     continue; }
    emutex_unlock (&this->sequence_mutex);

    dbus_message_unref(reply);
// 'old fashioned' ipc via dbus
   } else if (dbus_message_is_method_call(message, "org.einit.Einit.Information", "IPC")) {
    pthread_t th;
    if (pthread_create (&th, &einit_ipc_dbus_thread_attribute_detached, (void *(*)(void *))&(einit_dbus::ipc_spawn_safe_bootstrap), message)) {
     this->ipc_spawn_safe_bootstrap(message);
    }
   } else if (dbus_message_is_method_call(message, "org.einit.Einit.Command", "IPC")) {
    pthread_t th;
    if (pthread_create (&th, &einit_ipc_dbus_thread_attribute_detached, (void *(*)(void *))&(einit_dbus::ipc_spawn_bootstrap), message)) {
     this->ipc_spawn_bootstrap(message);
    }
   } else if (dbus_message_is_method_call(message, "org.einit.Einit.Command", "EmitEvent")) {
    pthread_t th;
    if (pthread_create (&th, &einit_ipc_dbus_thread_attribute_detached, (void *(*)(void *))&(einit_dbus::handle_event_bootstrap), message)) {
     this->handle_event(message);
    }
   }
  }

  if (this->terminate_thread) {
   this->active = 0;

   int r = 1;
   while ((r = sleep(r))) ; // wait a second before disconnecting

   dbus_connection_close(this->connection); // close the connection after looping

   this->connection = NULL;
   return;
  }
 }

 this->active = 0;

 notice (1, "dbus connection was dropped, suspending signals until we're reconnected!\n");
 fprintf (stderr, "dbus connection was dropped, suspending signals until we're reconnected!\n");
 fflush (stderr);

 int r = 5;
 while ((r = sleep(r))) ; // wait a couple of seconds before reconnecting to have everyone calm down a little

// this->connection = NULL;

 this->dbus_connect();

 if (this->connection) this->active = 1;
 }
}

char *einit_dbus::ipc_request (char *command) {
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

  if (!returnvalue) returnvalue = estrdup("meow!\n");
  
  return returnvalue;
}

void *einit_dbus::ipc_spawn_bootstrap (DBusMessage *message) {
 einit_main_dbus_class.ipc_spawn (message);

 dbus_message_unref(message);
 return NULL;
}

void *einit_dbus::ipc_spawn_safe_bootstrap (DBusMessage *message) {
 einit_main_dbus_class.ipc_spawn_safe (message);

 dbus_message_unref(message);
 return NULL;
}

void einit_dbus::ipc_spawn(DBusMessage *message) {
 DBusMessage *reply;
 DBusMessageIter args;
 char *command = "";

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n");
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) 
  fprintf(stderr, "Argument is not string!\n"); 
 else {
  char *returnvalue = NULL;
  dbus_message_iter_get_basic(&args, &command);

  reply = dbus_message_new_method_return(message);

  returnvalue = this->ipc_request (command);

  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { free (returnvalue); return; }

  free (returnvalue);

  emutex_lock (&this->sequence_mutex);
  this->sequence++;
  if (!dbus_connection_send(this->connection, reply, &(this->sequence))) { emutex_unlock (&this->sequence_mutex); return; }
  emutex_unlock (&this->sequence_mutex);

  dbus_message_unref(reply);
 }
}

void einit_dbus::ipc_spawn_safe(DBusMessage *message) {
 DBusMessage *reply;
 DBusMessageIter args;
 char *command = "";

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n");
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) 
  fprintf(stderr, "Argument is not string!\n"); 
 else {
  char *returnvalue = NULL;
  dbus_message_iter_get_basic(&args, &command);

  if (!strmatch (command, "list modules --xml") && !strmatch (command, "list services --xml")) {
   reply = dbus_message_new_method_return(message);

   if (!returnvalue) returnvalue = "<einit-ipc><error type=\"unsafe-request\" /></einit-ipc>\n";

   dbus_message_iter_init_append(reply, &args);
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { return; }

   emutex_lock (&this->sequence_mutex);
   this->sequence++;
   if (!dbus_connection_send(this->connection, reply, &(this->sequence))) { emutex_unlock (&this->sequence_mutex); return; }
   emutex_unlock (&this->sequence_mutex);

   dbus_message_unref(reply);

   return;
  }

  reply = dbus_message_new_method_return(message);

  returnvalue = this->ipc_request (command);

  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { free (returnvalue); return; }

  free (returnvalue);

  emutex_lock (&this->sequence_mutex);
  this->sequence++;
  if (!dbus_connection_send(this->connection, reply, &(this->sequence))) { emutex_unlock (&this->sequence_mutex); return; }
  emutex_unlock (&this->sequence_mutex);

  dbus_message_unref(reply);
 }
}

struct einit_event *einit_dbus::read_event (DBusMessage *message) {
 struct einit_event *ev = evinit (0);
 DBusMessageIter args;
 DBusMessageIter sub;

 emutex_init (&(ev->mutex), NULL);

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

DBusMessage *einit_dbus::create_event_message (DBusMessage *message, struct einit_event *ev) {
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

void einit_dbus::handle_event (DBusMessage *message) {
 DBusMessage *reply;

 if (!message) return;

 struct einit_event *ev = this->read_event (message);

 if (!ev) return;

 event_emit (ev, einit_event_flag_broadcast);

 reply = dbus_message_new_method_return(message);
 if (!reply) { return; }

 reply = this->create_event_message (reply, ev);
 if (!reply) { evpurge (ev); return; }

 evpurge (ev);

 emutex_lock (&this->sequence_mutex);
 this->sequence++;
 if (!dbus_connection_send(this->connection, reply, &this->sequence)) { emutex_unlock (&this->sequence_mutex); return; }
 emutex_unlock (&this->sequence_mutex);

 dbus_message_unref(message);
 dbus_message_unref(reply);
}

void *einit_dbus::handle_event_bootstrap (DBusMessage *message) {
 einit_main_dbus_class.handle_event (message);

 return NULL;
}

int einit_ipc_dbus_enable (void *pa, struct einit_event *status) {
 return einit_main_dbus_class.enable(status);
}

int einit_ipc_dbus_disable (void *pa, struct einit_event *status) {
 return einit_main_dbus_class.disable(status);
}

int einit_dbus_cleanup (struct lmodule *) {
 event_ignore (einit_event_subsystem_any, einit_main_dbus_class.generic_event_handler);

 return 0;
}
