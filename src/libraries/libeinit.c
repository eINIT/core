/*
 *  libeinit.c
 *  einit
 *
 *  Created by Magnus Deininger on 24/07/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
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

#include <einit/einit.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <expat.h>

#include <dlfcn.h>

#ifdef DARWIN
/* dammit, what's wrong with macos!? */

struct exported_function *cfg_addnode_fs = NULL;
struct exported_function *cfg_findnode_fs = NULL;
struct exported_function *cfg_getstring_fs = NULL;
struct exported_function *cfg_getnode_fs = NULL;
struct exported_function *cfg_filter_fs = NULL;
struct exported_function *cfg_getpath_fs = NULL;
struct exported_function *cfg_prefix_fs = NULL;

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
struct stree *exported_functions = NULL;
unsigned char *gdebug = 0;
struct stree *hconfiguration = NULL;
struct utsname osinfo = {};
pthread_attr_t thread_attribute_detached = {};
struct spidcb *sched_deadorphans = NULL;
sched_watch_pid_t sched_watch_pid_fp = NULL;
char einit_quietness = 0;

#endif

struct remote_event_function {
 uint32_t type;                                 /*!< type of function */
 void (*handler)(struct einit_remote_event *);  /*!< handler function */
 struct remote_event_function *next;            /*!< next function */
};

pthread_mutex_t einit_evf_mutex = PTHREAD_MUTEX_INITIALIZER;
struct remote_event_function *event_remote_event_functions = NULL;
uint32_t remote_event_cseqid = 1;

char einit_connected = 0;

void *einit_ipc_lib_handle = NULL;

char (*einit_disconnect_fp)() = NULL;
void (*einit_receive_events_fp)() = NULL;
char *(*einit_ipc_fp)(const char *) = NULL;
char *(*einit_ipc_safe_fp)(const char *) = NULL;
char *(*einit_ipc_request_fp)(const char *) = NULL;

void (*einit_remote_event_emit_dispatch_fp)(struct einit_remote_event *) = NULL;
void (*einit_remote_event_emit_fp)(struct einit_remote_event *, enum einit_event_emit_flags) = NULL;

char einit_connect(int *argc, char **argv) {
/* TODO: fix this up again */
 char t = 0;

 for (; t < 2; t++) {
  char *clib;

  switch (t) {
   case 0: clib = EINIT_LIB_BASE "/ipc/libeinit-ipc-dbus.so"; break;
   case 1: clib = EINIT_LIB_BASE "/ipc/libeinit-ipc-socket.so"; break;
   default: clib = NULL; break;
  }

  if ((einit_ipc_lib_handle = dlopen (clib, RTLD_NOW))) {
   char (*cfunc)(int*, char**) = dlsym (einit_ipc_lib_handle, "einit_connect");
   if (cfunc) {
    if (cfunc(argc, argv)) {
     einit_disconnect_fp = dlsym (einit_ipc_lib_handle, "einit_disconnect");
     einit_receive_events_fp = dlsym (einit_ipc_lib_handle, "einit_receive_events");
     einit_ipc_fp = dlsym (einit_ipc_lib_handle, "einit_ipc");
     einit_ipc_safe_fp = dlsym (einit_ipc_lib_handle, "einit_ipc_safe");
     einit_ipc_request_fp = dlsym (einit_ipc_lib_handle, "einit_ipc_request");

     einit_remote_event_emit_dispatch_fp = dlsym (einit_ipc_lib_handle, "einit_remote_event_emit_dispatch");
     einit_remote_event_emit_fp = dlsym (einit_ipc_lib_handle, "einit_remote_event_emit");

     return (einit_connected = 1);
    }
   } else {
    eputs (dlerror (), stderr);

    dlclose (einit_ipc_lib_handle);
    einit_ipc_lib_handle = NULL;
   }
  } else {
/* don't nag if the lib can't be loaded... */
//   eputs (dlerror (), stderr);
   continue;
  }
 }

 return (einit_connected = 0);
}

char einit_disconnect() {
 if (einit_disconnect_fp) return einit_disconnect_fp();

 return 0;
}

void einit_receive_events() {
 if (einit_receive_events_fp) einit_receive_events_fp();

 return;
}

char *einit_ipc(const char *command) {
 if (einit_ipc_fp) return einit_ipc_fp(command);

 return NULL;
}

char *einit_ipc_safe(const char *command) {
 if (einit_ipc_safe_fp) return einit_ipc_safe_fp(command);

 return NULL;
}

char *einit_ipc_request(const char *command) {
 if (einit_ipc_request_fp) return einit_ipc_request_fp(command);

 return NULL;
}

char *einit_ipc_request_xml(const char *command) {
 char *tmp;
 char *rv;
 uint32_t len;

 if (!command) return NULL;
 tmp = emalloc ((len = (strlen (command) + 8)));

 esprintf (tmp, len, "%s --xml", command);

 rv = einit_ipc_request(tmp);

 efree (tmp);

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
   struct stree *cur = streelinear_prepare(((struct einit_xml_tree_node *)(tree->value))->elements);
   while (cur) {
    recursion++;
    xmlstree_free (cur);
    recursion--;

    cur = streenext(cur);
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

  if ((sres = streefind (attributes, "functions", tree_find_first)))
   module.functions = str2set (':', sres->value);

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
    efree (statusbits);
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
   } else efree (group);
  }

  rtree = streeadd (rtree, service.name, &service, sizeof (struct einit_service), NULL);
 }

 return rtree;
}

struct stree *einit_get_all_modules () {
 struct stree *rtree = NULL;
 char *module_data;

 if (!einit_connected && !einit_connect(NULL, NULL)) return NULL;

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

  efree (module_data);
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

   rv->functions = (char **)setdup ((const void **)(((struct einit_module *)(rc->value))->functions), SET_TYPE_STRING);

   rv->before = (char **)setdup ((const void **)(((struct einit_module *)(rc->value))->before), SET_TYPE_STRING);
  }

  modulestree_free (md);
 }

 return rv;
}

void einit_module_free (struct einit_module *module) {
 if (module) {
  if (module->id) efree (module->id);
  if (module->name) efree (module->name);
  if (module->requires) efree (module->requires);
  if (module->provides) efree (module->provides);
  if (module->after) efree (module->after);
  if (module->functions) efree (module->functions);
  if (module->before) efree (module->before);

  efree (module);
 }
}

void modulestree_free(struct stree *tree) {
 if (!tree) return;

 struct stree *cur = streelinear_prepare(tree);
 do {
  struct einit_module *module = cur->value;

  if (module) {
   if (module->id) efree (module->id);
   if (module->name) efree (module->name);
   if (module->requires) efree (module->requires);
   if (module->provides) efree (module->provides);
   if (module->after) efree (module->after);
   if (module->functions) efree (module->functions);
   if (module->before) efree (module->before);
  }

  cur = streenext (cur);
 } while (cur);

 streefree (tree);
}

struct stree *einit_get_all_services () {
 struct stree *rtree = NULL;
 char *module_data;

 if (!einit_connected && !einit_connect(NULL, NULL)) return NULL;

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

  efree (module_data);
 }

 return rtree;
}

void servicestree_free_protect(struct stree *tree, char *pserv) {
 if (!tree) return;

 struct stree *cur = streelinear_prepare(tree);
 do {
  if (!strmatch (pserv, cur->key)) {
   struct einit_service *service = cur->value;

   if (service) {
    if (service->name) efree (service->name);
    if (service->used_in_mode) efree (service->used_in_mode);
    if (service->group) {
     if (service->group->services)
      efree (service->group->services);
     if (service->group->seq)
      efree (service->group->seq);

     efree (service->group);
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
  if (service->name) efree (service->name);
  if (service->used_in_mode) efree (service->used_in_mode);
  if (service->group) {
   if (service->group->services)
    efree (service->group->services);
   if (service->group->seq)
    efree (service->group->seq);

   efree (service->group);
  }
  if (service->modules) modulestree_free (service->modules);
 }
}

void servicestree_free(struct stree *tree) {
 if (!tree) return;

 struct stree *cur = streelinear_prepare(tree);
 do {
  struct einit_service *service = cur->value;

  if (service) {
   if (service->name) efree (service->name);
   if (service->used_in_mode) efree (service->used_in_mode);
   if (service->group) {
    if (service->group->services)
     efree (service->group->services);
    if (service->group->seq)
     efree (service->group->seq);

    efree (service->group);
   }
   if (service->modules) modulestree_free (service->modules);
  }

  cur = streenext (cur);
 } while (cur);

 streefree (tree);
}


void einit_power_down () { // shut down
 char *r = einit_ipc_request_xml ("power down");
 efree (r);
}

void einit_power_reset () { // reboot
 char *r = einit_ipc_request_xml ("power reset");
 efree (r);
}

void einit_service_call (const char *service, const char *command) {
 char *tmp;
 uint32_t len;

 if (!command || !service) return;
 tmp = emalloc ((len = (strlen(service) + strlen (command) + 14)));

 esprintf (tmp, len, "rc %s %s --detach", service, command);

 einit_ipc_request_xml(tmp);

 efree (tmp);
}

void einit_service_enable (const char *service) {
 einit_service_call (service, "enable");
}

void einit_service_disable (const char *service) {
 einit_service_call (service, "disable");
}

void einit_module_id_call (const char *module, const char *command) {
 char *tmp;
 uint32_t len;

 if (!command || !module) return;
 tmp = emalloc ((len = (strlen(module) + strlen (command) + 21)));

// esprintf (tmp, len, "module-rc %s %s --detach", module, command);
 esprintf (tmp, len, "rc %s %s --detach", module, command);

 einit_ipc_request_xml(tmp);

 efree (tmp);
}

void einit_module_id_enable (const char *module) {
 einit_module_id_call (module, "enable");
}

void einit_module_id_disable (const char *module) {
 einit_module_id_call (module, "disable");
}

void einit_switch_mode (const char *mode) { // think "runlevel"
 char *tmp;
 uint32_t len;

 if (!mode) return;
 tmp = emalloc ((len = (strlen(mode) + 25)));

 esprintf (tmp, len, "rc switch-mode %s --detach", mode);

 einit_ipc_request_xml(tmp);

 efree (tmp);
}

void einit_reload_configuration () { // "update configuration"
 char *r = einit_ipc_request_xml ("update configuration");
 efree (r);
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

 if (!einit_connected && !einit_connect(NULL, NULL)) return NULL;

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

  efree (mode_data);
 }

 return rtree;
}

void modestree_free(struct stree *tree) {
 if (!tree) return;

 struct stree *cur = streelinear_prepare(tree);
 do {
  struct einit_mode_summary *mode = cur->value;

  if (mode) {
   if (mode->id) efree (mode->id);
   if (mode->base) efree (mode->base);
   if (mode->services) efree (mode->services);
   if (mode->critical) efree (mode->critical);
   if (mode->disable) efree (mode->disable);
  }

  cur = streenext (cur);
 } while (cur);

 streefree (tree);
}

void einit_remote_event_listen (enum einit_event_subsystems type, void (* handler)(struct einit_remote_event *)) {
 struct remote_event_function *fstruct = ecalloc (1, sizeof (struct remote_event_function));

 fstruct->type = type & EVENT_SUBSYSTEM_MASK;
 fstruct->handler = handler;

 emutex_lock (&einit_evf_mutex);
  if (event_remote_event_functions)
   fstruct->next = event_remote_event_functions;

  event_remote_event_functions = fstruct;
 emutex_unlock (&einit_evf_mutex);
}

void einit_remote_event_ignore (enum einit_event_subsystems type, void (* handler)(struct einit_remote_event *)) {
 if (!event_remote_event_functions) return;

 uint32_t ltype = type & EVENT_SUBSYSTEM_MASK;

 emutex_lock (&einit_evf_mutex);
  struct remote_event_function *cur = event_remote_event_functions;
  struct remote_event_function *prev = NULL;
  while (cur) {
   if ((cur->type==ltype) && (cur->handler==handler)) {
    if (prev == NULL) {
     event_remote_event_functions = cur->next;
     efree (cur);
     cur = event_remote_event_functions;
    } else {
     prev->next = cur->next;
     efree (cur);
     cur = prev->next;
    }
   } else {
    prev = cur;
    cur = cur->next;
   }
  }
 emutex_unlock (&einit_evf_mutex);

 return;
}

void einit_remote_event_emit_dispatch (struct einit_remote_event *ev) {
 if (einit_remote_event_emit_dispatch_fp) einit_remote_event_emit_dispatch_fp(ev);

 return;
}

void einit_remote_event_emit (struct einit_remote_event *ev, enum einit_event_emit_flags flags) {
 if (einit_remote_event_emit_fp) einit_remote_event_emit_fp(ev, flags);

 return;
}

struct einit_remote_event *einit_remote_event_create (uint32_t type) {
 struct einit_remote_event *nev = ecalloc (1, sizeof (struct einit_remote_event));

 nev->type = type;

 return nev;
}

void einit_remote_event_destroy (struct einit_remote_event *ev) {
 efree (ev);
}
