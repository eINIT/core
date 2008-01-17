/*
 *  linux-network.c
 *  einit
 *
 *  Created on 03/01/2008.
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

int linux_network_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_network_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Network Helpers (Linux)",
 .rid       = "linux-network",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_network_configure
};

module_register(linux_network_self);

#endif

char **linux_network_interfaces = NULL;

pthread_mutex_t linux_network_interfaces_mutex = PTHREAD_MUTEX_INITIALIZER;

char **linux_network_list_interfaces_proc (int spawn_events) {
 char **interfaces = NULL;
 char **new_interfaces = NULL;
 char *buffer = readfile ("/proc/net/dev");

 if (buffer) {
  char **buffer_lines = str2set ('\n', buffer);
  efree (buffer);

  int i = 0;
  for (; buffer_lines[i]; i++) {
   strtrim (buffer_lines[i]);
   char **line_split = str2set(':', buffer_lines[i]);
   if (line_split[1]) { /* have at least two elements: it's one of the interface stat lines */
    interfaces = (char **)setadd((void **)interfaces, line_split[0], SET_TYPE_STRING);
   }
   efree (line_split);
  }
  efree (buffer_lines);
 }

 if (spawn_events) {
  if (interfaces) {
   emutex_lock (&linux_network_interfaces_mutex);
   int i = 0;
   for (; interfaces[i]; i++) {
    if (!linux_network_interfaces || !inset ((const void **)linux_network_interfaces, interfaces[i], SET_TYPE_STRING))
     new_interfaces = (char **)setadd ((void **)new_interfaces, interfaces[i], SET_TYPE_STRING);
   }
   emutex_unlock (&linux_network_interfaces_mutex);
  }

  if (new_interfaces) {
// spawn some events here about the new interfaces
   efree (new_interfaces);
  }
 }

 return interfaces;
}

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

/*
void linux_network_interface_configure (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct stree *st = d->functions->get_all_addresses(ev->string);

 if (st) {
  struct stree *cur = streelinear_prepare (st);

  while (cur) {
   char **v = cur->value;
   int y = 0;

   for (; v[y]; y+=2) {
    if (strmatch (v[y], "address")) {
     fprintf (stderr, "configured %s address for interfaces %s: %s\n", cur->key, ev->string, v[y+1]);
    }
   }

   cur = streenext (cur);
  }

  streefree (st);
 } else {
  fprintf (stderr, "no addresses for interface %s?\n", ev->string);
 }
}
*/

void linux_network_interface_construct (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct cfgnode *node = d->functions->get_option(ev->string, "kernel-modules");
 if (node && node->svalue) {
  char buffer[BUFFERSIZE];

  esprintf (buffer, BUFFERSIZE, "kern-%s", ev->string);

  if (!d->static_descriptor->si.requires || !inset ((const void **)d->static_descriptor->si.requires, buffer, SET_TYPE_STRING)) {
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

void linux_network_interface_prepare (struct einit_event *ev) {
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

void linux_network_interface_done (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 char buffer[BUFFERSIZE];

 buffer[0] = 0;

 if (d->action == interface_down) {
  char **ip_binary = which ("ip");

  if (ip_binary) {
  /* looks like we have the ip command handy, so let's use it */
   efree (ip_binary);

   if (d->action == interface_down) {
    esprintf (buffer, BUFFERSIZE, "ip link set %s down", ev->string);
   }
  } else {
  /* fall back to ifconfig -- this means we get to use only one ip address per interface */

   if (d->action == interface_down) {
    esprintf (buffer, BUFFERSIZE, "ifconfig %s down", ev->string);
   }
  }

  struct cfgnode **dns = d->functions->get_multiple_options (ev->string, "nameserver");

  if (dns) {
   char **resolvconf_binary = which ("resolvconf");

   if (resolvconf_binary) {
    efree (resolvconf_binary);

    if (buffer[0]) {
     if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
      fbprintf (d->feedback, "command failed: %s", buffer);
     }
    }

    esprintf (buffer, BUFFERSIZE, "resolvconf -d %s", ev->string);
   }
  }

  if (buffer[0]) {
   if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
    fbprintf (d->feedback, "command failed: %s", buffer);
//   d->status = status_failed;
// don't fail just because we can't set the interface down
   }
  }
 } else if (d->action == interface_up) {
  struct cfgnode **dns = d->functions->get_multiple_options (ev->string, "nameserver");

  if (dns) {
   char **resolv_conf_data = NULL;
   char *resolv_conf = NULL;
   int i = 0;

   for (; dns[i]; i++) {
    if (dns[i]->arbattrs) {
     char buffer[BUFFERSIZE];
     int j = 0;
     for (; dns[i]->arbattrs[j]; j+=2) {
      if (strmatch (dns[i]->arbattrs[j], "address")) {
       esprintf (buffer, BUFFERSIZE, "nameserver %s", dns[i]->arbattrs[j+1]);
       resolv_conf_data = (char **)setadd ((void **)resolv_conf_data, buffer, SET_TYPE_STRING);
      } else if (strmatch (dns[i]->arbattrs[j], "options")) {
       esprintf (buffer, BUFFERSIZE, "options %s", dns[i]->arbattrs[j+1]);
       resolv_conf_data = (char **)setadd ((void **)resolv_conf_data, buffer, SET_TYPE_STRING);
      } else if (strmatch (dns[i]->arbattrs[j], "sortlist")) {
       esprintf (buffer, BUFFERSIZE, "sortlist %s", dns[i]->arbattrs[j+1]);
       resolv_conf_data = (char **)setadd ((void **)resolv_conf_data, buffer, SET_TYPE_STRING);
      } else if (strmatch (dns[i]->arbattrs[j], "search")) {
       esprintf (buffer, BUFFERSIZE, "search %s", dns[i]->arbattrs[j+1]);
       resolv_conf_data = (char **)setadd ((void **)resolv_conf_data, buffer, SET_TYPE_STRING);
      } else if (strmatch (dns[i]->arbattrs[j], "domain")) {
       esprintf (buffer, BUFFERSIZE, "domain %s", dns[i]->arbattrs[j+1]);
       resolv_conf_data = (char **)setadd ((void **)resolv_conf_data, buffer, SET_TYPE_STRING);
      }
     }
    }
   }

   if (resolv_conf_data) {
    resolv_conf = set2str ('\n', (const char **)resolv_conf_data);
    efree (resolv_conf_data);
   }

   if (resolv_conf) {
    char **resolvconf_binary = which ("resolvconf");

    if (resolvconf_binary) {
     efree (resolvconf_binary);

     fbprintf (d->feedback, "updating resolv.conf using resolvconf");

     esprintf (buffer, BUFFERSIZE, "resolvconf -a %s", ev->string);

     FILE *f = popen (buffer, "w");
     if (f) {
      fputs (resolv_conf, f);
      fputs ("\n", f);
      pclose (f);
     }
    } else {
     fbprintf (d->feedback, "overwriting old resolv.conf");
     FILE *f = fopen ("/etc/resolv.conf", "w");
     if (f) {
      fputs (resolv_conf, f);
      fputs ("\n", f);
      fclose (f);
     }
    }

    efree (resolv_conf);
   }
  }
 }
}

void linux_network_address_static (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 char ip_binary = 0;
 char **ipwhich = which ("ip");
 if (ipwhich) {
  ip_binary = 1;
  efree (ipwhich);
  ipwhich = NULL;
 }

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
    buffer[0] = 0;

    if (ip_binary) {
/* looks like we have the ip command handy, so let's use it */
     if (d->action == interface_up) {
/* silently try to erase the address first, then re-add it */
      if (strmatch (cur->key, "ipv4")) {
       esprintf (buffer, BUFFERSIZE, "ip -f inet addr delete local %s dev %s", address, ev->string);
       pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
       esprintf (buffer, BUFFERSIZE, "ip -f inet addr add local %s dev %s", address, ev->string);
      } else if (strmatch (cur->key, "ipv6")) {
       esprintf (buffer, BUFFERSIZE, "ip -f inet6 addr delete local %s dev %s", address, ev->string);
       pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
       esprintf (buffer, BUFFERSIZE, "ip -f inet6 addr add local %s dev %s", address, ev->string);
      } else {
       esprintf (buffer, BUFFERSIZE, "ip -f %s addr delete local %s dev %s", cur->key, address, ev->string);
       pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
       esprintf (buffer, BUFFERSIZE, "ip -f %s addr add local %s dev %s", cur->key, address, ev->string);
      }
     } else if (d->action == interface_down) {
      if (strmatch (cur->key, "ipv4")) {
       esprintf (buffer, BUFFERSIZE, "ip -f inet addr delete local %s dev %s", address, ev->string);
      } else if (strmatch (cur->key, "ipv6")) {
       esprintf (buffer, BUFFERSIZE, "ip -f inet6 addr delete local %s dev %s", address, ev->string);
      } else {
       esprintf (buffer, BUFFERSIZE, "ip -f %s addr delete local %s dev %s", cur->key, address, ev->string);
      }
     }
    } else {
/* fall back to ifconfig -- this means we get to use only one ip address per interface */
     if (d->action == interface_up) {
      if (strmatch (cur->key, "ipv4"))
       esprintf (buffer, BUFFERSIZE, "ifconfig %s inet %s", ev->string, address);
      else if (strmatch (cur->key, "ipv6"))
       esprintf (buffer, BUFFERSIZE, "ifconfig %s inet6 add %s", ev->string, address);
      else
       esprintf (buffer, BUFFERSIZE, "ifconfig %s %s add %s", ev->string, cur->key, address);
     } else if (d->action == interface_down) {
/* ifconfig only seems to be able to handle ipv6 interfaces like this */
      if (strmatch (cur->key, "ipv6")) {
       esprintf (buffer, BUFFERSIZE, "ifconfig %s inet6 del %s", ev->string, address);
      }
     }
    }

    if (buffer[0]) {
     if ((pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) && (d->action == interface_up)) {
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

 if (d->status != status_failed) {
  struct cfgnode **rt = d->functions->get_multiple_options (ev->string, "route");

  if (rt) {
   int i = 0;
   for (; rt[i]; i++) {
    if (rt[i]->arbattrs) {
     int j = 0;
     char *network = NULL;
     char *gateway = NULL;
     char buffer[BUFFERSIZE];
     buffer[0] = 0;

     for (; rt[i]->arbattrs[j]; j+=2) {
      if (strmatch (rt[i]->arbattrs[j], "network")) {
       network = rt[i]->arbattrs[j+1];
      } else if (strmatch (rt[i]->arbattrs[j], "gateway")) {
       gateway = rt[i]->arbattrs[j+1];
      }
     }

     if (d->action == interface_up) {
      if (gateway) {
       if (network) {
        if (ip_binary) {
         esprintf (buffer, BUFFERSIZE, "ip route del %s via %s dev %s", network, gateway, ev->string);
         pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
         esprintf (buffer, BUFFERSIZE, "ip route add %s via %s dev %s", network, gateway, ev->string);
        } else {
         esprintf (buffer, BUFFERSIZE, "route del -net %s gw %s dev %s", network, gateway, ev->string);
         pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
         esprintf (buffer, BUFFERSIZE, "route add -net %s gw %s dev %s", network, gateway, ev->string);
        }
       } else {
        if (ip_binary) {
         esprintf (buffer, BUFFERSIZE, "ip route del via %s dev %s", gateway, ev->string);
         pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
         esprintf (buffer, BUFFERSIZE, "ip route add via %s dev %s", gateway, ev->string);
        } else {
         esprintf (buffer, BUFFERSIZE, "route del -net default gw %s dev %s", gateway, ev->string);
         pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
         esprintf (buffer, BUFFERSIZE, "route add -net default gw %s dev %s", gateway, ev->string);
        }
       }
      } else if (network) {
       if (ip_binary) {
        esprintf (buffer, BUFFERSIZE, "ip route del %s dev %s", network, ev->string);
        pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
        esprintf (buffer, BUFFERSIZE, "ip route add %s dev %s", network, ev->string);
       } else {
        esprintf (buffer, BUFFERSIZE, "route del -net %s dev %s", network, ev->string);
        pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
        esprintf (buffer, BUFFERSIZE, "route add -net %s dev %s", network, ev->string);
       }
      }

      if (buffer[0]) {
       if (pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) {
        fbprintf (d->feedback, "command failed: %s", buffer);
        d->status = status_failed;
        break;
       }
      }
     } else if (d->action == interface_down) {
      if (gateway) {
       if (network) {
        if (ip_binary) {
         esprintf (buffer, BUFFERSIZE, "ip route del %s via %s dev %s", network, gateway, ev->string);
        } else {
         esprintf (buffer, BUFFERSIZE, "route del -net %s gw %s dev %s", network, gateway, ev->string);
        }
       } else {
        if (ip_binary) {
         esprintf (buffer, BUFFERSIZE, "ip route del via %s dev %s", gateway, ev->string);
        } else {
         esprintf (buffer, BUFFERSIZE, "route del -net default gw %s dev %s", gateway, ev->string);
        }
       }
      } else if (network) {
       if (ip_binary) {
        esprintf (buffer, BUFFERSIZE, "ip route del %s dev %s", network, ev->string);
       } else {
        esprintf (buffer, BUFFERSIZE, "route del -net %s dev %s", network, ev->string);
       }
      }

      if (buffer[0]) {
       if ((pexec (buffer, NULL, 0, 0, NULL, NULL, NULL, d->feedback) == status_failed) && (d->action == interface_up)) {
        fbprintf (d->feedback, "command failed: %s", buffer);
        d->status = status_failed;
        break;
       }
      }
     }
    }
   }

   efree (rt);
  }
 }
}

void linux_network_verify_carrier (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct cfgnode *node = d->functions->get_option(ev->string, "wpa-supplicant");
 if (!node) {
/* only do link carrier detection if we're NOT relying on wpa-supplicant, if
  we do, then wpa-sup will do that detection already */
  char buffer[BUFFERSIZE];

  esprintf (buffer, BUFFERSIZE, "/sys/class/net/%s/carrier", ev->string);

  int repe = 5;

  while (repe) {
   FILE *f = fopen(buffer, "r");
   if (f) {
    char t[BUFFERSIZE];

    if (fgets (t, BUFFERSIZE, f)) {
     strtrim(t);
     if (strmatch (t, "0")) {
      if (repe != 1) {
       fbprintf (d->feedback, "no carrier, waiting for %i seconds", (repe -1));
      } else {
       fbprintf (d->feedback, "no carrier, giving up");
      }
     }
    }
    fclose (f);
   } else {
    break;
   }

   if (repe != 1) sleep (1);
   repe--;
  }

  if (!repe) {
   d->status = status_failed;
  }
 }
}

int linux_network_cleanup (struct lmodule *pa) {
 exec_cleanup (pa);

 function_unregister ("network-list-interfaces-linux", 1, (void *)linux_network_list_interfaces_proc);
 function_unregister ("network-list-interfaces-generic", 1, (void *)linux_network_list_interfaces_proc);

#if 0
 event_ignore (einit_network_interface_configure, linux_network_interface_configure);
#endif
 event_ignore (einit_network_interface_construct, linux_network_interface_construct);
 event_ignore (einit_network_interface_update, linux_network_interface_construct);
 event_ignore (einit_network_address_static, linux_network_address_static);
 event_ignore (einit_network_interface_prepare, linux_network_interface_prepare);
 event_ignore (einit_network_interface_done, linux_network_interface_done);
 event_ignore (einit_network_verify_carrier, linux_network_verify_carrier);

 return 0;
}

int linux_network_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 pa->cleanup = linux_network_cleanup;

 function_register ("network-list-interfaces-linux", 1, (void *)linux_network_list_interfaces_proc);
 function_register ("network-list-interfaces-generic", 1, (void *)linux_network_list_interfaces_proc);

#if 0
 event_listen (einit_network_interface_configure, linux_network_interface_configure);
#endif
 event_listen (einit_network_interface_construct, linux_network_interface_construct);
 event_listen (einit_network_interface_update, linux_network_interface_construct);
 event_listen (einit_network_address_static, linux_network_address_static);
 event_listen (einit_network_interface_prepare, linux_network_interface_prepare);
 event_listen (einit_network_interface_done, linux_network_interface_done);
 event_listen (einit_network_verify_carrier, linux_network_verify_carrier);

 return 0;
}
