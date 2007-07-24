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

 message = dbus_message_new_signal("/org/einit/einit", "org.einit.Einit", "IPC");
 if (!message) { return; }

 dbus_message_iter_init_append(message, &argv);
 if (!dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &IN_string)) { return; }

 if (!dbus_connection_send(this->connection, message, &this->sequence)) { return; }
 dbus_connection_flush(this->connection);

 dbus_message_unref(message);
 this->sequence++;
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

 dbus_connection_flush(this->connection);
 this->terminate_thread = 1;

 dbus_connection_close(this->connection);

 return status_ok;
}

void *einit_dbus::message_thread_bootstrap(void *e) {
 einit_main_dbus_class.message_thread();

 return NULL;
}

void einit_dbus::message_thread() {
 DBusMessage *message;

 while (!(this->terminate_thread)) {
  dbus_connection_read_write(this->connection, -1); // wait until there's something to do
  message = dbus_connection_pop_message(this->connection);

  if (message) {
  }
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
