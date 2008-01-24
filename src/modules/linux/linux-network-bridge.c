/*
 *  linux-network-bridge.c
 *  einit
 *
 *  Created on 21/01/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
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
#include <unistd.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <pthread.h>
#include <string.h>

#include <einit-modules/network.h>
#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_network_bridge_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_network_bridge_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Network Helpers (Linux, Bridges)",
 .rid       = "linux-network-bridge",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_network_bridge_configure
};

module_register(linux_network_bridge_self);

#endif

/* reminder:

 einit_network_interface_construct  = einit_event_subsystem_network  | 0x001,
 einit_network_interface_configure  = einit_event_subsystem_network  | 0x002,
 einit_network_interface_update     = einit_event_subsystem_network  | 0x003,

 einit_network_interface_prepare    = einit_event_subsystem_network  | 0x011,
 einit_network_verify_carrier       = einit_event_subsystem_network  | 0x012,
 einit_network_kill_carrier         = einit_event_subsystem_network  | 0x013,
 einit_network_address_automatic    = einit_event_subsystem_network  | 0x014,
 einit_network_address_static       = einit_event_subsystem_network  | 0x015,
 einit_network_interface_done       = einit_event_subsystem_network  | 0x016,

 einit_network_interface_cancel     = einit_event_subsystem_network  | 0x020,

struct network_functions {
 int (*have_options) (char *);
 struct cfgnode * (*get_option) (char *, char *);
 struct stree * (*get_all_addresses) (char *);
};

enum interface_action {
 interface_nop = 0,
 interface_up,
 interface_down,
 interface_refresh_ip
};

struct network_event_data {
 struct network_functions *functions;
 struct lmodule *module;
 struct smodule *static_descriptor;
 enum interface_flags flags;
 int status;
 enum interface_action action;
 struct einit_event *feedback;
};
*/

void linux_network_bridge_interface_construct (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 if (strstr (d->static_descriptor->rid, "interface-carrier-") == d->static_descriptor->rid) {
  struct cfgnode *node = d->functions->get_option(ev->string, "bridge");
  if (node) {
   char **elements = NULL;

   char buffer[BUFFERSIZE];

   int i = 0;

   if (node->arbattrs) {
    for (; node->arbattrs[i]; i += 2) {
     if (strmatch (node->arbattrs[i], "elements")) {
      elements = str2set (':', node->arbattrs[i+1]);
     }
    }
   }

   if (elements) {
    for (i = 0; elements[i]; i++) {
     esprintf (buffer, BUFFERSIZE, "carrier-%s", elements[i]);

     if (!inset ((const void **)d->static_descriptor->si.requires, buffer, SET_TYPE_STRING)) {
      d->static_descriptor->si.requires =
        set_str_add (d->static_descriptor->si.requires, buffer);
     }
    }

    efree (elements);
   }
  }
 }
}

void linux_network_bridge_verify_carrier (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct cfgnode *node = d->functions->get_option(ev->string, "bridge");
 if (node) {
  char **elements = NULL;

  char buffer[BUFFERSIZE];

  int i = 0;

  if (node->arbattrs) {
   for (; node->arbattrs[i]; i += 2) {
    if (strmatch (node->arbattrs[i], "elements")) {
     elements = str2set (':', node->arbattrs[i+1]);
    }
   }
  }

  if (d->action == interface_up) {
   esprintf (buffer, BUFFERSIZE, "brctl addbr %s", ev->string);

   if (buffer[0]) {
    if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
     fbprintf (d->feedback, "command failed: %s", buffer);
     d->status = status_failed;
     return;
    }
   }
  }

  if (elements) {
   for (i = 0; elements[i]; i++) {
    if (d->action == interface_up) {
     esprintf (buffer, BUFFERSIZE, "brctl addif %s %s", ev->string, elements[i]);
    } else {
     esprintf (buffer, BUFFERSIZE, "brctl delif %s %s", ev->string, elements[i]);
    }

    if (buffer[0]) {
     if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
      fbprintf (d->feedback, "command failed: %s", buffer);
      if (d->action == interface_up) {
       d->status = status_failed;

       efree (elements);
       return;
      }
     }
    }
   }

   efree (elements);
  }

  if (d->action == interface_down) {
   esprintf (buffer, BUFFERSIZE, "brctl delbr %s", ev->string);

   if (buffer[0]) {
    if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
     fbprintf (d->feedback, "command failed: %s", buffer);
     d->status = status_failed;
     return;
    }
   }
  }

  char **ip_binary = which ("ip");

  if (d->action == interface_up) {
   if (ip_binary) {
/* looks like we have the ip command handy, so let's use it */
    efree (ip_binary);

    if (d->action == interface_up) {
     esprintf (buffer, BUFFERSIZE, "ip link set %s up", ev->string);
    }
   } else {
/* fall back to ifconfig -- this means we get to use only one ip address per interface */

    if (d->action == interface_up) {
     esprintf (buffer, BUFFERSIZE, "ifconfig %s up", ev->string);
    }
   }

   if (buffer[0]) {
    if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
     fbprintf (d->feedback, "command failed: %s", buffer);
     d->status = status_failed;
    }
   }
  }
 }
}

int linux_network_bridge_cleanup (struct lmodule *pa) {
 exec_cleanup (pa);

 event_ignore (einit_network_interface_construct, linux_network_bridge_interface_construct);
 event_ignore (einit_network_interface_update, linux_network_bridge_interface_construct);
 event_listen (einit_network_verify_carrier, linux_network_bridge_verify_carrier);

 return 0;
}

int linux_network_bridge_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 pa->cleanup = linux_network_bridge_cleanup;

 event_listen (einit_network_interface_construct, linux_network_bridge_interface_construct);
 event_listen (einit_network_interface_update, linux_network_bridge_interface_construct);
 event_listen (einit_network_verify_carrier, linux_network_bridge_verify_carrier);

 return 0;
}
