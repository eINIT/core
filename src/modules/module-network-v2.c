/*
 *  module-network-v2.c
 *  einit
 *
 *  Created on 12/09/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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
 .mode      = einit_module,
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

#if defined(__FreeBSD) || defined(__APPLE__)
char *bsd_network_suffixes[] = { "bsd", "generic", NULL };
#elif defined(__linux__)
char *bsd_network_suffixes[] = { "linux", "generic", NULL };
#else
char *bsd_network_suffixes[] = { "generic", NULL };
#endif

struct stree *einit_module_network_v2_interfaces = NULL;
char *einit_module_network_v2_module_functions[] = { "zap", "up", "down", "refresh" };

struct network_v2_interface_descriptor {
 enum interface_flags status;
 struct lmodule *module;
 struct lmodule *carrier_module;
 char *dhcp_client;
};

int einit_module_network_v2_emit_event (enum einit_event_code type, struct lmodule *module, struct smodule *sd, char *interface, enum interface_action action, struct einit_event *feedback);

#define INTERFACES_PREFIX "configuration-network-interfaces"
#define INTERFACE_DEFAULTS_PREFIX "subsystem-network-interface-defaults"
#define GLOBAL_DEFAULTS_PREFIX "configuration-network"

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

struct cfgnode *einit_module_network_v2_get_option_global (char *option) {
 char buffer[BUFFERSIZE];

 esprintf (buffer, BUFFERSIZE, GLOBAL_DEFAULTS_PREFIX "-%s", option);

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
 else if ((node = einit_module_network_v2_get_option_default (interface, option)))
  return node;
 else
  return einit_module_network_v2_get_option_global (option);
}

struct cfgnode **einit_module_network_v2_get_multiple_options (char *interface, char *option) {
 char buffer[BUFFERSIZE];
 struct cfgnode *node = NULL;
 struct cfgnode **rv = NULL;

 esprintf (buffer, BUFFERSIZE, INTERFACES_PREFIX "-%s-%s", interface, option);

 while ((node = cfg_findnode (buffer, 0, node))) {
  rv = (struct cfgnode **)set_noa_add ((void **)rv, node);
 }

 if (rv)
  return rv;
 else {
  if ((node = einit_module_network_v2_get_option_default (interface, option))) {
   rv = (struct cfgnode **)set_noa_add ((void **)rv, node);
  } else if ((node = einit_module_network_v2_get_option_global (option))) {
   rv = (struct cfgnode **)set_noa_add ((void **)rv, node);
  }

  return rv;
 }
}

struct stree *einit_module_network_v2_get_all_addresses (char *interface) {
 char buffer[BUFFERSIZE];
 struct stree *st;
 struct stree *rv = NULL;

 esprintf (buffer, BUFFERSIZE, INTERFACES_PREFIX "-%s-address-", interface);

 st = cfg_prefix (buffer);
 if (st) {
  struct stree *cur = streelinear_prepare (st);
  ssize_t prefixlen = strlen (buffer);

  while (cur) {
   struct cfgnode *n = cur->value;
   if (n->arbattrs) {
    char **narb = set_str_dup_stable (n->arbattrs);

    rv = streeadd (rv, (cur->key + prefixlen), narb, tree_value_noalloc, narb);
   }

   cur = streenext (cur);
  }

  streefree (st);
 } else {
  struct cfgnode *n;
  if ((n = einit_module_network_v2_get_option_default(interface, "address-ipv4")) && n->arbattrs) {
   char **narb = set_str_dup_stable (n->arbattrs);
   rv = streeadd (rv, "ipv4", narb, tree_value_noalloc, narb);
  }

  if ((n = einit_module_network_v2_get_option_default(interface, "address-ipv6")) && n->arbattrs) {
   char **narb = set_str_dup_stable (n->arbattrs);
   rv = streeadd (rv, "ipv6", narb, tree_value_noalloc, narb);
  }
 }

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

 if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, interface, tree_find_first);
 if (st) {
  struct network_v2_interface_descriptor *id = st->value;
  if (id)
   d.flags = id->status;
 }

 ev.string = interface;
 ev.para = &d;

 event_emit (&ev, 0);

 evstaticdestroy (&ev);

 if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, interface, tree_find_first);
 if (st) {
  struct network_v2_interface_descriptor *id = st->value;
  if (id)
   id->status = d.flags;
 }

 return d.status;
}

int einit_module_network_v2_module_custom (struct lmodule *m, char *task_s, struct einit_event *status) {
 enum interface_action task = interface_nop;

 if (strmatch (task_s, "up")) task = interface_up;
 else if (strmatch (task_s, "down")) task = interface_down;
 else if (strmatch (task_s, "refresh")) task = interface_refresh_ip;

 switch (task) {
  case interface_up:
   if (einit_module_network_v2_emit_event (einit_network_address_automatic, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;
   if (einit_module_network_v2_emit_event (einit_network_address_static, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;

   einit_module_network_v2_emit_event (einit_network_interface_done, m, (struct smodule *)m->module, (m->module->rid + 13), task, status);
   return status_ok;
   break;

  case interface_down:
   if (einit_module_network_v2_emit_event (einit_network_address_static, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;
   if (einit_module_network_v2_emit_event (einit_network_address_automatic, m, (struct smodule *)m->module, (m->module->rid + 13), task, status) == status_failed) goto cancel_fail;

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
// m->functions = einit_module_network_v2_module_functions;

 m->enable = einit_module_network_v2_module_enable;
 m->disable = einit_module_network_v2_module_disable;
 m->custom = (int (*)(void *, char *, struct einit_event *ev))einit_module_network_v2_module_custom;

 m->param = m;

 struct stree *st = NULL;
 if (einit_module_network_v2_interfaces) {
  st = streefind (einit_module_network_v2_interfaces, (m->module->rid + 13), tree_find_first);
 }

 if (st) {
  struct network_v2_interface_descriptor *id = st->value;
  id->module = m;
 } else {
  struct network_v2_interface_descriptor id;

  memset (&id, 0, sizeof (struct network_v2_interface_descriptor));
  id.module = m;

  einit_module_network_v2_interfaces = streeadd (einit_module_network_v2_interfaces, (m->module->rid + 13), &id, sizeof (struct network_v2_interface_descriptor), NULL);
 }

 einit_module_network_v2_emit_event (einit_network_interface_configure, m, (struct smodule *)m->module, (m->module->rid + 13), interface_nop, NULL);

 return 0;
}

int einit_module_network_v2_carrier_module_custom (struct lmodule *m, char *task_s, struct einit_event *status) {
 enum interface_action task = interface_nop;

 if (strmatch (task_s, "up")) task = interface_up;
 else if (strmatch (task_s, "down")) task = interface_down;
 else if (strmatch (task_s, "refresh")) task = interface_refresh_ip;

 if (task != interface_nop) {
  if (einit_module_network_v2_emit_event (einit_network_interface_prepare, m, (struct smodule *)m->module, (m->module->rid + 18), task, status) == status_failed) goto cancel_fail;
 } else goto cancel_fail;

 switch (task) {
  case interface_up:
   if (einit_module_network_v2_emit_event (einit_network_verify_carrier, m, (struct smodule *)m->module, (m->module->rid + 18), task, status) == status_failed) goto cancel_fail;

   return status_ok;
   break;

  case interface_down:
   if (einit_module_network_v2_emit_event (einit_network_kill_carrier, m, (struct smodule *)m->module, (m->module->rid + 18), task, status) == status_failed) goto cancel_fail;

   einit_module_network_v2_emit_event (einit_network_interface_done, m, (struct smodule *)m->module, (m->module->rid + 18), task, status);
   return status_ok;
   break;

  case interface_refresh_ip:
   if (einit_module_network_v2_emit_event (einit_network_address_automatic, m, (struct smodule *)m->module, (m->module->rid + 18), task, status) == status_failed) goto cancel_fail;
   if (einit_module_network_v2_emit_event (einit_network_address_static, m, (struct smodule *)m->module, (m->module->rid + 18), task, status) == status_failed) goto cancel_fail;

  case interface_nop:
   break;
 }

 cancel_fail:
   einit_module_network_v2_emit_event (einit_network_interface_cancel, m, (struct smodule *)m->module, (m->module->rid + 18), task, status);

 return status_failed;
}

int einit_module_network_v2_carrier_module_enable (void *p, struct einit_event *status) {
 return einit_module_network_v2_carrier_module_custom(p, "up", status);
}

int einit_module_network_v2_carrier_module_disable (void *p, struct einit_event *status) {
 return einit_module_network_v2_carrier_module_custom(p, "down", status);
}

int einit_module_network_v2_carrier_module_configure (struct lmodule *m) {
// m->functions = einit_module_network_v2_module_functions;

 m->enable = einit_module_network_v2_carrier_module_enable;
 m->disable = einit_module_network_v2_carrier_module_disable;
 m->custom = (int (*)(void *, char *, struct einit_event *ev))einit_module_network_v2_carrier_module_custom;

 m->param = m;

 struct stree *st = NULL;
 if (einit_module_network_v2_interfaces) {
  st = streefind (einit_module_network_v2_interfaces, (m->module->rid + 18), tree_find_first);
 }

 if (st) {
  struct network_v2_interface_descriptor *id = st->value;
  id->carrier_module = m;
 } else {
  struct network_v2_interface_descriptor id;

  memset (&id, 0, sizeof (struct network_v2_interface_descriptor));
  id.carrier_module = m;

  einit_module_network_v2_interfaces = streeadd (einit_module_network_v2_interfaces, (m->module->rid + 18), &id, sizeof (struct network_v2_interface_descriptor), NULL);
 }

 einit_module_network_v2_emit_event (einit_network_interface_configure, m, (struct smodule *)m->module, (m->module->rid + 18), interface_nop, NULL);

 return 0;
}

char *einit_module_network_v2_last_auto = NULL;

char **einit_module_network_v2_add_configured_interfaces (char **interfaces) {
 struct stree *interface_nodes = cfg_prefix(INTERFACES_PREFIX "-");

 if (interface_nodes) {
  struct stree *cur = streelinear_prepare(interface_nodes);

  while (cur) {
   struct cfgnode *n = cur->value;
   if (!n->arbattrs && !strchr (cur->key + sizeof (INTERFACES_PREFIX), '-')) {
/* only accept interfaces without dashes in them here... */
    if (!inset ((const void **)interfaces, cur->key + sizeof (INTERFACES_PREFIX), SET_TYPE_STRING)) {
     interfaces = set_str_add (interfaces, cur->key + sizeof (INTERFACES_PREFIX));
    }
   }

   cur = streenext (cur);
  }

  streefree (interface_nodes);
 }

 return interfaces;
}

void *einit_module_network_v2_scanmodules_enable_immediate (struct lmodule **lm) {
 int i = 0;
 for (; lm[i]; i++) {
  mod(einit_module_enable, lm[i], NULL);
 }
 efree (lm);
 return NULL;
}

void einit_module_network_v2_scanmodules (struct einit_event *ev) {
 char **interfaces = function_call_by_name_multi (char **, "network-list-interfaces", 1, (const char **)bsd_network_suffixes, 0);
 char **automatic = NULL;
 struct lmodule **immediate = NULL;

 if (interfaces) {
/* doing this check now ensures that we have the support module around */
  interfaces = einit_module_network_v2_add_configured_interfaces(interfaces);

  int i = 0;
  struct stree *st;
  for (; interfaces[i]; i++) {
   struct lmodule *lm = NULL;
   struct lmodule *cm = NULL;

   if (einit_module_network_v2_interfaces && (st = streefind (einit_module_network_v2_interfaces, interfaces[i], tree_find_first))) {
    struct network_v2_interface_descriptor *id = st->value;

    lm = id->module;
    cm = id->carrier_module;

    if (lm) {
     einit_module_network_v2_emit_event (einit_network_interface_update, lm, (struct smodule *)lm->module, interfaces[i], interface_nop, NULL);
     mod_update (lm);
    }
    if (cm) {
     einit_module_network_v2_emit_event (einit_network_interface_update, cm, (struct smodule *)cm->module, interfaces[i], interface_nop, NULL);
     mod_update (cm);
    }

    fflush (stderr);
   } else {
    char buffer[BUFFERSIZE];
    struct smodule *sm = NULL;

/* add carrier module */
    if ((sm = emalloc (sizeof (struct smodule)))) {
     memset (sm, 0, sizeof (struct smodule));

     esprintf (buffer, BUFFERSIZE, "interface-carrier-%s", interfaces[i]);
     sm->rid = (char *)str_stabilise (buffer);

     esprintf (buffer, BUFFERSIZE, "Network Interface Carrier (%s)", interfaces[i]);
     sm->name = (char *)str_stabilise (buffer);

     sm->eiversion = EINIT_VERSION;
     sm->eibuild = BUILDNUMBER;
     sm->version = 1;
     sm->mode = einit_module | einit_module_fork_actions;

     esprintf (buffer, BUFFERSIZE, "carrier-%s", interfaces[i]);
     sm->si.provides = set_str_add (NULL, buffer);

     sm->configure = einit_module_network_v2_carrier_module_configure;

     einit_module_network_v2_emit_event (einit_network_interface_construct, NULL, sm, interfaces[i], interface_nop, NULL);

     lm = mod_add (NULL, sm);
    }
/* add network module */
    if ((sm = emalloc (sizeof (struct smodule)))) {
     memset (sm, 0, sizeof (struct smodule));

     esprintf (buffer, BUFFERSIZE, "interface-v2-%s", interfaces[i]);
     sm->rid = (char *)str_stabilise (buffer);

     esprintf (buffer, BUFFERSIZE, "Network Interface (%s)", interfaces[i]);
     sm->name = (char *)str_stabilise (buffer);

     sm->eiversion = EINIT_VERSION;
     sm->eibuild = BUILDNUMBER;
     sm->version = 1;
     sm->mode = einit_module | einit_module_fork_actions;

     esprintf (buffer, BUFFERSIZE, "net-%s", interfaces[i]);
     sm->si.provides = set_str_add (NULL, buffer);
     esprintf (buffer, BUFFERSIZE, "carrier-%s", interfaces[i]);
     sm->si.requires = set_str_add (NULL, buffer);

     sm->configure = einit_module_network_v2_module_configure;

     einit_module_network_v2_emit_event (einit_network_interface_construct, NULL, sm, interfaces[i], interface_nop, NULL);

     lm = mod_add (NULL, sm);
    }
/* done adding modules */
   }

   if (lm) {
    struct cfgnode *cn;

    if (!(coremode & (einit_mode_sandbox | einit_mode_ipconly))) {
     if ((cn = einit_module_network_v2_get_option (interfaces[i], "immediate")) && cn->flag &&
         lm && !(lm->status & (status_working | status_enabled))) {
      immediate = (struct lmodule **)set_noa_add ((void **)immediate, cm);
      immediate = (struct lmodule **)set_noa_add ((void **)immediate, lm);
     }
    }

    if ((cn = einit_module_network_v2_get_option (interfaces[i], "automatic")) && cn->flag) {
     char buffer[BUFFERSIZE];

     esprintf (buffer, BUFFERSIZE, "net-%s", interfaces[i]);

     automatic = set_str_add (automatic, buffer);
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

    newnode.id = (char *)str_stabilise("services-alias-network");
    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, "group");
    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, au);
    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, "seq");
    newnode.arbattrs = set_str_add_stable (newnode.arbattrs, "all");

    cfg_addnode (&newnode);
   }

   if (einit_module_network_v2_last_auto)
    efree (einit_module_network_v2_last_auto);

   einit_module_network_v2_last_auto = au;
  }

  efree (automatic);
 }

 if (immediate) {
  einit_module_network_v2_scanmodules_enable_immediate (immediate);
 }

 return;
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
    char *pidfile = NULL;
//    char **environment = straddtoenviron (NULL, "interface", interface);
    char **vars = set_str_add (NULL, "interface");
    vars = set_str_add (vars, interface);
    int i = 0;

    for (; node->arbattrs[i]; i+=2) {
     if (strmatch (node->arbattrs[i], "need-binaries")) {
      need_binaries = str2set (':', node->arbattrs[i+1]);
     } else if ((d->action == interface_up) && strmatch (node->arbattrs[i], "up")) {
//      command = node->arbattrs[i+1];
      command = apply_variables (node->arbattrs[i+1], (const char **)vars);
     } else if ((d->action == interface_down) && strmatch (node->arbattrs[i], "down")) {
//      command = node->arbattrs[i+1];
      command = apply_variables (node->arbattrs[i+1], (const char **)vars);
     } else if (strmatch (node->arbattrs[i], "pid")) {
//      command = node->arbattrs[i+1];
      pidfile = apply_variables (node->arbattrs[i+1], (const char **)vars);
     }
    }

    if (command) {
     if (need_binaries) {
      for (i = 0; need_binaries[i]; i++) {
       char **w = which (need_binaries[i]);

       if (!w) {
        efree (need_binaries);
        efree (vars);

        fbprintf (d->feedback, "dhcp client not available: %s", client);

        if (pidfile) efree (pidfile);

        return status_failed;
       } else {
        efree (w);
       }
      }

      efree (need_binaries);
     }

     if ((d->action == interface_up) && pidfile) unlink (pidfile);

     rv = pexec (command, NULL, 0, 0, NULL, NULL, NULL, d->feedback);
     if (rv == status_ok) {
      fbprintf (d->feedback, "dhcp client OK: %s", client);

      if ((d->action == interface_down) && pidfile) unlink (pidfile);
     } else if (rv == status_failed) {
      fbprintf (d->feedback, "dhcp client failed: %s", client);
     }
    }

    if (pidfile) efree (pidfile);

    efree (vars);
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

       struct stree *tmpst = NULL;
       if (einit_module_network_v2_interfaces) tmpst = streefind (einit_module_network_v2_interfaces, ev->string, tree_find_first);
       if (tmpst) {
        struct network_v2_interface_descriptor *id = tmpst->value;
        if (id)
         id->dhcp_client = (char *)str_stabilise (v[i]);
       }

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

  if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, ev->string, tree_find_first);
  if (st) {
   struct network_v2_interface_descriptor *id = st->value;
   if (id && id->dhcp_client) {
    client = (char *)str_stabilise(id->dhcp_client);
   }
  }

  if (client) {
   if (einit_module_network_v2_do_dhcp(d, client, ev->string) == status_ok) {
    if (einit_module_network_v2_interfaces) st = streefind (einit_module_network_v2_interfaces, ev->string, tree_find_first);
    if (st) {
     struct network_v2_interface_descriptor *id = st->value;
     if (id && id->dhcp_client) {
      id->dhcp_client = NULL;
     }
    }
   }
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
    xt = set_str_add (xt, (void *)comb);
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

 if (tmpxt) {
  tmpx = set2str ('|', (const char **)tmpxt);
 }

 if (tmpx) {
  esprintf (tmp, BUFFERSIZE, "^fs-(root|%s)$", tmpx);
  efree (tmpx);
 }

 efree (tmpxt);

 return tmp;
}


void einit_module_network_v2_interface_construct (struct einit_event *ev) {
 struct network_event_data *d = ev->para;

 if (strprefix (d->static_descriptor->rid, "interface-v2-")) {
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
         char **vars = set_str_add (NULL, "interface");
         vars = set_str_add (vars, ev->string);

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
               set_str_add (d->static_descriptor->si.after, a);
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
}

/* **************** end * dhcp code *****************************/

int einit_module_network_v2_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 event_listen (einit_core_update_modules, einit_module_network_v2_scanmodules);

 event_listen (einit_network_address_automatic, einit_module_network_v2_address_automatic);
 event_listen (einit_network_interface_construct, einit_module_network_v2_interface_construct);
 event_listen (einit_network_interface_update, einit_module_network_v2_interface_construct);

 einit_module_network_v2_scanmodules(NULL);

 return 0;
}
