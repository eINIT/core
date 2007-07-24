/*
 *  einit-control-dbus.c
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <einit/config.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <libgen.h>

#include <einit/bitch.h>
#include <einit/utility.h>
#include <einit/config.h>

DBusError error;
DBusConnection *connection;

int print_usage_info () {
 fputs ("eINIT " EINIT_VERSION_LITERAL " Control\nCopyright (c) 2006, 2007, Magnus Deininger\nUsage:\n einit-control-dbus [-v] [-h] [function] [--] command\n [function] [-v] [-h] [--] command\n\npossible commands for function \"power\":\n down   tell einit to shut down the computer\n reset  reset/reboot the computer\n\nNOTE: calling einit-control [function] [command] is equivalent to calling [function] [command] directly.\n  (provided that the proper symlinks are in place.)\n", stderr);
 return -1;
}

int send_ipc_dbus (char *command) {
 DBusMessage *message;
 DBusMessageIter args;
 DBusPendingCall *pending;

 dbus_error_init(&error);
 if (!(connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error))) {
  if (dbus_error_is_set(&error)) {
   fprintf(stderr, "Connection Error (%s)\n", error.message);
   dbus_error_free(&error);
  }
  exit(1);
 }

 if (!(message = dbus_message_new_method_call("org.einit.Einit", "/org/einit/einit", "org.einit.Einit.Command", "IPC"))) {
  fprintf(stderr, "Sending message failed.\n");
  exit (1);
 }

 dbus_message_iter_init_append(message, &args);
 if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &command)) { 
  fprintf(stderr, "Out Of Memory!\n"); 
  exit(1);
 }

 if (!dbus_connection_send_with_reply (connection, message, &pending, -1)) {
  fprintf(stderr, "Out Of Memory!\n");
  exit(1);
 }
 if (!pending) { 
  fprintf(stderr, "No return value?\n"); 
  exit(1); 
 }
 dbus_connection_flush(connection);

 dbus_message_unref(message);

 char *returnvalue;

 dbus_pending_call_block(pending);

 if (!(message = dbus_pending_call_steal_reply(pending))) {
  fprintf(stderr, "Bad Reply\n");
  exit(1);
 }
 dbus_pending_call_unref(pending);

 if (!dbus_message_iter_init(message, &args))
  fprintf(stderr, "Message has no arguments!\n"); 
 else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
  fprintf(stderr, "Argument is not a string...?\n"); 
 else
  dbus_message_iter_get_basic(&args, &returnvalue);

/* if (!dbus_message_iter_next(&args))
  fprintf(stderr, "Message has too few arguments!\n"); 
 else if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args)) 
  fprintf(stderr, "Argument is not int!\n"); 
 else
  dbus_message_iter_get_basic(&args, &level);*/

 fputs (returnvalue, stdout);
 fputs ("\n", stdout);

 dbus_message_unref(message);

 return 0;
}

int main(int argc, char **argv) {
 int i, l, ret = 0;
 char *c = emalloc (1*sizeof (char));
 char *name = estrdup ((char *)basename(argv[0]));
 char ansi = 0;
 c[0] = 0;
 if (strmatch (name, "erc")) {
  c = (char *)erealloc (c, 3*sizeof (char));
  c = strcat (c, "rc");
 } else if (strcmp (name, "einit-control-dbus")) {
  c = (char *)erealloc (c, (1+strlen(name))*sizeof (char));
  c = strcat (c, name);
 }

 struct winsize ws;
 if (!ioctl (1, TIOCGWINSZ, &ws) || !(errno == ENOTTY)) {
  ansi = 1;
 }

 for (i = 1; i < argc; i++) {
  if (argv[i][0] == '-')
   switch (argv[i][1]) {
    case 'h':
     return print_usage_info ();
     break;
    case 'v':
     eputs("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006, 2007, Magnus Deininger\n", stdout);
     return 0;
    case '-':
     i++;
     if (i < argc) goto copy_remainder_verbatim;
     return 0;
   }
   else while (i < argc) {
    copy_remainder_verbatim:
     l = strlen(c);
    if (l) {
     c = erealloc (c, (l+2+strlen(argv[i]))*sizeof (char));
     c[l] = ' ';
     c[l+1] = 0;
    } else {
     c = erealloc (c, (1+strlen(argv[i]))*sizeof (char));
    }
    c = strcat (c, argv[i]);

    i++;
   }
 }

 if (ansi) {
  c = erealloc (c, (strlen(c)+8)*sizeof (char));
  c = strcat (c, " --ansi");
 }

 ret = send_ipc_dbus(c);

 return 0;
}
