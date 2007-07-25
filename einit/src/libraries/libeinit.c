/*
 *  libeinit.c
 *  einit
 *
 *  Created by Magnus Deininger on 24/07/2007.
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

#include <dbus/dbus.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

DBusError einit_dbus_error;
DBusConnection *einit_dbus_connection;

char einit_connect() {
 dbus_error_init(&einit_dbus_error);

 if (!(einit_dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &einit_dbus_error))) {
  if (dbus_error_is_set(&einit_dbus_error)) {
   fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error.message);
   dbus_error_free(&einit_dbus_error);
  }
  return 0;
 }

 dbus_connection_set_exit_on_disconnect(einit_dbus_connection, FALSE);

 return 1;
}

char *einit_ipc(char *command) {
 char *returnvalue;

 DBusMessage *message;
 DBusMessageIter args;
 DBusPendingCall *pending;

 if (!(message = dbus_message_new_method_call("org.einit.Einit", "/org/einit/einit", "org.einit.Einit.Command", "IPC"))) {
  fprintf(stderr, "Sending message failed.\n");
  return NULL;
 }

 dbus_message_iter_init_append(message, &args);
 if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &command)) { 
  fprintf(stderr, "Out Of Memory!\n"); 
  return NULL;
 }

 if (!dbus_connection_send_with_reply (einit_dbus_connection, message, &pending, -1)) {
  fprintf(stderr, "Out Of Memory!\n");
  return NULL;
 }
 if (!pending) { 
  fprintf(stderr, "No return value?\n"); 
  return NULL;
 }
 dbus_connection_flush(einit_dbus_connection);

 dbus_message_unref(message);

 dbus_pending_call_block(pending);

 if (!(message = dbus_pending_call_steal_reply(pending))) {
  fprintf(stderr, "Bad Reply\n");
  return NULL;
 }
 dbus_pending_call_unref(pending);

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n"); 
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
  fprintf(stderr, "Argument is not a string...?\n"); 
 else
  dbus_message_iter_get_basic(&args, &returnvalue);

 if (returnvalue) returnvalue = estrdup (returnvalue);

 dbus_message_unref(message);

 return returnvalue;
}

char *einit_ipc_request(char *command) {
 if (einit_connect()) {
  return einit_ipc(command);
 }

 return NULL;
}

char *einit_ipc_request_xml(char *command) {
 char *tmp;
 uint32_t len;

 if (!command) return NULL;
 tmp = emalloc ((len = (strlen (command) + 8)));

 esprintf (tmp, len, "%s --xml", command);

 return einit_ipc_request(tmp);
}
