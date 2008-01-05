/*
 *  linux-network-wpa-supplicant.c
 *  einit
 *
 *  Created on 04/01/2008.
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

int linux_network_wpa_supplicant_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_network_wpa_supplicant_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Network Helpers (Linux, WPA Supplicant)",
 .rid       = "linux-network",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_network_wpa_supplicant_configure
};

module_register(linux_network_wpa_supplicant_self);

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

void linux_network_wpa_supplicant_interface_construct (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct cfgnode *node = d->functions->get_option(ev->string, "kernel-modules");
 if (node && node->svalue) {
  char buffer[BUFFERSIZE];

  esprintf (buffer, BUFFERSIZE, "kern-%s", ev->string);

  if (!inset ((const void **)d->static_descriptor->si.requires, buffer, SET_TYPE_STRING)) {
//   fprintf (stderr, "%s\n", buffer);

   d->static_descriptor->si.requires =
    (char **)setadd ((void **)d->static_descriptor->si.requires, buffer, SET_TYPE_STRING);
  }

  struct cfgnode newnode;

  memset (&newnode, 0, sizeof(struct cfgnode));

  esprintf (buffer, BUFFERSIZE, "configuration-kernel-modules-%s", ev->string);
  newnode.id = estrdup (buffer);
  newnode.type = einit_node_regular;

  esprintf (buffer, BUFFERSIZE, "kernel-module-%s", ev->string);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)buffer, SET_TYPE_STRING);

  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"s", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)node->svalue, SET_TYPE_STRING);

  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"provide-service", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"yes", SET_TYPE_STRING);

  newnode.svalue = newnode.arbattrs[3];

  cfg_addnode (&newnode);
 }
}

void linux_network_wpa_supplicant_interface_prepare (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 char buffer[BUFFERSIZE];
 char **ip_binary = which ("ip");

 buffer[0] = 0;

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

void linux_network_wpa_supplicant_address_static (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct stree *st = d->functions->get_all_addresses (ev->string);
 if (st) {
  struct stree *cur = streelinear_prepare (st);

  while (cur) {
   char dhcp = 0;
   char *address = NULL;

   if (cur->value) {
    char **v = cur->value;
    int i = 0;

    for (; v[i]; i+=2) {
     if (strmatch (v[i], "address")) {
      if (strmatch (v[i+1], "dhcp")) {
       dhcp = 1;
      } else {
       address = v[i+1];
      }
     }
    }
   }

   if (!dhcp && address) {
    char buffer[BUFFERSIZE];
    char **ip_binary = which ("ip");

    buffer[0] = 0;

    if (ip_binary) {
     char *aftype;

     if (strmatch (cur->key, "ipv4"))
      aftype = "inet";
     else if (strmatch (cur->key, "ipv4"))
      aftype = "inet6";
     else
      aftype = cur->key;

/* looks like we have the ip command handy, so let's use it */
     if (d->action == interface_up) {
      esprintf (buffer, BUFFERSIZE, "ip -f %s addr add local %s dev %s", aftype, address, ev->string);
     } else if (d->action == interface_down) {
      esprintf (buffer, BUFFERSIZE, "ip -f %s addr delete local %s dev %s", aftype, address, ev->string);
     }

     efree (ip_binary);
    } else {
/* fall back to ifconfig -- this means we get to use only one ip address per interface */

     if (d->action == interface_up) {
      char *aftype;

      if (strmatch (cur->key, "ipv4"))
       aftype = "inet";
      else if (strmatch (cur->key, "ipv4"))
       aftype = "inet6";
      else
       aftype = cur->key;

      if (strmatch (aftype, "inet"))
       esprintf (buffer, BUFFERSIZE, "ifconfig %s %s %s", ev->string, aftype, address);
      else
       esprintf (buffer, BUFFERSIZE, "ifconfig %s %s add %s", ev->string, aftype, address);
     } else if (d->action == interface_down) {
/* ifconfig only seems to be able to handle ipv6 interfaces like this */
      if (strmatch (cur->key, "ipv6")) {
       esprintf (buffer, BUFFERSIZE, "ifconfig %s inet6 del %s", ev->string, address);
      }
     }
    }

    if (buffer[0]) {
     if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
      fbprintf (d->feedback, "command failed: %s", buffer);
      d->status = status_failed;
      break;
     }
    }
   }

   cur = streenext(cur);
  }

  streefree (st);
 }
}

int linux_network_wpa_supplicant_cleanup (struct lmodule *pa) {
 exec_cleanup (pa);

#if 0
 event_ignore (einit_network_interface_construct, linux_network_wpa_supplicant_interface_construct);
 event_ignore (einit_network_interface_update, linux_network_wpa_supplicant_interface_construct);
 event_ignore (einit_network_address_static, linux_network_wpa_supplicant_address_static);
 event_ignore (einit_network_interface_prepare, linux_network_wpa_supplicant_interface_prepare);
#endif

 return 0;
}

int linux_network_wpa_supplicant_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 pa->cleanup = linux_network_wpa_supplicant_cleanup;


#if 0
 event_listen (einit_network_interface_construct, linux_network_wpa_supplicant_interface_construct);
 event_listen (einit_network_interface_update, linux_network_wpa_supplicant_interface_construct);
 event_listen (einit_network_address_static, linux_network_wpa_supplicant_address_static);
 event_listen (einit_network_interface_prepare, linux_network_wpa_supplicant_interface_prepare);
#endif

 return 0;
}
