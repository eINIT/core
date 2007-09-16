/*
 *  module-xml.c
 *  einit
 *
 *  Created by Magnus Deininger on 16/10/2007.
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

#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_xml_v2_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_xml_v2_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (Configuration: Unified Daemon and Shell Modules)",
 .rid       = "einit-module-xml-v2",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_xml_v2_configure
};

module_register(module_xml_v2_self);

#endif

#define MODULES_PREFIX "services-virtual-module-"
#define MODULES_PREFIX_LENGTH (sizeof(MODULES_PREFIX) -1)

#define MODULES_EXECUTE_NODE_TEMPLATE MODULES_PREFIX "%s-execute"

struct stree *module_xml_v2_modules = NULL;

int module_xml_v2_scanmodules (struct lmodule *);

int module_xml_v2_cleanup (struct lmodule *pa) {
 exec_cleanup (pa);

 return 0;
}

int module_xml_v2_module_custom_action (char *name, char *action, struct einit_event *status) {
 if (name && action) {
  char buffer[BUFFERSIZE];
  struct cfgnode *node = NULL;

  esprintf (buffer, BUFFERSIZE, MODULES_EXECUTE_NODE_TEMPLATE, name);

  while ((node = cfg_findnode (buffer, 0, node))) {
   if (node->idattr && strmatch (node->idattr, action)) {
    int x = 0;
    char *code = NULL, *variables = NULL, *user = NULL, *group = NULL;

    for (; node->arbattrs[x]; x+=2) {
     if (strmatch (node->arbattrs[x], "code")) code = node->arbattrs[x+1];
     else if (strmatch (node->arbattrs[x], "variables")) variables = node->arbattrs[x+1];
     else if (strmatch (node->arbattrs[x], "user")) user = node->arbattrs[x+1];
     else if (strmatch (node->arbattrs[x], "group")) group = node->arbattrs[x+1];
    }

    if (code) {
     if (variables) {
      char **split_variables;
      int result;

      split_variables = str2set (':', variables);

      result = pexec (code, (const char **)split_variables, 0, 0, user, group, NULL, status);

      free (split_variables);

      return result;
     } else
      return pexec (code, NULL, 0, 0, user, group, NULL, status);
    } else
     return status_failed;
   }
  }
 }

 return status_failed;
}

int module_xml_v2_module_enable (char *name, struct einit_event *status) {
 return module_xml_v2_module_custom_action (name, "enable", status);
}

int module_xml_v2_module_disable (char *name, struct einit_event *status) {
 return module_xml_v2_module_custom_action (name, "disable", status);
}

int module_xml_v2_module_configure (struct lmodule *pa) {
 pa->param = pa->module->rid;

 pa->enable = (int (*)(void *, struct einit_event *))module_xml_v2_module_enable;
 pa->disable = (int (*)(void *, struct einit_event *))module_xml_v2_module_disable;
 pa->custom = (int (*)(void *, char *, struct einit_event *))module_xml_v2_module_custom_action;

 module_xml_v2_modules = streeadd (module_xml_v2_modules, pa->module->rid, pa, SET_NOALLOC, NULL);

 return 0;
}

int module_xml_v2_scanmodules (struct lmodule *modchain) {
 struct stree *modules_to_update = module_xml_v2_modules;

 while (modules_to_update) {
  mod_update (modules_to_update->value);

  modules_to_update = streenext (modules_to_update);
 }

 struct stree *module_nodes = cfg_prefix(MODULES_PREFIX);

 if (module_nodes) {
  struct stree *cur = module_nodes;

  for (; cur; cur = streenext (cur)) {
/* exclude legacy nodes */
   if (strcmp (cur->key, MODULES_PREFIX "shell") && strcmp (cur->key, MODULES_PREFIX "daemon")) {
    struct cfgnode *node = cur->value;
    notice (1, "processing id=%s", cur->key + MODULES_PREFIX_LENGTH);

    if ((!module_xml_v2_modules || !streefind (module_xml_v2_modules, cur->key + MODULES_PREFIX_LENGTH, tree_find_first)) && node->arbattrs) {
     int i = 0;
     char *name = NULL, *requires = NULL, *provides = NULL, *after = NULL, *before = NULL;

     for (; node->arbattrs[i]; i+=2) {
      if (strmatch (node->arbattrs[i], "name")) {
       name = node->arbattrs[i+1];
      } else if (strmatch (node->arbattrs[i], "provides")) {
       provides = node->arbattrs[i+1];
      } else if (strmatch (node->arbattrs[i], "requires")) {
       requires = node->arbattrs[i+1];
      } else if (strmatch (node->arbattrs[i], "after")) {
       after = node->arbattrs[i+1];
      } else if (strmatch (node->arbattrs[i], "before")) {
       before = node->arbattrs[i+1];
      }
     }

     if (name && provides) {
      struct smodule *new_sm = emalloc (sizeof (struct smodule));
      memset (new_sm, 0, sizeof (struct smodule));

      new_sm->rid = estrdup (cur->key + MODULES_PREFIX_LENGTH);
      new_sm->name = estrdup (name);

      new_sm->eiversion = EINIT_VERSION;
      new_sm->eibuild = BUILDNUMBER;
      new_sm->version = 1;
      new_sm->mode = einit_module_generic;

      new_sm->si.provides = str2set (':', provides);
      if (requires) new_sm->si.requires = str2set (':', requires);
      if (after) new_sm->si.after = str2set (':', after);
      if (before) new_sm->si.before = str2set (':', before);

      new_sm->configure = module_xml_v2_module_configure;

      mod_add (NULL, new_sm);

      continue;
     }
    }
   }

  }

  streefree (module_nodes);
 }

 return 1;
}

int module_xml_v2_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 pa->scanmodules = module_xml_v2_scanmodules;
 pa->cleanup = module_xml_v2_cleanup;

 return 0;
}
