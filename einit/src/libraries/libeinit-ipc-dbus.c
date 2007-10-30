/*
 *  libeinit-ipc-dbus.c
 *  einit
 *
 *  Created by Magnus Deininger on 07/10/2007.
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

#include <dbus/dbus.h>
#include <einit/einit.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <expat.h>

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

struct remote_event_function {
 uint32_t type;                                 /*!< type of function */
 void (*handler)(struct einit_remote_event *);  /*!< handler function */
 struct remote_event_function *next;            /*!< next function */
};

extern pthread_mutex_t einit_evf_mutex;
extern struct remote_event_function *event_remote_event_functions;
extern uint32_t remote_event_cseqid;

#ifdef DARWIN
pthread_attr_t einit_dbus_thread_attribute_detached = {};
#else
pthread_attr_t einit_dbus_thread_attribute_detached;
#endif

DBusError *einit_dbus_error = NULL;
DBusError *einit_dbus_error_events = NULL;
DBusConnection *einit_dbus_connection = NULL;
DBusConnection *einit_dbus_connection_events = NULL;

char einit_dbus_disconnect = 0;
void *einit_event_emit_remote_dispatch (struct einit_remote_event *ev) {
 uint32_t subsystem;

 if (!ev || !ev->type) return NULL;
 subsystem = ev->type & EVENT_SUBSYSTEM_MASK;

 /* initialise sequence id and timestamp of the event */
 ev->seqid = remote_event_cseqid++;
 ev->timestamp = time(NULL);

 struct remote_event_function *cur = event_remote_event_functions;
 while (cur) {
  if (((cur->type == subsystem) || (cur->type == einit_event_subsystem_any)) && cur->handler) {
   cur->handler (ev);
  }
  cur = cur->next;
 }

 if (subsystem == einit_event_subsystem_ipc) {
  if (ev->argv) free (ev->argv);
  if (ev->command) free (ev->command);
 } else {
  if (ev->string) free (ev->string);
  if (ev->stringset) free (ev->stringset);
 }

 free (ev);

 return NULL;
}

void einit_event_emit_remote (struct einit_remote_event *ev, enum einit_event_emit_flags flags) {
 pthread_t threadid;

 if (ethread_create (&threadid, &einit_dbus_thread_attribute_detached, (void *(*)(void *))einit_event_emit_remote_dispatch, ev))
  einit_event_emit_remote_dispatch (ev);
}

struct einit_remote_event *einit_read_remote_event_off_dbus (DBusMessage *message) {
 struct einit_remote_event *ev = emalloc (sizeof (struct einit_remote_event));
 DBusMessageIter args;
 DBusMessageIter sub;

 memset (ev, 0, sizeof (struct einit_remote_event));

  if (!dbus_message_iter_init(message, &args)) { fprintf(stderr, "Message has no arguments!\n"); return NULL; }
  else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32) { fprintf(stderr, "Argument has incorrect type (event type).\n"); return NULL; }
  else dbus_message_iter_get_basic(&args, &(ev->type));

  if ((ev->type & EVENT_SUBSYSTEM_MASK) != einit_event_subsystem_ipc) {
   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)) { fprintf(stderr, "Argument has incorrect type (integer).\n"); return NULL; }
   else dbus_message_iter_get_basic(&args, &(ev->integer));

   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)) { fprintf(stderr, "Argument has incorrect type (status).\n"); return NULL; }
   else dbus_message_iter_get_basic(&args, &(ev->status));

   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT32)) { fprintf(stderr, "Argument has incorrect type (task).\n"); return NULL; }
   else dbus_message_iter_get_basic(&args, &(ev->task));

   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT16)) { fprintf(stderr, "Argument has incorrect type (flag).\n"); return NULL; }
   else { uint16_t r; dbus_message_iter_get_basic(&args, &(r)); ev->flag = r; }

   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }

   if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
    dbus_message_iter_get_basic(&args, &(ev->string));
    ev->string = estrdup (ev->string);

    if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }
   }
   if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY) { fprintf(stderr, "Argument has incorrect type (string set).\n"); return NULL; }

   dbus_message_iter_recurse (&args, &sub);
   while (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_INVALID) {
    char *value;

    if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING) { fprintf(stderr, "Argument has incorrect type (string set member).\n"); return NULL; }
    dbus_message_iter_get_basic(&sub, &(value));

    ev->stringset = (char **)setadd ((void **)ev->stringset, (void *)value, SET_TYPE_STRING);

    dbus_message_iter_next (&sub);
   }
  } else {
   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32)) { fprintf(stderr, "Argument has incorrect type (ipc-options).\n"); return NULL; }
   else dbus_message_iter_get_basic(&args, &(ev->integer));

   if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }

   if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
    dbus_message_iter_get_basic(&args, &(ev->command));
    ev->command = estrdup (ev->command);

    if ((!dbus_message_iter_next(&args)) || (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INVALID)) { goto message_read; }
   }
 
   if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY) { fprintf(stderr, "Argument has incorrect type (ipc argv).\n"); return NULL; }
   dbus_message_iter_recurse (&args, &sub);
   while (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_INVALID) {
    char *value;

    if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING) { fprintf(stderr, "Argument has incorrect type (ipc argv member).\n"); return NULL; }
    dbus_message_iter_get_basic(&sub, &(value));

    ev->argv = (char **)setadd ((void **)ev->argv, (void *)value, SET_TYPE_STRING);

    dbus_message_iter_next (&sub);
   }

   ev->argc = setcount ((const void **)ev->argv);
  }

 message_read:

 return ev;
}

DBusHandlerResult einit_incoming_event_handler(DBusConnection *connection, DBusMessage *message, void *user_data) {
 if (dbus_message_is_signal(message, "org.einit.Einit.Information", "EventSignal")) {
  struct einit_remote_event *ev = einit_read_remote_event_off_dbus (message);

  if (!ev) return DBUS_HANDLER_RESULT_HANDLED;

  einit_event_emit_remote (ev, einit_event_flag_broadcast);

  return DBUS_HANDLER_RESULT_HANDLED;
 }

 return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void einit_message_thread_reconnect () {
 if (!(einit_dbus_connection_events = dbus_bus_get(DBUS_BUS_SYSTEM, einit_dbus_error_events))) {
  if (dbus_error_is_set(einit_dbus_error_events)) {
   fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error_events->message);
   dbus_error_free(einit_dbus_error_events);
  }
  return;
 }
 dbus_connection_set_exit_on_disconnect(einit_dbus_connection_events, FALSE);
 dbus_connection_ref(einit_dbus_connection_events);

 dbus_bus_add_match(einit_dbus_connection_events, "type='signal',interface='org.einit.Einit.Information'", einit_dbus_error_events);
 if (dbus_error_is_set(einit_dbus_error_events)) {
  fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error_events->message);
  dbus_error_free(einit_dbus_error_events);
 }

 dbus_connection_add_filter (einit_dbus_connection_events, einit_incoming_event_handler, NULL, NULL);
}

void *einit_message_thread(void *notused) {
 while (1) {
  int s = 5;

  while (dbus_connection_read_write_dispatch(einit_dbus_connection_events, -1)) {
   if (einit_dbus_disconnect) {
    einit_dbus_connection_events = NULL;
    return NULL;
   }
  }

  while ((s = sleep (s))); // wait 5 seconds to cool down

  einit_message_thread_reconnect();
 }

 fprintf (stderr, "lost connection...\n");

 einit_dbus_connection_events = NULL;
 return NULL;
}

char einit_connect(int *argc, char **argv) {
// char *dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 pthread_attr_init (&einit_dbus_thread_attribute_detached);
 pthread_attr_setdetachstate (&einit_dbus_thread_attribute_detached, PTHREAD_CREATE_DETACHED);

 dbus_threads_init_default();

 einit_dbus_error = ecalloc (1, sizeof (DBusError));
 dbus_error_init(einit_dbus_error);

 if (!(einit_dbus_connection = dbus_bus_get_private(DBUS_BUS_SYSTEM, einit_dbus_error))) {
  if (dbus_error_is_set(einit_dbus_error)) {
   fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error->message);
   dbus_error_free(einit_dbus_error);
  }
  return 0;
 }

/* einit_dbus_connection = dbus_connection_open_private (dbusaddress, einit_dbus_error);
 if (dbus_error_is_set(einit_dbus_error)) {
 fprintf(stderr, "DBUS: Connection Error (%s)\n", einit_dbus_error->message);
 dbus_error_free(einit_dbus_error);
}
 if (!einit_dbus_connection) return 0;

 if (dbus_bus_register (einit_dbus_connection, einit_dbus_error) != TRUE) {
 if (dbus_error_is_set(einit_dbus_error)) { 
 fprintf(stderr, "DBUS: Registration Error (%s)\n", einit_dbus_error->message); 
 dbus_error_free(einit_dbus_error);
}
 return 0;
}*/

 dbus_connection_set_exit_on_disconnect(einit_dbus_connection, FALSE);
 dbus_connection_ref(einit_dbus_connection);

 return 1;
}

char einit_disconnect() {
// ethread_join (&einit_message_thread_id);
 einit_dbus_disconnect = 1;

 dbus_connection_unref(einit_dbus_connection);

 dbus_connection_close (einit_dbus_connection);

 return 1;
}

void einit_receive_events() {
 pthread_t einit_message_thread_id;
 dbus_threads_init_default();

 einit_dbus_error_events = ecalloc (1, sizeof (DBusError));
 dbus_error_init(einit_dbus_error_events);

 if (!einit_dbus_connection_events) {
  if (!(einit_dbus_connection_events = dbus_bus_get(DBUS_BUS_SYSTEM, einit_dbus_error_events))) {
   if (dbus_error_is_set(einit_dbus_error_events)) {
    fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error_events->message);
    dbus_error_free(einit_dbus_error_events);
   }
   return;
  }
  dbus_connection_set_exit_on_disconnect(einit_dbus_connection_events, FALSE);
  dbus_connection_ref(einit_dbus_connection_events);

  dbus_bus_add_match(einit_dbus_connection_events, "type='signal',interface='org.einit.Einit.Information'", einit_dbus_error_events);
  if (dbus_error_is_set(einit_dbus_error_events)) {
   fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error_events->message);
   dbus_error_free(einit_dbus_error_events);
  }

  dbus_connection_add_filter (einit_dbus_connection_events, einit_incoming_event_handler, NULL, NULL);
  ethread_create (&einit_message_thread_id, &einit_dbus_thread_attribute_detached, einit_message_thread, NULL);
 } else {
  dbus_connection_ref(einit_dbus_connection_events);
  dbus_bus_add_match(einit_dbus_connection_events, "type='signal',interface='org.einit.Einit.Information'", einit_dbus_error_events);
  if (dbus_error_is_set(einit_dbus_error_events)) {
   fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error_events->message);
   dbus_error_free(einit_dbus_error_events);
  }
 }
}

char *einit_ipc_i (const char *command, const char *interface) {
 dbus_connection_ref(einit_dbus_connection);

 char *returnvalue;

 DBusMessage *message, *call;
 DBusMessageIter args;

 if (!(call = dbus_message_new_method_call("org.einit.Einit", "/org/einit/einit", interface, "IPC"))) {
  dbus_connection_unref(einit_dbus_connection);
  fprintf(stderr, "Sending message failed.\n");
  return NULL;
 }

 dbus_message_iter_init_append(call, &args);
 if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &command)) { 
  dbus_connection_unref(einit_dbus_connection);
  fprintf(stderr, "Out Of Memory!\n"); 
  return NULL;
 }

 if (dbus_error_is_set(einit_dbus_error)) {
  fprintf(stderr, "had an error before... (%s)\n", einit_dbus_error->message);
  dbus_error_free(einit_dbus_error);
 }

 if (!(message = dbus_connection_send_with_reply_and_block (einit_dbus_connection, call, 5000, einit_dbus_error))) {
  dbus_connection_unref(einit_dbus_connection);
  if (dbus_error_is_set(einit_dbus_error)) {
   fprintf(stderr, "DBus Error (%s)\n", einit_dbus_error->message);
   dbus_error_free(einit_dbus_error);
  }
  return NULL;
 }

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n"); 
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
  fprintf(stderr, "Argument is not a string...?\n"); 
 else
  dbus_message_iter_get_basic(&args, &returnvalue);

 if (returnvalue) returnvalue = estrdup (returnvalue);

 dbus_message_unref(message);
 dbus_connection_unref(einit_dbus_connection);

 return returnvalue;
}

char *einit_ipc(const char *command) {
 return einit_ipc_i (command, "org.einit.Einit.Command");
}

char *einit_ipc_safe(const char *command) {
 return einit_ipc_i (command, "org.einit.Einit.Information");
}

char *einit_ipc_request(const char *command) {
 if (einit_dbus_connection || einit_connect(NULL, NULL)) {
  return einit_ipc(command);
 }

 return NULL;
}

DBusMessage *einit_create_event_message (DBusMessage *message, struct einit_remote_event *ev) {
 DBusMessageIter argv;

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

void einit_remote_event_emit_dispatch (struct einit_remote_event *ev) {
 dbus_connection_ref(einit_dbus_connection);

 DBusMessage *message, *call;

 if (!(call = dbus_message_new_method_call("org.einit.Einit", "/org/einit/einit", "org.einit.Einit.Command", "EmitEvent"))) {
  dbus_connection_unref(einit_dbus_connection);
  fprintf(stderr, "Sending message failed.\n");
  return;
 }

 if (!(call = einit_create_event_message(call, ev))) { 
  dbus_connection_unref(einit_dbus_connection);
  fprintf(stderr, "Out Of Memory!\n"); 
  return;
 }

 if (!(message = dbus_connection_send_with_reply_and_block (einit_dbus_connection, call, 5000, einit_dbus_error))) {
  dbus_connection_unref(einit_dbus_connection);
  if (dbus_error_is_set(einit_dbus_error)) {
   fprintf(stderr, "DBus Error (%s)\n", einit_dbus_error->message);
   dbus_error_free(einit_dbus_error);
  }
  return;
 }

 uint32_t subsystem = ev->type & EVENT_SUBSYSTEM_MASK;

 if (subsystem == einit_event_subsystem_ipc) {
  if (ev->argv) free (ev->argv);
  if (ev->command) free (ev->command);
 } else {
  if (ev->string) free (ev->string);
  if (ev->stringset) free (ev->stringset);
 }

 free (ev);

 dbus_message_unref(call);
 dbus_message_unref(message);
 dbus_connection_unref(einit_dbus_connection);

 return;
}

void einit_remote_event_emit (struct einit_remote_event *ev, enum einit_event_emit_flags flags) {
 pthread_t threadid;

 if (flags & einit_event_flag_spawn_thread) {
  struct einit_remote_event *rev = emalloc (sizeof (struct einit_remote_event));
  memcpy (rev, ev, sizeof (struct einit_remote_event));
  uint32_t subsystem = ev->type & EVENT_SUBSYSTEM_MASK;

  if (subsystem == einit_event_subsystem_ipc) {
   if (ev->command) rev->string = estrdup (ev->command);
   if (ev->argv) rev->argv = (char **)setdup ((const void **)ev->argv, SET_TYPE_STRING);
  } else {
   if (ev->string) rev->string = estrdup (ev->string);
   if (ev->stringset) rev->stringset = (char **)setdup ((const void **)ev->stringset, SET_TYPE_STRING);
  }

  if (ethread_create (&threadid, &einit_dbus_thread_attribute_detached, (void *(*)(void *))einit_remote_event_emit_dispatch, rev))
   einit_remote_event_emit_dispatch (rev);
 } else {
  dbus_connection_ref(einit_dbus_connection);
  struct einit_remote_event *rv;

  DBusMessage *message, *call;

  if (!(call = dbus_message_new_method_call("org.einit.Einit", "/org/einit/einit", "org.einit.Einit.Command", "EmitEvent"))) {
   dbus_connection_unref(einit_dbus_connection);
   fprintf(stderr, "Sending message failed.\n");
   return;
  }

  if (!(call = einit_create_event_message(call, ev))) { 
   dbus_connection_unref(einit_dbus_connection);
   fprintf(stderr, "Out Of Memory!\n"); 
   return;
  }

  if (!(message = dbus_connection_send_with_reply_and_block (einit_dbus_connection, call, 5000, einit_dbus_error))) {
   dbus_connection_unref(einit_dbus_connection);
   if (dbus_error_is_set(einit_dbus_error)) {
    fprintf(stderr, "DBus Error (%s)\n", einit_dbus_error->message);
    dbus_error_free(einit_dbus_error);
   }
   return;
  }

  if (!(rv = einit_read_remote_event_off_dbus (message))) {
   dbus_connection_unref(einit_dbus_connection);
   fprintf (stderr, "Error Parsing Message Reply.");
   return;
  }

  memcpy (ev, rv, sizeof (struct einit_remote_event));
  free (rv);

  dbus_message_unref(call);
  dbus_message_unref(message);
  dbus_connection_unref(einit_dbus_connection);

  return;
 }
}
