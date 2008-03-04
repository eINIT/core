/*
 *  linux-network-bonding.c
 *  einit
 *
 *  Created on 26/01/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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

#include <sys/types.h>
#include <sys/stat.h>

#include <einit-modules/network.h>
#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_network_bonding_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_network_bonding_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
 .name      = "Network Helpers (Linux, Bonding)",
 .rid       = "linux-network-bonding",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_network_bonding_configure
};

module_register(linux_network_bonding_self);

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

void linux_network_bonding_interface_construct (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 if (strprefix (d->static_descriptor->rid, "interface-carrier-")) {
  struct cfgnode *node = d->functions->get_option(ev->string, "bond");
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

   struct stat st;
   if (!stat ("/sys/class", &st) && stat ("/sys/class/net/bonding_masters", &st)) {
    if (!d->static_descriptor->si.requires || !inset ((const void **)d->static_descriptor->si.requires, "kern-bonding", SET_TYPE_STRING)) {
     d->static_descriptor->si.requires =
       set_str_add (d->static_descriptor->si.requires, "kern-bonding");
    }

    struct cfgnode newnode;

    memset (&newnode, 0, sizeof(struct cfgnode));

    newnode.id = (char *)str_stabilise ("configuration-kernel-modules-bonding");
    newnode.type = einit_node_regular;

    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"id");
    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"kernel-module-bonding");

    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"s");
    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"bonding");

    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"provide-service");
    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"yes");

    newnode.svalue = newnode.arbattrs[3];

    cfg_addnode (&newnode);
   }
  }
 }
}

void linux_network_bonding_verify_carrier (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct cfgnode *node = d->functions->get_option(ev->string, "bond");
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

  struct stat st;
  if (stat ("/sys/class/net/bonding_masters", &st)) {
/* make sure the bonding driver is loaded... if not, bail */
   fbprintf (d->feedback, "bridging driver not detected");
   d->status = status_failed;
   return;
  }

  if (d->action == interface_up) {
   esprintf (buffer, BUFFERSIZE, "/sys/class/net/%s", ev->string);
   if (stat (buffer, &st)) {
    FILE *f = fopen("/sys/class/net/bonding_masters", "w");

    if (f) {
     esprintf (buffer, BUFFERSIZE, "+%s\n", ev->string);

     fputs (buffer, f);
     fclose (f);
    }
   }

   char **ip_binary = which ("ip");

   if (d->action == interface_up) {
    if (ip_binary) {
     /* looks like we have the ip command handy, so let's use it */
     efree (ip_binary);

     esprintf (buffer, BUFFERSIZE, "ip link set %s up", ev->string);
    } else {
     /* fall back to ifconfig -- this means we get to use only one ip address per interface */

     esprintf (buffer, BUFFERSIZE, "ifconfig %s up", ev->string);
    }

    if (buffer[0]) {
     if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
      fbprintf (d->feedback, "command failed: %s", buffer);
      d->status = status_failed;
     }
    }
   }
  }

  if (elements) {
   for (i = 0; elements[i]; i++) {
    if ((d->action == interface_up) || (d->action == interface_down)) {
     esprintf (buffer, BUFFERSIZE, "/sys/class/net/%s/bonding/slaves", ev->string);
     FILE *f = fopen(buffer, "w");

     if (f) {
      if (d->action == interface_up) {
       char **ip_binary = which ("ip");

       if (d->action == interface_up) {
        if (ip_binary) {
         efree (ip_binary);

         esprintf (buffer, BUFFERSIZE, "ip link set %s down", elements[i]);
        } else {
         esprintf (buffer, BUFFERSIZE, "ifconfig %s down", elements[i]);
        }

        if (buffer[0]) {
         if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
          fbprintf (d->feedback, "command failed: %s", buffer);
         }
        }
       }

       esprintf (buffer, BUFFERSIZE, "+%s\n", elements[i]);
      } else if (d->action == interface_down) {
       esprintf (buffer, BUFFERSIZE, "-%s\n", elements[i]);
      }

      fputs (buffer, f);
      fclose (f);
     }
    }
   }

   efree (elements);
  }

  if (d->action == interface_down) {
   /* remove bond */
   esprintf (buffer, BUFFERSIZE, "/sys/class/net/%s", ev->string);
   if (!stat (buffer, &st)) {
    FILE *f = fopen("/sys/class/net/bonding_masters", "w");

    if (f) {
     esprintf (buffer, BUFFERSIZE, "-%s\n", ev->string);

     fputs (buffer, f);
     fclose (f);
    }
   }
  }
 }
}

int linux_network_bonding_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 event_listen (einit_network_interface_construct, linux_network_bonding_interface_construct);
 event_listen (einit_network_interface_update, linux_network_bonding_interface_construct);
 event_listen (einit_network_verify_carrier, linux_network_bonding_verify_carrier);

 return 0;
}
