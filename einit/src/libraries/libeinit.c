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
#include <einit/einit.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <expat.h>

DBusError *einit_dbus_error = NULL;
DBusError *einit_dbus_error_events = NULL;
DBusConnection *einit_dbus_connection = NULL;
DBusConnection *einit_dbus_connection_events = NULL;

#ifdef DARWIN
/* dammit, what's wrong with macos!? */

cfg_addnode_t cfg_addnode_fp = NULL;
cfg_findnode_t cfg_findnode_fp = NULL;
cfg_getstring_t cfg_getstring_fp = NULL;
cfg_getnode_t cfg_getnode_fp = NULL;
cfg_filter_t cfg_filter_fp = NULL;
cfg_getpath_t cfg_getpath_fp = NULL;
cfg_prefix_t cfg_prefix_fp = NULL;

struct cfgnode *cmode = NULL, *amode = NULL;
char *bootstrapmodulepath = NULL;
time_t boottime = 0;
enum einit_mode coremode = 0;
const struct smodule **coremodules[MAXMODULES] = { NULL };
char **einit_initial_environment = NULL;
char **einit_global_environment = NULL;
struct spidcb *cpids = NULL;
int einit_have_feedback = 1;
struct stree *service_aliases = NULL;
struct stree *service_usage = NULL;
char einit_new_node = 0;
struct event_function *event_functions = NULL;
struct stree *exported_functions = NULL;
unsigned char *gdebug = 0;
struct stree *hconfiguration = NULL;
struct utsname osinfo = {};
pthread_attr_t thread_attribute_detached = {};
struct spidcb *sched_deadorphans = NULL;
sched_watch_pid_t sched_watch_pid_fp = NULL;

#endif


DBusHandlerResult einit_incoming_event_handler(DBusConnection *connection, DBusMessage *message, void *user_data) {
 fprintf (stderr, "i've been called...\n");
 fflush (stderr);

 if (dbus_message_is_signal(message, "org.einit.Einit.Information", "EventSignal")) {
  fprintf (stderr, "got a message...\n");
  fflush (stderr);

  return DBUS_HANDLER_RESULT_HANDLED;
 }

 return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void *einit_message_thread(void *notused) {
 while (dbus_connection_read_write_dispatch(einit_dbus_connection_events, 100));

 fprintf (stderr, "lost connection...\n");

 return NULL;
}

char einit_connect() {
 einit_dbus_error = ecalloc (1, sizeof (DBusError));
 dbus_error_init(einit_dbus_error);

 if (!(einit_dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, einit_dbus_error))) {
  if (dbus_error_is_set(einit_dbus_error)) {
   fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error->message);
   dbus_error_free(einit_dbus_error);
  }
  return 0;
 }

 dbus_connection_set_exit_on_disconnect(einit_dbus_connection, FALSE);

 return 1;
}

char einit_disconnect() {
// ethread_join (&einit_message_thread_id);

 return 1;
}

//   if (dbus_message_is_signal(message, "org.einit.Einit.Information", "EventSignal"))

void einit_receive_events() {
 pthread_t einit_message_thread_id;
 einit_dbus_error_events = ecalloc (1, sizeof (DBusError));
 dbus_error_init(einit_dbus_error_events);

 if (!einit_dbus_connection_events) {
  if (!(einit_dbus_connection_events = dbus_bus_get(DBUS_BUS_SYSTEM, einit_dbus_error_events))) {
   if (dbus_error_is_set(einit_dbus_error)) {
    fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error->message);
    dbus_error_free(einit_dbus_error);
   }
   return;
  }
  dbus_connection_set_exit_on_disconnect(einit_dbus_connection_events, FALSE);
  dbus_bus_add_match(einit_dbus_connection_events, "type='signal',interface='org.einit.Einit.Information'", einit_dbus_error_events);

  dbus_connection_add_filter (einit_dbus_connection_events, einit_incoming_event_handler, NULL, NULL);
  ethread_create (&einit_message_thread_id, NULL, einit_message_thread, NULL);
 } else
  dbus_bus_add_match(einit_dbus_connection_events, "type='signal',interface='org.einit.Einit.Information'", einit_dbus_error_events);
}

char *einit_ipc_i (char *command, char *interface) {
 char *returnvalue;

 DBusMessage *message, *call;
 DBusMessageIter args;

 if (!(call = dbus_message_new_method_call("org.einit.Einit", "/org/einit/einit", interface, "IPC"))) {
  fprintf(stderr, "Sending message failed.\n");
  return NULL;
 }

 dbus_message_iter_init_append(call, &args);
 if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &command)) { 
  fprintf(stderr, "Out Of Memory!\n"); 
  return NULL;
 }

 if (!(message = dbus_connection_send_with_reply_and_block (einit_dbus_connection, call, 5000, einit_dbus_error))) {
  fprintf(stderr, "DBus Error (%s)\n", einit_dbus_error->message);
  return NULL;
 }

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

char *einit_ipc(char *command) {
 return einit_ipc_i (command, "org.einit.Einit.Command");
}

char *einit_ipc_safe(char *command) {
 return einit_ipc_i (command, "org.einit.Einit.Information");
}

char *einit_ipc_request(char *command) {
 if (einit_dbus_connection || einit_connect()) {
  return einit_ipc(command);
 }

 return NULL;
}

char *einit_ipc_request_xml(char *command) {
 char *tmp;
 char *rv;
 uint32_t len;

 if (!command) return NULL;
 tmp = emalloc ((len = (strlen (command) + 8)));

 esprintf (tmp, len, "%s --xml", command);

 rv = einit_ipc_request(tmp);

 free (tmp);

 return rv;
}

struct einit_xml2stree_parser_data {
 struct stree *node;
 struct stree *rootnode;
};

void einit_xml2stree_handler_tag_start (struct einit_xml2stree_parser_data *data, const XML_Char *name, const XML_Char **atts) {
 struct einit_xml_tree_node nnode_value;

 memset (&nnode_value, 0, sizeof (struct einit_xml_tree_node));

 nnode_value.parent = data->node;

 if (atts) {
  uint32_t i = 0;
  for (; atts[i]; i+=2) {
   nnode_value.attributes = streeadd (nnode_value.attributes, atts[i], atts[i+1], SET_TYPE_STRING, NULL);
  }
 }

 if (data->node) {
  ((struct einit_xml_tree_node *)(data->node->value))->elements = streeadd (((struct einit_xml_tree_node *)(data->node->value))->elements, name, &nnode_value, sizeof (struct einit_xml_tree_node), NULL);

  data->node = ((struct einit_xml_tree_node *)(data->node->value))->elements;
 } else {
  data->node = streeadd (data->node, name, &nnode_value, sizeof (struct einit_xml_tree_node), NULL);
 }

 if (!data->rootnode) data->rootnode = data->node;
}

void einit_xml2stree_handler_tag_end (struct einit_xml2stree_parser_data *data, const XML_Char *name) {
 if (data->node) {
  data->node = ((struct einit_xml_tree_node *)(data->node->value))->parent;
 }
}

struct stree *xml2stree (char *data) {
 if (data) {
  XML_Parser par = XML_ParserCreate (NULL);

  if (par) {
   struct einit_xml2stree_parser_data expatuserdata = {
    .node = NULL, .rootnode = NULL
   };

   XML_SetUserData (par, (void *)&expatuserdata);
   XML_SetElementHandler (par, (void (*)(void *, const XML_Char *, const XML_Char **))einit_xml2stree_handler_tag_start, (void (*)(void *, const XML_Char *))einit_xml2stree_handler_tag_end);

   if (XML_Parse (par, data, strlen(data), 1) == XML_STATUS_ERROR) {
    bitch (bitch_expat, 0, "XML Parser could not parse XML data");
   }

   XML_ParserFree (par);

   return expatuserdata.rootnode;
  } else {
   bitch (bitch_expat, 0, "XML Parser could not be created");
  }
 }

 return NULL;
}

void xmlstree_free (struct stree *tree) {
 static int recursion = 0;
 if (!tree) return;

 if (tree->value) {
  if (((struct einit_xml_tree_node *)(tree->value))->elements) {
   struct stree *cur = ((struct einit_xml_tree_node *)(tree->value))->elements;
   while (cur) {
    recursion++;
    xmlstree_free (cur);
    recursion--;

    cur = cur->next;
   }

   streefree (((struct einit_xml_tree_node *)(tree->value))->elements);
  }

  if (((struct einit_xml_tree_node *)(tree->value))->attributes) {
   streefree (((struct einit_xml_tree_node *)(tree->value))->attributes);
  }
 }

 if (!recursion)
  streefree (tree);
}

/* void print_xmlstree (struct stree *node) {
 if (node) {
  struct einit_xml_tree_node *da = node->value;

  eprintf (stderr, "element: %s {\n", node->key);
  if (da && da->attributes) {
   struct stree *cur = da->attributes;
   while (cur) {
    fprintf (stderr, " .%s = \"%s\";\n", cur->key, (char *)cur->value);
    cur = streenext(cur);
   }
  }

  if (da && da->elements) {
   struct stree *cur = da->elements;
   while (cur) {
    print_xmlstree(cur);
    cur = streenext(cur);
   }
  }

  eputs ("}\n", stderr);
 }
}*/

struct stree *einit_add_xmlstree_as_module (struct stree *rtree, struct stree *modulenode) {
 struct einit_module module;
 struct stree *attributes = ((struct einit_xml_tree_node *)(modulenode->value))->attributes;
 struct stree *sres;

 memset (&module, 0, sizeof (struct einit_module));

 if ((sres = streefind (attributes, "id", tree_find_first))) {
  module.id = estrdup(sres->value);

  if ((sres = streefind (attributes, "name", tree_find_first)))
   module.name = estrdup(sres->value);

  if ((sres = streefind (attributes, "requires", tree_find_first)))
   module.requires = str2set (':', sres->value);

  if ((sres = streefind (attributes, "provides", tree_find_first)))
   module.provides = str2set (':', sres->value);

  if ((sres = streefind (attributes, "after", tree_find_first)))
   module.after = str2set (':', sres->value);

  if ((sres = streefind (attributes, "before", tree_find_first)))
   module.before = str2set (':', sres->value);

  if ((sres = streefind (attributes, "status", tree_find_first))) {
   char **statusbits = str2set (':', sres->value);

   if (statusbits) {
    if (inset ((const void **)statusbits, (void *)"enabled", SET_TYPE_STRING)) {
     module.status |= status_enabled;
    }
    if (inset ((const void **)statusbits, (void *)"disabled", SET_TYPE_STRING)) {
     module.status |= status_disabled;
    }
    if (inset ((const void **)statusbits, (void *)"working", SET_TYPE_STRING)) {
     module.status |= status_working;
    }
    free (statusbits);
   }
  }

  rtree = streeadd (rtree, module.id, &module, sizeof (struct einit_module), NULL);
 }

 return rtree;
}

struct stree *einit_add_xmlstree_as_service (struct stree *rtree, struct stree *servicenode) {
 struct einit_service service;
 struct stree *attributes = ((struct einit_xml_tree_node *)(servicenode->value))->attributes;
 struct stree *sres;

 memset (&service, 0, sizeof (struct einit_service));

 if ((sres = streefind (attributes, "id", tree_find_first))) {
  service.name = estrdup(sres->value);

  if ((sres = streefind (attributes, "provided", tree_find_first)))
   service.status = strmatch (sres->value, "yes") ? service_provided : service_idle;

  if ((sres = streefind (attributes, "used-in", tree_find_first)))
   service.used_in_mode = (char **)str2set (':', sres->value);

  struct stree *modulenode;

  if ((modulenode = streefind (((struct einit_xml_tree_node *)(servicenode->value))->elements, "module", tree_find_first))) {
   do {
    service.modules = einit_add_xmlstree_as_module(service.modules, modulenode);
   } while ((modulenode = streefind (modulenode, "module", tree_find_next)));
  }

  struct stree *groupnode;

  if ((groupnode = streefind (((struct einit_xml_tree_node *)(servicenode->value))->elements, "group", tree_find_first))) {
   struct einit_group *group = emalloc (sizeof (struct einit_group));
   struct stree *gattributes = ((struct einit_xml_tree_node *)(groupnode->value))->attributes;
   struct stree *sres;

   if ((sres = streefind (gattributes, "members", tree_find_first))) {
    memset (group, 0, sizeof (struct einit_group));

    group->services = str2set(':', sres->value);

    if ((sres = streefind (gattributes, "seq", tree_find_first))) {
     group->seq = estrdup(sres->value);
    }

    service.group = group;
   } else free (group);
  }

  rtree = streeadd (rtree, service.name, &service, sizeof (struct einit_service), NULL);
 }

 return rtree;
}

struct stree *einit_get_all_modules () {
 struct stree *rtree = NULL;
 char *module_data;

 if (!einit_dbus_connection && !einit_connect()) return NULL;

 module_data = einit_ipc_safe ("list modules --xml");

 if (module_data) {
  struct stree *tree = xml2stree (module_data);

  if (tree) {
//   print_xmlstree(tree);

   struct stree *rootnode = streefind (tree, "einit-ipc", tree_find_first);
   if (rootnode && rootnode->value && ((struct einit_xml_tree_node *)(tree->value))->elements) {
    struct stree *modulenode;

    if ((modulenode = streefind (((struct einit_xml_tree_node *)(tree->value))->elements, "module", tree_find_first))) {
     do {
      rtree = einit_add_xmlstree_as_module(rtree, modulenode);
     } while ((modulenode = streefind (modulenode, "module", tree_find_next)));
    }
   }

   xmlstree_free (tree);
  }

  free (module_data);
 }

 return rtree;
}

struct einit_module *einit_get_module_status (char *module) {
 struct stree *md = einit_get_all_modules();
 struct einit_module *rv = NULL;

 if (md) {
  struct stree *rc = streefind (md, module, tree_find_first);

  if (rc && rc->value) {
   rv = emalloc (sizeof (struct einit_module));

   rv->id = estrdup (((struct einit_module *)(rc->value))->id);
   rv->name = estrdup (((struct einit_module *)(rc->value))->name);

   rv->status = ((struct einit_module *)(rc->value))->status;

   rv->requires = (char **)setdup ((const void **)(((struct einit_module *)(rc->value))->requires), SET_TYPE_STRING);
   rv->provides = (char **)setdup ((const void **)(((struct einit_module *)(rc->value))->provides), SET_TYPE_STRING);
   rv->after = (char **)setdup ((const void **)(((struct einit_module *)(rc->value))->after), SET_TYPE_STRING);
   rv->before = (char **)setdup ((const void **)(((struct einit_module *)(rc->value))->before), SET_TYPE_STRING);
  }

  modulestree_free (md);
 }

 return rv;
}

void einit_module_free (struct einit_module *module) {
 if (module) {
  if (module->id) free (module->id);
  if (module->name) free (module->name);
  if (module->requires) free (module->requires);
  if (module->provides) free (module->provides);
  if (module->after) free (module->after);
  if (module->before) free (module->before);

  free (module);
 }
}

void modulestree_free(struct stree *tree) {
 if (!tree) return;

 struct stree *cur = tree;
 do {
  struct einit_module *module = cur->value;

  if (module) {
   if (module->id) free (module->id);
   if (module->name) free (module->name);
   if (module->requires) free (module->requires);
   if (module->provides) free (module->provides);
   if (module->after) free (module->after);
   if (module->before) free (module->before);
  }

  cur = streenext (cur);
 } while (cur);

 streefree (tree);
}

struct stree *einit_get_all_services () {
 struct stree *rtree = NULL;
 char *module_data;

 if (!einit_dbus_connection && !einit_connect()) return NULL;

 module_data = einit_ipc_safe ("list services --xml");

 if (module_data) {
  struct stree *tree = xml2stree (module_data);

  if (tree) {
//   print_xmlstree(tree);

   struct stree *rootnode = streefind (tree, "einit-ipc", tree_find_first);
   if (rootnode && rootnode->value && ((struct einit_xml_tree_node *)(tree->value))->elements) {
    struct stree *servicenode;
    if ((servicenode = streefind (((struct einit_xml_tree_node *)(tree->value))->elements, "service", tree_find_first))) {
     do {
      rtree = einit_add_xmlstree_as_service(rtree, servicenode);
     } while ((servicenode = streefind (servicenode, "service", tree_find_next)));
    }
   }

   xmlstree_free (tree);
  }

  free (module_data);
 }

 return rtree;
}

void servicestree_free_protect(struct stree *tree, char *pserv) {
 if (!tree) return;

 struct stree *cur = tree;
 do {
  if (!strmatch (pserv, cur->key)) {
   struct einit_service *service = cur->value;

   if (service) {
    if (service->name) free (service->name);
    if (service->used_in_mode) free (service->used_in_mode);
    if (service->group) {
     if (service->group->services)
      free (service->group->services);
     if (service->group->seq)
      free (service->group->seq);

     free (service->group);
    }
    if (service->modules) modulestree_free (service->modules);
   }
  }

  cur = streenext (cur);
 } while (cur);

 streefree (tree);
}


struct einit_service *einit_get_service_status (char *service) {
 struct einit_service *rv = NULL;

 struct stree *s = einit_get_all_services();

 if (s) {
  struct stree *rc = streefind (s, service, tree_find_first);

  if (rc) {
   struct einit_service *ov = rc->value;

   if (ov) {
    rv = emalloc (sizeof (struct einit_service));

    memcpy (rv, ov, sizeof (struct einit_service));
   }
  }

  servicestree_free_protect(s, service); 
 }

 return rv;
}

void einit_service_free (struct einit_service *service) {
 if (service) {
  if (service->name) free (service->name);
  if (service->used_in_mode) free (service->used_in_mode);
  if (service->group) {
   if (service->group->services)
    free (service->group->services);
   if (service->group->seq)
    free (service->group->seq);

   free (service->group);
  }
  if (service->modules) modulestree_free (service->modules);
 }
}

void servicestree_free(struct stree *tree) {
 if (!tree) return;

 struct stree *cur = tree;
 do {
  struct einit_service *service = cur->value;

  if (service) {
   if (service->name) free (service->name);
   if (service->used_in_mode) free (service->used_in_mode);
   if (service->group) {
    if (service->group->services)
     free (service->group->services);
    if (service->group->seq)
     free (service->group->seq);

    free (service->group);
   }
   if (service->modules) modulestree_free (service->modules);
  }

  cur = streenext (cur);
 } while (cur);

 streefree (tree);
}


void einit_power_down () { // shut down
 char *r = einit_ipc_request_xml ("power down");
 free (r);
}

void einit_power_reset () { // reboot
 char *r = einit_ipc_request_xml ("power reset");
 free (r);
}

void einit_service_call (char *service, char *command) {
 char *tmp;
 uint32_t len;

 if (!command || !service) return;
 tmp = emalloc ((len = (strlen(service) + strlen (command) + 14)));

 esprintf (tmp, len, "rc %s %s --detach", service, command);

 einit_ipc_request_xml(tmp);

 free (tmp);
}

void einit_service_enable (char *service) {
 einit_service_call (service, "enable");
}

void einit_service_disable (char *service) {
 einit_service_call (service, "disable");
}

void einit_module_id_call (char *module, char *command) {
 char *tmp;
 uint32_t len;

 if (!command || !module) return;
 tmp = emalloc ((len = (strlen(module) + strlen (command) + 21)));

 esprintf (tmp, len, "module-rc %s %s --detach", module, command);

 einit_ipc_request_xml(tmp);

 free (tmp);
}

void einit_module_id_enable (char *module) {
 einit_module_id_call (module, "enable");
}

void einit_module_id_disable (char *module) {
 einit_module_id_call (module, "disable");
}

void einit_switch_mode (char *mode) { // think "runlevel"
 char *tmp;
 uint32_t len;

 if (!mode) return;
 tmp = emalloc ((len = (strlen(mode) + 25)));

 esprintf (tmp, len, "rc switch-mode %s --detach", mode);

 einit_ipc_request_xml(tmp);

 free (tmp);
}

void einit_reload_configuration () { // "update configuration"
 char *r = einit_ipc_request_xml ("update configuration");
 free (r);
}

struct stree *einit_add_xmlstree_as_mode (struct stree *rtree, struct stree *modenode) {
 struct einit_mode_summary mode;
 struct stree *attributes = ((struct einit_xml_tree_node *)(modenode->value))->attributes;
 struct stree *sres;

 memset (&mode, 0, sizeof (struct einit_mode_summary));

 if ((sres = streefind (attributes, "id", tree_find_first))) {
  mode.id = estrdup(sres->value);

  if ((sres = streefind (attributes, "base", tree_find_first)))
   mode.base = str2set(':', sres->value);

  struct stree *rnode;

  if ((rnode = streefind (((struct einit_xml_tree_node *)(modenode->value))->elements, "enable", tree_find_first))) {
   struct stree *gattributes = ((struct einit_xml_tree_node *)(rnode->value))->attributes;
   struct stree *sres;

   if ((sres = streefind (gattributes, "services", tree_find_first))) {
    mode.services = str2set(':', sres->value);
   }

   if ((sres = streefind (gattributes, "critical", tree_find_first))) {
    mode.critical = str2set(':', sres->value);
   }
  }

  if ((rnode = streefind (((struct einit_xml_tree_node *)(modenode->value))->elements, "disable", tree_find_first))) {
   struct stree *gattributes = ((struct einit_xml_tree_node *)(rnode->value))->attributes;
   struct stree *sres;

   if ((sres = streefind (gattributes, "services", tree_find_first))) {
    mode.disable = str2set(':', sres->value);
   }
  }

  rtree = streeadd (rtree, mode.id, &mode, sizeof (struct einit_mode_summary), NULL);
 }

 return rtree;
}

struct stree *einit_get_all_modes() {
 struct stree *rtree = NULL;
 char *mode_data;

 if (!einit_dbus_connection && !einit_connect()) return NULL;

 mode_data = einit_ipc_safe ("list modes --xml");

 if (mode_data) {
  struct stree *tree = xml2stree (mode_data);

  if (tree) {
//   print_xmlstree(tree);

   struct stree *rootnode = streefind (tree, "einit-ipc", tree_find_first);
   if (rootnode && rootnode->value && ((struct einit_xml_tree_node *)(tree->value))->elements) {
    struct stree *modenode;

    if ((modenode = streefind (((struct einit_xml_tree_node *)(tree->value))->elements, "mode", tree_find_first))) {
     do {
      rtree = einit_add_xmlstree_as_mode(rtree, modenode);
     } while ((modenode = streefind (modenode, "mode", tree_find_next)));
    }
   }

   xmlstree_free (tree);
  }

  free (mode_data);
 }

 return rtree;
}

void modestree_free(struct stree *tree) {
 if (!tree) return;

 struct stree *cur = tree;
 do {
  struct einit_mode_summary *mode = cur->value;

  if (mode) {
   if (mode->id) free (mode->id);
   if (mode->base) free (mode->base);
   if (mode->services) free (mode->services);
   if (mode->critical) free (mode->critical);
   if (mode->disable) free (mode->disable);
  }

  cur = streenext (cur);
 } while (cur);

 streefree (tree);
}
