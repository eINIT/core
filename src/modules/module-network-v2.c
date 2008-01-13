/*
 *  module-network-v2.c
 *  einit
 *
 *  Created on 12/09/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <sys/stat.h>

#include <einit-modules/network.h>
#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_module_network_v2_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_module_network_v2_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (Network, v2)",
 .rid       = "einit-module-network-v2",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_module_network_v2_configure
};

module_register(einit_module_network_v2_self);

#endif

#if defined(BSD)
char *bsd_network_suffixes[] = { "bsd", "generic", NULL };
#elif defined(LINUX)
char *bsd_network_suffixes[] = { "linux", "generic", NULL };
#else
char *bsd_network_suffixes[] = { "generic", NULL };
#endif

struct stree *einit_module_network_v2_interfaces = NULL;
char *einit_module_network_v2_module_functions[] = { "zap", "up", "down", "refresh" };

struct network_v2_interface_descriptor {
 enum interface_flags status;
 struct lmodule *module;
 char *dhcp_client;
};

pthread_mutex_t einit_module_network_v2_interfaces_mutex = PTHREAD_MUTEX_INITIALIZER,
 einit_module_network_v2_get_all_addresses_mutex = PTHREAD_MUTEX_INITIALIZER;

int einit_module_network_v2_scanmodules (struct lmodule *);
int einit_module_network_v2_emit_event (enum einit_event_code type, struct lmodule *module, struct smodule *sd, char *interface, enum interface_action action, struct einit_event *feedback);

#define INTERFACES_PREFIX "configuration-network-interfaces"
#define INTERFACE_DEFAULTS_PREFIX "subsystem-network-interface-defaults"

int einit_module_network_v2_have_options (char *interface) {
 char buffer[BUFFERSIZE];

 esprintf (buffer, BUFFERSIZE, INTERFACES_PREFIX "-%s", interface);

 return cfg_getnode (buffer, NULL) ? 1 : 0;
}

struct cfgnode *einit_module_network_v2_get_option_default_r (char *r, char *option) {
 char buffer[BUFFERSIZE];

 esprintf (buffer, BUFFERSIZE, INTERFACE_DEFAULTS_PREFIX "-%s-%s", r, option);

 return cfg_getnode (buffer, NULL);
}

struct cfgnode *einit_module_network_v2_get_option_default (char *interface, char *option) {
 char *u = cfg_getstring (INTERFACE_DEFAULTS_PREFIX, NULL);

 if (u) {
  char **set = str2set (':', u);
  int i = 0;
  for (; set[i]; i++) {
   struct cfgnode *n;
   char buffer[BUFFERSIZE];
   regex_t r;
   esprintf (buffer, BUFFERSIZE, INTERFACE_DEFAULTS_PREFIX "-%s", set[i]);

   n = cfg_getnode (buffer, NULL);

   if (n && (!n->idattr || !eregcomp(&r, n->idattr))) {
    if (!n->idattr || regexec (&r, interface, 0, NULL, 0) != REG_NOMATCH) {
     if (n->idattr) eregfree (&r);

     struct cfgnode *res = einit_module_network_v2_get_option_default_r(set[i], option);
     if (res) {
      efree (set);
      return res;
     }
    } else {
     if (n->idattr) eregfree (&r);
    }
   }
  }

  efree (set);
 }

 return NULL;
}

struct cfgnode *einit_module_network_v2_get_option (char *interface, char *option) {
 char buffer[BUFFERSIZE];
 struct cfgnode *node;

 esprintf (buffer, BUFFERSIZE, INTERFACES_PREFIX "-%s-%s", interface, option);

 if ((node = cfg_getnode (buffer, NULL)))
  return node;
 else
  return einit_module_network_v2_get_option_default (interface, option);
}

struct cfgnode **einit_module_network_v2_get_multiple_options (char *interface, char *option) {
 char buffer[BUFFERSIZE];
 struct cfgnode *node = NULL;
 struct cfgnode **rv = NULL;

 esprintf (buffer, BUFFERSIZE, INTERFACES_PREFIX "-%s-%s", interface, option);

 while ((node = cfg_findnode (buffer, 0, node))) {
  rv = (struct cfgnode **)setadd ((void **)rv, node, SET_NOALLOC);
 }

 if (rv)
  return rv;
 else {
  if ((node = einit_module_network_v2_get_option_default (interface, option))) {
   rv = (struct cfgnode **)setadd ((void **)rv, node, SET_NOALLOC);
  }

  return rv;
 }
}

struct stree *einit_module_network_v2_get_all_addresses (char *interface) {
 char buffer[BUFFERSIZE];
 struct stree *st;
 struct stree *rv = NULL;

 esprintf (buffer, BUFFERSIZE, INTERFACES_PREFIX "-%s-address-", interface);

 emutex_lock (&einit_module_network_v2_get_all_addresses_mutex);

 st = cfg_prefix (buffer);
 if (st) {
  struct stree *cur = streelinear_prepare (st);
  ssize_t prefixlen = strlen (buffer);

  while (cur) {
   struct cfgnode *n = cur->value;
   if (n->arbattrs) {
    char **narb = (char **)setdup ((const void **)n->arbattrs, SET_TYPE_STRING);

    rv = streeadd (rv, (cur->key + prefixlen), narb, tree_value_noalloc, narb);
   }

   cur = streenext (cur);
  }

  streefree (st);
 } else {
  struct cfgnode *n;
  if ((n = einit_module_network_v2_get_option_default(interface, "address-ipv4")) && n->arbattrs) {
   char **narb = (char **)setdup ((const void **)n->arbattrs, SET_TYPE_STRING);
   rv = streeadd (rv, "ipv4", narb, tree_value_noalloc, narb);
  }

  if ((n = einit_module_network_v2_get_option_default(interface, "address-ipv6")) && n->arbattrs) {
   char **narb = (char **)setdup ((const void **)n->arbattrs, SET_TYPE_STRING);
   rv = streeadd (rv, "ipv6", narb, tree_value_noalloc, narb);
  }
 }

 emutex_unlock (&einit_module_network_v2_get_all_addresses_mutex);

 return rv;
}

/* this structure is needed for all submodules */
struct network_functions einit_module_network_v2_function_list = {
 .have_options = einit_module_network_v2_have_options,
 .get_option = einit_module_network_v2_get_option,
 .get_multiple_options = einit_module_network_v2_get_multiple_options,
 .get_all_addresses = einit_module_network_v2_get_all_addresses
};

int einit_module_network_v2_emit_event (enum einit_event_code type, struct lmodule *module, struct smodule *sd, char *interface, enum interface_action action, struct einit_event *feedback) {
 struct network_event_data d = {
  .functions = &einit_module_network_v2_function_list,
  .module = module,
  .static_descriptor = sd,
  .flags = 0,
  .status = status_idle,
  .action = action,
  .feedback = feedback
 };
 struct einit_event ev = evstaticinit (type);
 struct stree *st = NULL;

 emutex_lock (&einit_module_network_v2_interfaces_mutex);
 if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, interface, tree_find_first);
 if (st) {
  struct network_v2_interface_descriptor *id = st->value;
  if (id)
   d.flags = id->status;
 }
 emutex_unlock (&einit_module_network_v2_interfaces_mutex);

 ev.string = interface;
 ev.para = &d;

 event_emit (&ev, einit_event_flag_broadcast);

 evstaticdestroy (&ev);

 emutex_lock (&einit_module_network_v2_interfaces_mutex);
 if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, interface, tree_find_first);
 if (st) {
  struct network_v2_interface_descriptor *id = st->value;
  if (id)
   id->status = d.flags;
 }
 emutex_unlock (&einit_module_network_v2_interfaces_mutex);

 return d.status;
}

int einit_module_network_v2_module_custom (struct lmodule *m, char *task_s, struct einit_event *status) {
 enum interface_action task = interface_nop;

 if (strmatch (task_s, "up")) task = interface_up;
 else if (strmatch (task_s, "down")) task = interface_down;
 else if (strmatch (task_s, "refresh")) task = interface_refresh_ip;

 if (task != interface_nop) {
  if (einit_module_network_v2_emit_event (einit_network_interface_prepare, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;
 } else goto cancel_fail;

 switch (task) {
  case interface_up:
   if (einit_module_network_v2_emit_event (einit_network_verify_carrier, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;
   if (einit_module_network_v2_emit_event (einit_network_address_automatic, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;
   if (einit_module_network_v2_emit_event (einit_network_address_static, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;

   einit_module_network_v2_emit_event (einit_network_interface_done, m, (struct smodule *)m->module, (m->module->rid + 13), task, status);
   return status_ok;
   break;

  case interface_down:
   if (einit_module_network_v2_emit_event (einit_network_address_static, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;
   if (einit_module_network_v2_emit_event (einit_network_address_automatic, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;

   if (einit_module_network_v2_emit_event (einit_network_kill_carrier, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;

   einit_module_network_v2_emit_event (einit_network_interface_done, m, (struct smodule *)m->module, (m->module->rid + 13), task, status);
   return status_ok;
   break;

  case interface_refresh_ip:
   if (einit_module_network_v2_emit_event (einit_network_address_automatic, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;
   if (einit_module_network_v2_emit_event (einit_network_address_static, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;

  case interface_nop:
   break;
 }

 cancel_fail:
  einit_module_network_v2_emit_event (einit_network_interface_cancel, m, (struct smodule *)m->module, (m->module->rid + 13), task, status);

 return status_failed;
}

int einit_module_network_v2_module_enable (void *p, struct einit_event *status) {
 return einit_module_network_v2_module_custom(p, "up", status);
}

int einit_module_network_v2_module_disable (void *p, struct einit_event *status) {
 return einit_module_network_v2_module_custom(p, "down", status);
}

int einit_module_network_v2_module_configure (struct lmodule *m) {
 struct network_v2_interface_descriptor id;

 memset (&id, 0, sizeof (struct network_v2_interface_descriptor));
 id.module = m;

 m->functions = einit_module_network_v2_module_functions;

 m->enable = einit_module_network_v2_module_enable;
 m->disable = einit_module_network_v2_module_disable;
 m->custom = (int (*)(void *, char *, struct einit_event *ev))einit_module_network_v2_module_custom;

 m->param = m;

 emutex_lock (&einit_module_network_v2_interfaces_mutex);
 einit_module_network_v2_interfaces = streeadd (einit_module_network_v2_interfaces, (m->module->rid + 13), &id, sizeof (struct network_v2_interface_descriptor), NULL);
 emutex_unlock (&einit_module_network_v2_interfaces_mutex);

 einit_module_network_v2_emit_event (einit_network_interface_configure, m, (struct smodule *)m->module, (m->module->rid + 13), interface_nop, NULL);

 return 0;
}

char *einit_module_network_v2_last_auto = NULL;

char **einit_module_network_v2_add_configured_interfaces (char **interfaces) {
 struct stree *interface_nodes = cfg_prefix(INTERFACES_PREFIX "-");

 if (interface_nodes) {
  struct stree *cur = streelinear_prepare(interface_nodes);

  while (cur) {
   struct cfgnode *n = cur->value;
   if (!n->arbattrs) {
    if (!inset ((const void **)interfaces, cur->key + sizeof (INTERFACES_PREFIX), SET_TYPE_STRING)) {
     interfaces = (char **)setadd ((void **)interfaces, cur->key + sizeof (INTERFACES_PREFIX), SET_TYPE_STRING);
    }
   }

   cur = streenext (cur);
  }

  streefree (interface_nodes);
 }

 return interfaces;
}

int einit_module_network_v2_scanmodules (struct lmodule *modchain) {
 char **interfaces = function_call_by_name_multi (char **, "network-list-interfaces", 1, (const char **)bsd_network_suffixes, 0);
 char **automatic = NULL, **immediate = NULL;

 if (interfaces) {
/* doing this check now ensures that we have the support module around */
  interfaces = einit_module_network_v2_add_configured_interfaces(interfaces);

  int i = 0;
  struct stree *st;
  for (; interfaces[i]; i++) {
   struct lmodule *lm = NULL;

   emutex_lock (&einit_module_network_v2_interfaces_mutex);
   if (einit_module_network_v2_interfaces && (st = streefind (einit_module_network_v2_interfaces, interfaces[i], tree_find_first))) {
    struct network_v2_interface_descriptor *id = st->value;
    emutex_unlock (&einit_module_network_v2_interfaces_mutex);

    lm = id->module;

    einit_module_network_v2_emit_event (einit_network_interface_update, lm, (struct smodule *)lm->module, interfaces[i], interface_nop, NULL);

    mod_update (lm);
   } else {
    emutex_unlock (&einit_module_network_v2_interfaces_mutex);

    char buffer[BUFFERSIZE];
    struct smodule *sm = emalloc (sizeof (struct smodule));
    memset (sm, 0, sizeof (struct smodule));

    esprintf (buffer, BUFFERSIZE, "interface-v2-%s", interfaces[i]);
    sm->rid = estrdup (buffer);

    esprintf (buffer, BUFFERSIZE, "Network Interface (%s)", interfaces[i]);
    sm->name = estrdup (buffer);

    sm->eiversion = EINIT_VERSION;
    sm->eibuild = BUILDNUMBER;
    sm->version = 1;
    sm->mode = einit_module_generic;

    esprintf (buffer, BUFFERSIZE, "net-%s", interfaces[i]);
    sm->si.provides = (char **)setadd ((void **)NULL, buffer, SET_TYPE_STRING);

    sm->configure = einit_module_network_v2_module_configure;

    einit_module_network_v2_emit_event (einit_network_interface_construct, NULL, sm, interfaces[i], interface_nop, NULL);

    lm = mod_add (NULL, sm);
   }

   if (lm) {
    struct cfgnode *cn;

    if (!(coremode & (einit_mode_sandbox | einit_mode_ipconly))) {
     if ((cn = einit_module_network_v2_get_option (interfaces[i], "immediate")) && cn->flag &&
         (!lm || !(lm->status & status_enabled))) {
      char buffer[BUFFERSIZE];

//      fprintf (stderr, "bring this up immediately: %s\n", interfaces[i]);

      esprintf (buffer, BUFFERSIZE, "net-%s", interfaces[i]);

      immediate = (char **)setadd ((void **)immediate, buffer, SET_TYPE_STRING);
     }
    }

    if ((cn = einit_module_network_v2_get_option (interfaces[i], "automatic")) && cn->flag) {
     char buffer[BUFFERSIZE];

//     fprintf (stderr, "bring this up automatically: %s\n", interfaces[i]);

     esprintf (buffer, BUFFERSIZE, "net-%s", interfaces[i]);

     automatic = (char **)setadd ((void **)automatic, buffer, SET_TYPE_STRING);
    }
   }
  }

  efree (interfaces);
 }

 if (automatic) {
  char *au = automatic ? set2str (':', (const char **)automatic) : estrdup ("none");
  char doadd = 1;

  if (au) {
   struct cfgnode *n = cfg_getnode ("services-alias-network", NULL);
   if (n && n->arbattrs) {
    int i = 0;
    for (; n->arbattrs[i]; i+=2) {
     if (strmatch (n->arbattrs[i], "group")) {
      if (strmatch (au, n->arbattrs[i+1])) {
       doadd = 0;
      } else if (einit_module_network_v2_last_auto && strmatch (einit_module_network_v2_last_auto, n->arbattrs[i+1])) {
       doadd = 1;
      } else {
       doadd = 0;
      }
      break;
     }
    }
   }

   if (doadd) {
//    fprintf (stderr, "adding group...\n");
    struct cfgnode newnode;
    memset (&newnode, 0, sizeof (struct cfgnode));

    newnode.id = estrdup("services-alias-network");
    newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, "group", SET_TYPE_STRING);
    newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, au, SET_TYPE_STRING);
    newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, "seq", SET_TYPE_STRING);
    newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, "all", SET_TYPE_STRING);

    cfg_addnode (&newnode);
   }

   if (einit_module_network_v2_last_auto)
    efree (einit_module_network_v2_last_auto);

   einit_module_network_v2_last_auto = au;
  }

  efree (automatic);
 }

 if (immediate) {
  struct einit_event eml = evstaticinit(einit_core_manipulate_services);
  eml.stringset = immediate;
  eml.task = einit_module_enable;

  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy(eml);
 }

 return 1;
}

/* ********************** dhcp code *****************************/

int einit_module_network_v2_do_dhcp (struct network_event_data *d, char *client, char *interface) {
 struct cfgnode *node = NULL;
 fbprintf (d->feedback, "trying dhcp client: %s", client);
 int rv = status_failed;

 while ((node = cfg_findnode ("subsystem-network-dhcp-client", 0, node))) {
  if (node->idattr && strmatch (node->idattr, client)) {
   if (node->arbattrs) {
    char *command = NULL;
    char **need_binaries = NULL;
    char **environment = straddtoenviron (NULL, "interface", interface);
    int i = 0;

    for (; node->arbattrs[i]; i+=2) {
     if (strmatch (node->arbattrs[i], "need-binaries")) {
      need_binaries = str2set (':', node->arbattrs[i+1]);
     } else if ((d->action == interface_up) && strmatch (node->arbattrs[i], "up")) {
      command = node->arbattrs[i+1];
     } else if ((d->action == interface_down) && strmatch (node->arbattrs[i], "down")) {
      command = node->arbattrs[i+1];
     }
    }

    if (command) {
     if (need_binaries) {
      for (i = 0; need_binaries[i]; i++) {
       char **w = which (need_binaries[i]);

       if (!w) {
        efree (need_binaries);
        efree (environment);

        fbprintf (d->feedback, "dhcp client not available: %s", client);
        return status_failed;
       } else {
        efree (w);
       }
      }

      efree (need_binaries);
     }

     rv = pexec (command, NULL, 0, 0, NULL, NULL, environment, d->feedback);
     if (rv == status_ok) {
      fbprintf (d->feedback, "dhcp client OK: %s", client);
     } else if (rv == status_failed) {
      fbprintf (d->feedback, "dhcp client failed: %s", client);
     }
    }

    efree (environment);
   }

   break;
  }
 }

 return rv;
}

void einit_module_network_v2_address_automatic (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 if (d->action == interface_up) {
  struct stree *st = d->functions->get_all_addresses (ev->string);
  if (st) {
   struct stree *cur = streefind (st, "ipv4", tree_find_first);
   char do_dhcp = 0;

   while (cur) {
    if (cur->value) {
     char **v = cur->value;
     int i = 0;

     for (; v[i]; i+=2) {
      if (strmatch (v[i], "address")) {
       if (strmatch (v[i+1], "dhcp")) {
        do_dhcp = 1;
       }
      }
     }
    }

    cur = streefind (cur, "ipv4", tree_find_next);
   }

   if (do_dhcp) {
    struct cfgnode *node = d->functions->get_option(ev->string, "dhcp-client");
    if (node && node->svalue) {
     char **v = str2set (':', node->svalue);
     int i = 0;
     char ok = 0;

     for (; v[i]; i++) {
      if (einit_module_network_v2_do_dhcp(d, v[i], ev->string) == status_ok) {
       ok = 1;

       emutex_lock (&einit_module_network_v2_interfaces_mutex);
       struct stree *tmpst = NULL;
       if (einit_module_network_v2_interfaces) tmpst = streefind (einit_module_network_v2_interfaces, ev->string, tree_find_first);
       if (tmpst) {
        struct network_v2_interface_descriptor *id = tmpst->value;
        if (id)
         id->dhcp_client = estrdup (v[i]);
       }
       emutex_unlock (&einit_module_network_v2_interfaces_mutex);

       break;
      }
     }

     efree(v);

     if (!ok) {
      d->status = status_failed;
     }
    } else {
     fbprintf (d->feedback, "dhcp requested, but no clients to try");

     d->status = status_failed;
    }
   }

   streefree (st);
  }
 } else if (d->action == interface_down) {
  char *client = NULL;
  struct stree *st = NULL;

  emutex_lock (&einit_module_network_v2_interfaces_mutex);
  if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, ev->string, tree_find_first);
  if (st) {
   struct network_v2_interface_descriptor *id = st->value;
   if (id && id->dhcp_client) {
    client = estrdup(id->dhcp_client);
   }
  }
  emutex_unlock (&einit_module_network_v2_interfaces_mutex);

  if (client) {
   if (einit_module_network_v2_do_dhcp(d, client, ev->string) == status_ok) {
    emutex_lock (&einit_module_network_v2_interfaces_mutex);
    if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, ev->string, tree_find_first);
    if (st) {
     struct network_v2_interface_descriptor *id = st->value;
     if (id && id->dhcp_client) {
      efree (id->dhcp_client);
      id->dhcp_client = NULL;
     }
    }
    emutex_unlock (&einit_module_network_v2_interfaces_mutex);
   }

   efree (client);
  }
 }
}

char **einit_module_network_v2_add_fs (char **xt, char *s) {
 if (s) {
  char **tmp = s[0] == '/' ? str2set ('/', s+1) : str2set ('/', s);
  uint32_t r = 0;

  for (r = 0; tmp[r]; r++);
  for (r--; tmp[r] && r > 0; r--) {
   tmp[r] = 0;
   char *comb = set2str ('-', (const char **)tmp);

   if (!inset ((const void **)xt, comb, SET_TYPE_STRING)) {
    xt = (char **)setadd ((void **)xt, (void *)comb, SET_TYPE_STRING);
   }

   efree (comb);
  }

  if (tmp) {
   efree (tmp);
  }
 }

 return xt;
}

char *einit_module_network_v2_generate_defer_fs (char **tmpxt) {
 char *tmp = NULL;

 char *tmpx = NULL;
 tmp = emalloc (BUFFERSIZE);

// tmpxt = (char **)setadd ((void **)tmpxt, (void *)"root", SET_TYPE_STRING);

 if (tmpxt) {
  tmpx = set2str ('|', (const char **)tmpxt);
 }

 if (tmpx) {
  esprintf (tmp, BUFFERSIZE, "^fs-(%s)$", tmpx);
  efree (tmpx);
 }

 efree (tmpxt);

 return tmp;
}


void einit_module_network_v2_interface_construct (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 struct stree *st = d->functions->get_all_addresses (ev->string);
 if (st) {
  struct stree *cur = streefind (st, "ipv4", tree_find_first);
  char do_dhcp = 0;

  while (cur) {
   if (cur->value) {
    char **v = cur->value;
    int i = 0;

    for (; v[i]; i+=2) {
     if (strmatch (v[i], "address")) {
      if (strmatch (v[i+1], "dhcp")) {
       do_dhcp = 1;
      }
     }
    }
   }

   cur = streefind (cur, "ipv4", tree_find_next);
  }

  if (do_dhcp) {
   struct cfgnode *node = d->functions->get_option(ev->string, "dhcp-client");
   if (node && node->svalue) {
    char **v = str2set (':', node->svalue);
    int i = 0;

    for (; v[i]; i++) {
     struct cfgnode *node = NULL;

     while ((node = cfg_findnode ("subsystem-network-dhcp-client", 0, node))) {
      if (node->idattr && strmatch (node->idattr, v[i])) {
       if (node->arbattrs) {
        char *pidfile = NULL;
        char **vars = (char **)setadd (NULL, "interface", SET_TYPE_STRING);
        vars = (char **)setadd ((void **)vars, ev->string, SET_TYPE_STRING);

        int y = 0;

        for (; node->arbattrs[y]; y+=2) {
         if (strmatch (node->arbattrs[y], "pid")) {
          pidfile = apply_variables (node->arbattrs[y+1], (const char **)vars);
         }
        }

        if (pidfile) {
         char **fs = einit_module_network_v2_add_fs(NULL, pidfile);

         if (fs) {
          char *a = einit_module_network_v2_generate_defer_fs(fs);

          if (a) {
           if (!inset ((const void **)d->static_descriptor->si.after, a, SET_TYPE_STRING)) {

            d->static_descriptor->si.after =
             (char **)setadd ((void **)d->static_descriptor->si.after, a, SET_TYPE_STRING);
           }

           efree (a);
          }
         }

         efree (pidfile);
        }

        efree (vars);
       }
      }
     }
    }
   }
  }

  streefree (st);
 }
}

/* **************** end * dhcp code *****************************/

int einit_module_network_v2_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_network_address_automatic, einit_module_network_v2_address_automatic);
 event_ignore (einit_network_interface_construct, einit_module_network_v2_interface_construct);
 event_ignore (einit_network_interface_update, einit_module_network_v2_interface_construct);

 return 0;
}

int einit_module_network_v2_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 pa->scanmodules = einit_module_network_v2_scanmodules;
 pa->cleanup = einit_module_network_v2_cleanup;

 event_listen (einit_network_address_automatic, einit_module_network_v2_address_automatic);
 event_listen (einit_network_interface_construct, einit_module_network_v2_interface_construct);
 event_listen (einit_network_interface_update, einit_module_network_v2_interface_construct);

 return 0;
}
