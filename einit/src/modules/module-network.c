/*
 *  network.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/04/2006.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <expat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/event.h>
#include <dirent.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>
#include <einit-modules/exec.h>

#define ECXE_MASTERTAG 0x00000001
#define IF_OK          0x1

enum interface_status {
 is_down       = 0x1000,
 is_ip_blocked = 0x0010,
 is_ifctl_up   = 0x0001,
 is_ip_up      = 0x0002
};

enum interface_action_command {
 iac_need_all,
 iac_need_any,
 iac_need_this
};

enum interface_template_action_type {
 ita_pexec,
 ita_daemon,
 ita_function
};

struct interface_template_action {
 enum interface_template_action_type type;

 union {
  char *pexec_function;
  struct dexecinfo *daemon_dxdata;
 };
};

struct interface_template_item {
 struct stree *action;
 char **variables;
 char **environment;
 char *pidfile;

 char *name;
};

struct interface_descriptor {
 char *interface_name;
 enum interface_status status;
 struct interface_template_item **controller;
 struct interface_template_item **ip_manager;
 struct interface_template_item **macchanger;

#if 0
 uint32_t ci, pi;
#endif
 char **variables;
 char *kernel_module;

// struct stree *bridge_interfaces;
 char **bridge;

 struct cfgnode *interface;
};

int network_scanmodules (struct lmodule *);
int network_interface_enable (struct interface_descriptor *, struct einit_event *);
int network_interface_disable (struct interface_descriptor *, struct einit_event *);
int network_interface_custom (struct interface_descriptor *, char *, struct einit_event *);
int network_interface_cleanup (struct lmodule *);
int network_interface_configure (struct lmodule *);
int network_cleanup (struct lmodule *);
int network_configure (struct lmodule *);

int create_bridge (struct interface_descriptor *, struct einit_event *);
int bridge_enable (struct interface_descriptor *, struct einit_event *);
int flush_ip (struct interface_descriptor *, struct einit_event *);

struct interface_descriptor *network_import_interface_descriptor (struct lmodule *);
void network_free_interface_descriptor (struct interface_descriptor *);
struct interface_template_item **network_import_templates (char *, char *, struct interface_descriptor *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_network_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Network Interface Configuration",
 .rid       = "einit-network",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = network_configure
};

module_register(einit_network_self);

#endif

#if 0
void network_einit_event_handler (struct einit_event *ev) {
 if (ev->type == einit_core_update_configuration) {
  if (configuration_network_parse_and_add_nodes()) {
   ev->chain_type = einit_core_configuration_update;
  }
 }
}
#endif

struct stree *network_modules = NULL;

void network_reorder_interface_template_item (struct interface_template_item **str, char *name) {
 if (name) {
  struct interface_template_item *start = str[0];
  struct interface_template_item *current = str[0];

  while (!strmatch (current->name, name)) {
   current = str[0];

   if (str[1]) {
    ssize_t rx = 1;

    for (; str[rx]; rx++) {
     str[rx-1] = str[rx];
    }

    str[rx-1] = current;
    current = str[0];
   }

   if (current == start) return;
  }
 }
}

int network_scanmodules (struct lmodule *mainlist) {
 struct stree *network_nodes = cfg_prefix("configuration-network-interfaces-");

 if (network_nodes) {
  struct stree *cur = network_nodes;

  while (cur) {
   if (cur->value) {
    struct cfgnode *nn = cur->value;

    if (nn->arbattrs) {
     char *interfacename = cur->key+33; /* 33 = strlen("configuration-network-interfaces-") */
     struct smodule *newmodule = emalloc (sizeof (struct smodule));
     char tmp[BUFFERSIZE], **req = NULL, **after = NULL, **before = NULL;
     struct lmodule *lm;
     struct cfgnode *node = cur->value;
     uint32_t y = 0;

     req = (char **)setadd ((void **)req, (void *)"mount-critical", SET_TYPE_STRING);

     for (; node->arbattrs[y]; y+=2) {
      if (strmatch (node->arbattrs[y], "bridge")) {
       req = (char **)setadd ((void **)req, (void *)"kern-bridge", SET_TYPE_STRING);
      } else if (strmatch (node->arbattrs[y], "kernel-module")) {
       struct cfgnode newnode;

       esprintf (tmp, BUFFERSIZE, "kern-%s", interfacename);
       req = (char **)setadd ((void **)req, tmp, SET_TYPE_STRING);

       memset (&newnode, 0, sizeof(struct cfgnode));

       esprintf (tmp, BUFFERSIZE, "configuration-kernel-modules-%s", interfacename);
       newnode.id = estrdup (tmp);
       newnode.source = self->rid;
       newnode.type = einit_node_regular;

       esprintf (tmp, BUFFERSIZE, "kernel-module-%s", interfacename);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"s", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)nn->arbattrs[y+1], SET_TYPE_STRING);

       newnode.svalue = newnode.arbattrs[3];

       cfg_addnode (&newnode);
      } else if (strmatch (node->arbattrs[y], "after")) {
       after = str2set (':', node->arbattrs[y+1]);
      } else if (strmatch (node->arbattrs[y], "before")) {
       before = str2set (':', node->arbattrs[y+1]);
      }
     }

     memset (newmodule, 0, sizeof (struct smodule));

     lm = mainlist;

     esprintf (tmp, BUFFERSIZE, "interface-%s", interfacename);

     while (lm) {
      if (lm->source && strmatch(lm->source, tmp)) {
       struct smodule *sm = (struct smodule *)lm->module;
       sm->si.requires = req;
       sm->si.after = after;
       sm->si.before = before;

       lm = mod_update (lm);

       emutex_lock (&(lm->mutex));

       if (lm->param) {
        struct interface_descriptor *id = lm->param;
        char *ip_name = id && id->ip_manager ? id->ip_manager[0]->name : NULL;
        char *ifctl_name = id && id->controller ? id->controller[0]->name : NULL;
        char *macchanger_name = id && id->macchanger ? id->macchanger[0]->name : NULL;
        int ifstatus = id ? id->status : is_down;

        network_free_interface_descriptor (lm->param);
        lm->param = network_import_interface_descriptor (lm);

        id = lm->param;
        id->status = ifstatus;

        if (ip_name) {
         network_reorder_interface_template_item (id->ip_manager, ip_name);
        }
        if (macchanger_name) {
         network_reorder_interface_template_item (id->macchanger, macchanger_name);
        }
        if (ifctl_name) {
         network_reorder_interface_template_item (id->controller, ifctl_name);
        }
       }

       emutex_unlock (&(lm->mutex));

       goto do_next;
      }

      lm = lm->next;
     }

     newmodule->configure = network_interface_configure;
     esprintf (tmp, BUFFERSIZE, "interface-%s", interfacename);
     newmodule->rid = estrdup (tmp);
     newmodule->eiversion = EINIT_VERSION;
     newmodule->eibuild = BUILDNUMBER;
     newmodule->version = 1;
     newmodule->mode = einit_module_generic;

     esprintf (tmp, BUFFERSIZE, "Network Interface (%s)", interfacename);
     newmodule->name = estrdup (tmp);

     esprintf (tmp, BUFFERSIZE, "net-%s", interfacename);
     newmodule->si.provides = (char **)setadd ((void **)newmodule->si.provides, (void *)tmp, SET_TYPE_STRING);
     newmodule->si.requires = req;

     newmodule->si.after = after;
     newmodule->si.before = before;

     lm = mod_add (NULL, newmodule);
    }
   }

   do_next:

   cur = streenext (cur);
  }

  streefree (network_nodes);
 }

 return 0;
}

void network_free_interface_descriptor (struct interface_descriptor *id) {
 if (!id) return;

 if (id->variables) free (id->variables);
 if (id->controller) {
  uint32_t i = 0;
  for (; id->controller[i]; i++) {
   if (id->controller[i]->variables) free (id->controller[i]->variables);
   if (id->controller[i]->environment) free (id->controller[i]->environment);
   if (id->controller[i]->action) streefree (id->controller[i]->action);
  }
 }
 if (id->ip_manager) {
  uint32_t i = 0;
  for (; id->ip_manager[i]; i++) {
   if (id->ip_manager[i]->variables) free (id->ip_manager[i]->variables);
   if (id->ip_manager[i]->environment) free (id->ip_manager[i]->environment);
   if (id->ip_manager[i]->action) streefree (id->ip_manager[i]->action);
  }
 }
 if (id->macchanger) {
  uint32_t i = 0;
  for (; id->macchanger[i]; i++) {
   if (id->macchanger[i]->variables) free (id->macchanger[i]->variables);
   if (id->macchanger[i]->environment) free (id->macchanger[i]->environment);
   if (id->macchanger[i]->action) streefree (id->macchanger[i]->action);
  }
 }

 free (id);
}

struct interface_template_item **network_import_templates (char *type, char *list, struct interface_descriptor *id) {
 if (!type || !list || strmatch (list, "none")) return NULL;

 struct interface_template_item **retval = NULL;

 char nodename[BUFFERSIZE];
 char **ll = str2set (':', list);
 uint32_t i = 0;

 if (!ll) return NULL;

 char **if_vars = (char **)setdup ((const void **)id->interface->arbattrs, SET_TYPE_STRING);

 if_vars = (char **)setadd((void **)if_vars, (void *)"interface", SET_TYPE_STRING);
 if_vars = (char **)setadd((void **)if_vars, (void *)id->interface_name, SET_TYPE_STRING);

 esprintf (nodename, BUFFERSIZE, "services-virtual-network-%s", type);

 for (; ll[i]; i++) {
  struct cfgnode *node = NULL;

  while ((node = cfg_findnode (nodename, 0, node))) {
   if (node->idattr && node->arbattrs && strmatch(node->idattr, ll[i])) {
    uint32_t y = 0;
    struct interface_template_item ni;
    memset (&ni, 0, sizeof (struct interface_template_item));

    for (; node->arbattrs[y]; y+=2) {
     char *name = NULL;
     if (((strmatch (node->arbattrs[y], "enable") || strmatch (node->arbattrs[y], "disable")) && (name = node->arbattrs[y])) ||
         ((strstr (node->arbattrs[y], "execute:") == node->arbattrs[y]) && (name = node->arbattrs[y]+8))) {
      char *tmp = apply_variables (node->arbattrs[y+1], (const char **)if_vars);
      struct stree *streeadd (const struct stree *stree, const char *key, const void *value, int32_t vlen, const void *luggage);

      if (tmp) {
       ni.action = streeadd (ni.action, name, tmp, SET_TYPE_STRING, NULL);

       free (tmp);
      }
     } else if (strmatch (node->arbattrs[y], "variables")) {
      char *tmp = apply_variables (node->arbattrs[y+1], (const char **)if_vars);
      ni.variables = str2set (':', tmp);
      free (tmp);
     } else if (strmatch (node->arbattrs[y], "pid")) {
      char *tmp = apply_variables (node->arbattrs[y+1], (const char **)if_vars);
      ni.pidfile = tmp;
      ni.environment = straddtoenviron(ni.environment, "pidfile", tmp);
     } else if (strmatch (node->arbattrs[y], "id")) {
      ni.name =  estrdup (node->arbattrs[y+1]);
     } else {
      char *tmp = apply_variables (node->arbattrs[y+1], (const char **)if_vars);
      ni.environment = straddtoenviron(ni.environment, node->arbattrs[y], tmp);
      free (tmp);
     }
    }

    if (ni.action) {
     retval = (struct interface_template_item **)setadd ((void **)retval, (void *)&ni, sizeof (struct interface_template_item));
    }
   }
  }
 }

 free (if_vars);
 free (ll);

 return retval;
}

struct interface_descriptor *network_import_interface_descriptor_string (char *ifname) {
 struct interface_descriptor *id = ecalloc (1, sizeof (struct interface_descriptor));
 char nodename[BUFFERSIZE];

 notice (2, "importing network interface descriptor for interface %s.", ifname);

 id->interface_name = ifname;

 esprintf (nodename, BUFFERSIZE, "configuration-network-interfaces-%s", id->interface_name);

// eputs (nodename, stderr);
 id->interface = cfg_getnode (nodename, NULL);

 if (id->interface && id->interface->arbattrs) {
  uint32_t i = 0;

  for (; id->interface->arbattrs[i]; i+=2) {
   if (strmatch (id->interface->arbattrs[i], "ip")) {
    id->ip_manager = network_import_templates ("ip", id->interface->arbattrs[i+1], id);
   } else if (strmatch (id->interface->arbattrs[i], "control")) {
    id->controller = network_import_templates ("ifctl", id->interface->arbattrs[i+1], id);
   } else if (strmatch (id->interface->arbattrs[i], "kernel-module")) {
    id->kernel_module = id->interface->arbattrs[i+1];
   } else if (strmatch (id->interface->arbattrs[i], "macchanger")) {
    id->macchanger = network_import_templates ("misc", "macchanger", id);
   } else if (strmatch (id->interface->arbattrs[i], "bridge")) {
    id->bridge = str2set (' ', id->interface->arbattrs[i+1]);

/*    if (n) {
     uint32_t r = 0;

     for (; n[r]; r++) {
      struct stree *ifst = streefind (network_modules, n[r], tree_find_first);

      if (ifst) {
       id->bridge_interfaces = streeadd (id->bridge_interfaces, n[r], ifst->value, SET_NOALLOC, NULL);
      }
     }

     free (n);
    } */
   }
  }
 } else {
  free (id);
  return NULL;
 }

 if (!id->ip_manager) {
  network_free_interface_descriptor (id);
  return NULL;
 }

 return id;
}

struct interface_descriptor *network_import_interface_descriptor (struct lmodule *lm) {
 struct interface_descriptor *id = network_import_interface_descriptor_string (lm->module->rid+10);

 if (id) id->status |= is_down;

 return id;
}

int network_execute_interface_action (struct interface_template_item **str, char *action, char *ctype, enum interface_action_command command, struct einit_event *status) {
 if (str) { // == NULL if ip not mentioned or ="none"
  fbprintf (status, "%s: %s controller", action, ctype);
  struct interface_template_item *start;
  struct interface_template_item *current = str[0];

  for (start = str[0], current = str[0]; current; ) {
   if (strmatch(action, "enable") && current->pidfile) unlink (current->pidfile);
   struct stree *t = streefind (current->action, action, tree_find_first);

   if (t) {
    if (pexec (t->value, (const char **)current->variables, 0, 0, NULL, NULL, current->environment, status) & status_ok) {
     if (strmatch(action, "disable") && current->pidfile) unlink (current->pidfile);

     return status_ok;
    } else {
     fbprintf (status, "%s controller doesn't work", ctype);
     if (command == iac_need_all) return status_failed;
    }
   }


   if ((command == iac_need_any) || (command == iac_need_all)) {
    if (str[1]) {
     ssize_t rx = 1;

     for (; str[rx]; rx++) {
      str[rx-1] = str[rx];
     }

     str[rx-1] = current;
     current = str[0];
    } else if (command == iac_need_any) {
     return status_failed;
    }
   } else {
    return status_failed;
   }

   if (current == start) {
    if (command == iac_need_all) {
     if (strmatch(action, "disable") && current->pidfile) unlink (current->pidfile);

     return status_ok;
    } else
     return status_failed;
   }
  }
 } else {
  return status_idle;
 }

 return status_failed;
}

int bridge_enable (struct interface_descriptor *id, struct einit_event *status) {
 uint32_t i = 0;

 if (id->bridge) for (; id->bridge[i]; i++) {
  struct stree *cur = streefind (network_modules, id->bridge[i], tree_find_first);

  if (cur) {
   struct lmodule *module = cur->value;
   if (module->status & status_enabled) {
    fbprintf (status, "interface %s already enabled, disabling ip-manager", cur->key);
    if (network_execute_interface_action (id->ip_manager, "disable", "IP", iac_need_this, status) == status_failed)
     return status_failed | status_enabled;
    if (flush_ip(id,status) == status_failed)
     return status_failed | status_enabled;
   } else {
    struct einit_event ev = evstaticinit(einit_core_change_service_status);
    char tmp[BUFFERSIZE];
    fbprintf (status, "suppressing ip controller on interface %s", cur->key);
    return mod (einit_module_custom, cur->value, "block-ip");
    fbprintf (status, "enabling network interface %s", cur->key);
    esprintf (tmp, BUFFERSIZE, "net-%s", cur->key);
    ev.set = setadd (ev.set, tmp, SET_TYPE_STRING);
    ev.set = setadd (ev.set, "enable", SET_TYPE_STRING);
    ev.stringset = (char **)ev.set;
    event_emit (&ev, einit_event_flag_broadcast);
    free (ev.set);
    evstaticdestroy (ev);
   }
  } else {
   fbprintf (status, "interface not defined: %s; skipping.", id->bridge[i]);
  }
 }
 return status_failed;
}

int flush_ip (struct interface_descriptor *id, struct einit_event *status) {
 char *ip_flush = cfg_getstring ("configuration-command-ip-flush/with-env", NULL);
 if (ip_flush) {
  fbprintf (status, "flushing routes and addresses");
  const char *ip_flush_env[] = { "interface", id->interface_name, NULL};
  char *cmd = apply_variables (ip_flush, ip_flush_env);
  return pexec (cmd, NULL, 0, 0, NULL, NULL, NULL, status);
 }
 return status_failed;
}

int create_bridge (struct interface_descriptor *id, struct einit_event *status) {
 char *cb = cfg_getstring ("configuration-command-create-bridge/with-env", NULL);
 if (cb) {
  char tmp[BUFFERSIZE];
  esprintf (tmp, BUFFERSIZE, "creating bridge %s", id->interface_name);
  const char *cb_env[] = { "interface", id->interface_name, NULL};
  char *cmd = apply_variables (cb, cb_env);
  return pexec (cmd, NULL, 0, 0, NULL, NULL, NULL, status);
 }
 return status_failed;
}

int network_ready (struct interface_descriptor *id, struct einit_event *status) {
#ifdef LINUX
/* made linux-specific, because only linux has sysfs */
 uint32_t retries = 0;
 int ret = status_ok;
 struct stat st;
 fbprintf (status, "waiting for interface to exist");
 char interface_path[BUFFERSIZE];
 esprintf (interface_path, BUFFERSIZE, "/sys/class/net/%s", id->interface_name); 
 while (stat(interface_path, &st)) {
  if (retries > 10000000) {
   fbprintf (status, "too many retries, giving up.");

   return status_failed;
  }

  sched_yield();
  retries++;
 }
 return ret;
#else
/* non-linux systems: hope for the best */
 return status_ok;
#endif
}

int network_interface_enable (struct interface_descriptor *id, struct einit_event *status) {
 int ret = 0;
 if (!id && !(id = network_import_interface_descriptor(status->para))) return status_failed;
 status->module->param = id;

 if (id->bridge) {
  if (create_bridge(id,status) == status_failed)
   goto fail;
  if (bridge_enable(id,status) == status_failed)
   goto fail;
 }

 fbprintf (status, "enabling network interface %s", id->interface_name);
 if (id->kernel_module) {
  if (network_ready(id,status) == status_failed)
   goto fail;
 }

 if (flush_ip(id,status) == status_failed)
  goto fail;

 if (id->macchanger) {
  if (network_execute_interface_action (id->macchanger, "enable", "macchanger", iac_need_any, status) == status_failed)
   goto fail;
 }

 if (network_execute_interface_action (id->controller, "enable", "interface", iac_need_all, status) == status_failed)
  goto fail;

 id->status |= is_ifctl_up;

 if (id->status & is_ip_blocked) {
  return status_ok;
 } else {
  ret = network_execute_interface_action (id->ip_manager, "enable", "IP", iac_need_any, status);

  if ((ret == status_idle) || (ret == status_failed)) {
   goto fail;
  } else {
   id->status |= is_ip_up;
   return ret;
  }
 }

 return status_ok;

 fail:
 if (id->bridge) {
//  struct stree *cur = id->bridge;
  uint32_t i = 0;

  if (id->bridge) for (; id->bridge[i]; i++) {
   struct stree *cur = streefind (network_modules, id->bridge[i], tree_find_first);

   if (cur) {
    fbprintf (status, "re-enabling ip controller on interface %s", cur->key);
    mod (einit_module_custom, cur->value, "unblock-ip");
   }
  }
 }

 return status_failed;
}

int network_interface_disable (struct interface_descriptor *id, struct einit_event *status) {
 if (!id && !(id = network_import_interface_descriptor(status->para))) return status_failed;
 status->module->param = id;

 if (id->status & is_ip_up) {
  if (network_execute_interface_action (id->ip_manager, "disable", "IP", iac_need_this, status) == status_failed)
   return status_failed;

  id->status ^= is_ip_up;
 } else {
  fbprintf (status, "IP Controller not up, skipping");
 }

 if (id->status & is_ifctl_up) {
  if (network_execute_interface_action (id->controller, "disable", "interface", iac_need_all, status) == status_failed)
   return status_failed;

  id->status ^= is_ifctl_up;
 } else {
  fbprintf (status, "Interface Controller not up, skipping");
 }

 return status_ok;
}

int network_interface_custom (struct interface_descriptor *id, char *action, struct einit_event *status) {
 if (!id && !(id = network_import_interface_descriptor(status->para))) return status_failed;
 status->module->param = id;

 if (strmatch (action, "block-ip")) {
  fbprintf (status, "blocking IP controller");

  if (id->status & is_ip_up) {

   if (network_execute_interface_action (id->ip_manager, "disable", "IP", iac_need_this, status) == status_failed)
    return status_failed | status_enabled;

   id->status |= is_ip_blocked;
   id->status ^= is_ip_up;

   return status_ok;
  } else {
   id->status |= is_ip_blocked;
   return status_ok;
  }
 } else if (strmatch (action, "unblock-ip")) {
  fbprintf (status, "unblocking IP controller");

  if (id->status & is_ip_blocked) id->status ^= is_ip_blocked;

  return status_ok;
 } else if (strmatch (action, "enable-ip")) {
  int ret = network_execute_interface_action (id->ip_manager, "enable", "IP", iac_need_this, status);

  if ((ret == status_failed) || (ret == status_idle))
   return status_failed | status_enabled;

  id->status |= is_ip_up;

  return status_ok;
 }

 if (status->module->status & status_enabled) {
  if (network_execute_interface_action (id->ip_manager, action, "IP", iac_need_this, status) == status_failed)
  return status_failed | status_enabled;

  return network_execute_interface_action (id->controller, action, "interface", iac_need_all, status) | status_enabled;
 } else {
  if (network_execute_interface_action (id->controller, action, "interface", iac_need_all, status) == status_failed)
   return status_failed | status_enabled;

  return network_execute_interface_action (id->ip_manager, action, "IP", iac_need_any, status) | status_enabled;
 }

 return status_ok | status_enabled;
}

int network_cleanup (struct lmodule *this) {
 exec_cleanup (this);

 return 0;
}

int network_interface_cleanup (struct lmodule *this) {
 return 0;
}

int network_interface_configure (struct lmodule *tm) {
 if (!tm->module || !tm->module->rid) return 1;

// eprintf (stderr, "new network module: %s\n", tm->module->rid);

 tm->cleanup = network_interface_cleanup;
 tm->enable = (int (*)(void *, struct einit_event *))network_interface_enable;
 tm->disable = (int (*)(void *, struct einit_event *))network_interface_disable;
 tm->custom = (int (*)(void *, char *, struct einit_event *))network_interface_custom;

 tm->param = NULL;

 tm->source = estrdup(tm->module->rid);

 network_modules = streeadd (network_modules, tm->module->rid + 10, tm, SET_NOALLOC, NULL);

 return 0;
}

int network_configure (struct lmodule *this) {
 module_init(this);
 exec_configure (this);

 thismodule->cleanup = network_cleanup;
 thismodule->scanmodules = network_scanmodules;

#if 0
 event_listen (einit_event_subsystem_core, network_einit_event_handler);
#endif

 return 0;
}
