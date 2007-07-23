/*
 *  exec.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 23/11/2006.
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
#include <einit/event.h>

#ifndef EINIT_MODULES_DBUS_H_
#define EINIT_MODULES_DBUS_H_

/*const char * einit_dbus_introspection_data =
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
  "<node name=\"/org/einit/ipc\">"
   "<interface name=\"org.einit.IPC\">"
    "<annotation name=\"org.einit.DBus.IPC\" value=\"einit_dbus\"/>"
    "<method name=\"configure\">"
     "<annotation name=\"org.einit.DBus.IPC.configure\" value=\"einit_dbus_configure\"/>"
    "</method>"
    "<method name=\"event_emit\">"
     "<annotation name=\"org.einit.DBus.IPC.string\" value=\"einit_dbus_string\"/>"
     "<arg type=\"s\" name=\"string\" direction=\"in\" />"
     "<arg type=\"s\" name=\"result\" direction=\"out\" />"
    "</method>"
   "</interface>"
  "</node>";*/

class einit_dbus {
 public:
//  const char *introspection_data;
  int configure ();

  einit_dbus();
  ~einit_dbus();

  void string(const char *IN_string, char ** OUT_result);
  void signal_dbus (const char *IN_string);

  int enable (struct einit_event *);
  int disable (struct einit_event *);

 private:
  DBusError error;
  DBusConnection* connection;
  dbus_uint32_t signal_seqid;

  static void ipc_event_handler (struct einit_event *);
};

#endif
