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
 "ipc-dbus",
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

int einit_dbus_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->enable = einit_ipc_dbus_enable;
 thismodule->disable = einit_ipc_dbus_disable;
 thismodule->cleanup = einit_dbus_cleanup;

 return einit_main_dbus_class.configure();;
}

}

int einit_dbus::configure() {
 return 0;
}

void einit_dbus::signal_dbus (const char *IN_string) {
 DBusMessage *message;
 DBusMessageIter argv;

 message = dbus_message_new_signal("/org/einit/einit", "org.einit.Einit.Information", "SignalIPC");
 if (!message) { return; }

 dbus_message_iter_init_append(message, &argv);
 if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &IN_string)) { return; }

 this->sequence++;
 if (!dbus_connection_send(this->connection, message, &this->sequence)) { return; }
// dbus_connection_flush(this->connection);

 dbus_message_unref(message);
}

void einit_dbus::ipc_event_handler (struct einit_event *ev) {
 einit_main_dbus_class.signal_dbus (ev->string);

 if (ev->argc >= 2) {
  if (strmatch(ev->argv[0], "test") && strmatch(ev->argv[1], "ipc")) {
   fprintf (ev->output, "meow!!\n");
   ev->implemented = 1;
  }
 }

 return;
}

void einit_dbus::string(const char *IN_string, char ** OUT_result) {
 *OUT_result = estrdup ("hello world!");

 return;
}

einit_dbus::einit_dbus() {
 dbus_error_init(&(this->error));
 this->sequence = 0;

// this->introspection_data = einit_dbus_introspection_data;
// dbus_g_object_type_install_info (COM_FOO_TYPE_MY_OBJECT, &(this->introspection_data));
}

einit_dbus::~einit_dbus() {
 dbus_error_free(&(this->error));
}

int einit_dbus::enable (struct einit_event *status) {
 char *dbusname;
 char *dbusaddress;
 int ret = 0;

 if (!(dbusaddress = cfg_getstring("configuraion-ipc-dbus-connection/address", NULL))) dbusaddress = "unix:path=/var/run/dbus/system_bus_socket";
 if (!(dbusname = cfg_getstring("configuraion-ipc-dbus-connection/name", NULL))) dbusname = "org.einit.Einit";

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

 event_listen (einit_event_subsystem_ipc, this->ipc_event_handler);

 this->terminate_thread = 0;
 ethread_create (&(this->message_thread_id), &thread_attribute_detached, this->message_thread_bootstrap, NULL);

 return status_ok;
}

int einit_dbus::disable (struct einit_event *status) {
 event_ignore (einit_event_subsystem_ipc, this->ipc_event_handler);

/* dbus_connection_flush(this->connection);*/
 this->terminate_thread = 1;

 dbus_connection_close(this->connection); // close the connection after looping

 return status_ok;
}

void *einit_dbus::message_thread_bootstrap(void *e) {
 einit_main_dbus_class.message_thread();

 return NULL;
}

void einit_dbus::message_thread() {
 DBusMessage *message;

 while (dbus_connection_read_write_dispatch(this->connection, 100)) {
  message = dbus_connection_pop_message(this->connection);

  if (message) {
// introspection support
   if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Introspectable", "Introspect")) {
    DBusMessage *reply;
    DBusMessageIter args;

    reply = dbus_message_new_method_return(message);

    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &this->introspection_data)) { continue; }
    this->sequence++;
    if (!dbus_connection_send(this->connection, reply, &(this->sequence))) { continue; }

    dbus_message_unref(reply);
// 'old fashioned' ipc via dbus
   } else if (dbus_message_is_method_call(message, "org.einit.Einit.Information", "IPC"))
    this->ipc(message, 1);
   else if (dbus_message_is_method_call(message, "org.einit.Einit.Command", "IPC"))
    this->ipc(message, 0);

   dbus_message_unref(message);
  }
 }
}

void einit_dbus::ipc(DBusMessage *message, char safe) {
 DBusMessage *reply;
 DBusMessageIter args;
 bool stat = true;
 char *command = "";

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n");
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) 
  fprintf(stderr, "Argument is not string!\n"); 
 else {
  int internalpipe[2];
  int rv;
  char *returnvalue = NULL;
  dbus_message_iter_get_basic(&args, &command);

  if (safe) { // check for safe requests, answer with "unsafe request" if necessary
   if (!strmatch (command, "list modules --xml")) {
    reply = dbus_message_new_method_return(message);

    if (!returnvalue) returnvalue = "<einit-ipc><error type=\"unsafe-request\" /></einit-ipc>\n";

    this->sequence++;
    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { return; }
    if (!dbus_connection_send(this->connection, reply, &(this->sequence))) { return; }

    dbus_message_unref(reply);

    return;
   }
  }

  if ((rv = pipe (internalpipe)) > 0) {
   ipc_process(command, stderr);
  } else if (rv == 0) {
   char *buf = NULL;
   uint32_t blen = 0;
   int rn = 0;
   char *data = NULL;
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
     bitch(bitch_stdio, errno, "reading file failed.");

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

  reply = dbus_message_new_method_return(message);

  if (!returnvalue) returnvalue = "meow!\n";

  this->sequence++;
  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &returnvalue)) { return; }
  if (!dbus_connection_send(this->connection, reply, &(this->sequence))) { return; }

  dbus_message_unref(reply);
 }
}

int einit_ipc_dbus_enable (void *pa, struct einit_event *status) {
 return einit_main_dbus_class.enable(status);
}

int einit_ipc_dbus_disable (void *pa, struct einit_event *status) {
 return einit_main_dbus_class.disable(status);
}

int einit_dbus_cleanup (struct lmodule *) {
 return 0;
}
