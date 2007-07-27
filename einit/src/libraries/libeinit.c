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

DBusError einit_dbus_error;
DBusConnection *einit_dbus_connection = NULL;

char einit_connect() {
 dbus_error_init(&einit_dbus_error);

 if (!(einit_dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &einit_dbus_error))) {
  if (dbus_error_is_set(&einit_dbus_error)) {
   fprintf(stderr, "Connection Error (%s)\n", einit_dbus_error.message);
   dbus_error_free(&einit_dbus_error);
  }
  return 0;
 }

 dbus_connection_set_exit_on_disconnect(einit_dbus_connection, FALSE);

 return 1;
}

char *einit_ipc(char *command) {
 char *returnvalue;

 DBusMessage *message;
 DBusMessageIter args;
 DBusPendingCall *pending;

 if (!(message = dbus_message_new_method_call("org.einit.Einit", "/org/einit/einit", "org.einit.Einit.Command", "IPC"))) {
  fprintf(stderr, "Sending message failed.\n");
  return NULL;
 }

 dbus_message_iter_init_append(message, &args);
 if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &command)) { 
  fprintf(stderr, "Out Of Memory!\n"); 
  return NULL;
 }

 if (!dbus_connection_send_with_reply (einit_dbus_connection, message, &pending, -1)) {
  fprintf(stderr, "Out Of Memory!\n");
  return NULL;
 }
 if (!pending) { 
  fprintf(stderr, "No return value?\n"); 
  return NULL;
 }
 dbus_connection_flush(einit_dbus_connection);

 dbus_message_unref(message);

 dbus_pending_call_block(pending);

 if (!(message = dbus_pending_call_steal_reply(pending))) {
  fprintf(stderr, "Bad Reply\n");
  return NULL;
 }
 dbus_pending_call_unref(pending);

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

/*
void print_xmlstree (struct stree *node) {
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

struct stree *einit_get_all_modules () {
 struct stree *rtree = NULL;
 char *module_data = einit_ipc_request_xml ("list modules");

 if (module_data) {
  struct stree *tree = xml2stree (module_data);

  if (tree) {
//   print_xmlstree(tree);

   struct stree *rootnode = streefind (tree, "einit-ipc", tree_find_first);
   if (rootnode && rootnode->value && ((struct einit_xml_tree_node *)(tree->value))->elements) {
    struct stree *modulenode;

    if ((modulenode = streefind (((struct einit_xml_tree_node *)(tree->value))->elements, "module", tree_find_first))) {
     do {
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
     } while ((modulenode = streefind (modulenode, "module", tree_find_next)));
    }
   }

   xmlstree_free (tree);
  }

  free (module_data);
 }

 return rtree;
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

void einit_power_down () {
 char *r = einit_ipc_request_xml ("power down");
 free (r);
}

void einit_power_reset () {
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
