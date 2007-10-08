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

#include <sys/stat.h>
#include <libgen.h>

#include <einit-modules/exec.h>

#include <dlfcn.h>

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
#define MODULES_ARBITRARY_NODE_TEMPLATE MODULES_PREFIX "%s-%s"

struct stree *module_xml_v2_modules = NULL;
struct stree *module_xml_v2_daemons = NULL;

int module_xml_v2_scanmodules (struct lmodule *);

struct cfgnode *module_xml_v2_module_get_node (char *name, char *action) {
 if (name && action) {
  char buffer[BUFFERSIZE];
  struct cfgnode *node = NULL;

  esprintf (buffer, BUFFERSIZE, MODULES_EXECUTE_NODE_TEMPLATE, name);

  while ((node = cfg_findnode (buffer, 0, node))) {
   if (node->idattr && strmatch (node->idattr, action)) {
    return node;
   }
  }
 }

 return NULL;
}

struct cfgnode *module_xml_v2_module_get_attributive_node (char *name, char *attribute) {
 if (name && attribute) {
  char buffer[BUFFERSIZE];

  esprintf (buffer, BUFFERSIZE, MODULES_ARBITRARY_NODE_TEMPLATE, name, attribute);

  return cfg_getnode (buffer, NULL);
 }

 return NULL;
}

char module_xml_v2_module_have_script_action (char *name, char *action) {
 struct cfgnode * t = module_xml_v2_module_get_attributive_node (name, "script");

 if (t && t->arbattrs) {
  int i = 0;

  for (; t->arbattrs[i]; i+=2) {
   if (strmatch (t->arbattrs[i], "actions")) {
    char **actions = str2set (':', t->arbattrs[i+1]);
    char rv = 0;

    rv = inset ((const void **)actions, action, SET_TYPE_STRING);

    free (actions);
    return rv;
   }
  }
 }

 return 0;
}

char module_xml_v2_module_have_action (char *name, char *action) {
 if (module_xml_v2_module_get_node (name, action)) {
  return 1;
 } else {
  return module_xml_v2_module_have_script_action (name, action);
 }

 return 0;
}

int module_xml_v2_module_custom_action (char *name, char *action, struct einit_event *status) {
 struct cfgnode *node = NULL;
 char **myenvironment = NULL;
 int returnvalue = status_failed;

 if ((node = module_xml_v2_module_get_attributive_node (name, "need-files")) && node->svalue) {
  char **files = str2set (':', node->svalue);

  if (files) {
   int i = 0;
   struct stat st;
   for (; files[i]; i++) {
    if (files[i][0] == '/') {
     if (stat (files[i], &st)) {
      free (files);
      return status_failed;
     }
    } else {
     char **w = which (files[i]);
     if (!w) {
      free (files);
      return status_failed;
     } else {
      free (w);
     }
    }
   }

   free (files);
  }
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "environment")) && node->arbattrs) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   myenvironment = straddtoenviron (myenvironment, node->arbattrs[i], node->arbattrs[i+1]);
  }
 }

 if ((node = module_xml_v2_module_get_node (name, action))) {
  int x = 0;
  char *code = NULL, *user = NULL, *group = NULL;

  for (; node->arbattrs[x]; x+=2) {
   if (strmatch (node->arbattrs[x], "code")) code = node->arbattrs[x+1];
   else if (strmatch (node->arbattrs[x], "user")) user = node->arbattrs[x+1];
   else if (strmatch (node->arbattrs[x], "group")) group = node->arbattrs[x+1];
  }

  if (code) {
   struct cfgnode *vnode = module_xml_v2_module_get_attributive_node (name, "variables");
   char *variables = vnode ? vnode->svalue : NULL; 

   if (variables) {
    char **split_variables;

    split_variables = str2set (':', variables);

    returnvalue = pexec (code, (const char **)split_variables, 0, 0, user, group, myenvironment, status);

    free (split_variables);
   } else
	returnvalue = pexec (code, NULL, 0, 0, user, group, myenvironment, status);
  } else
   returnvalue = status_failed;
 } else if (module_xml_v2_module_have_script_action (name, action)) {
  struct cfgnode * t = module_xml_v2_module_get_attributive_node (name, "script");

  if (t && t->arbattrs) {
   int i = 0;
   char *scriptpath = NULL, *user = NULL, *group = NULL;
   char **variables = NULL;

   for (; t->arbattrs[i]; i+=2) {
    if (strmatch (t->arbattrs[i], "file")) {

     if ((t->arbattrs[i+1][0] != '/') && t->source_file) {
      char spbuffer[BUFFERSIZE];
      char spbuffer2[BUFFERSIZE];

      strncpy (spbuffer2, t->source_file, BUFFERSIZE);

      char *scriptbase = dirname (spbuffer2);

      esprintf (spbuffer, BUFFERSIZE, "%s/%s", scriptbase, t->arbattrs[i+1]);

      scriptpath = estrdup (spbuffer);
     } else 
      scriptpath = estrdup (t->arbattrs[i+1]);
    } else if (strmatch (t->arbattrs[i], "user")) {
     user = estrdup (t->arbattrs[i+1]);
	} else if (strmatch (t->arbattrs[i], "group")) {
     group = estrdup (t->arbattrs[i+1]);
	}
   }

   if ((node = module_xml_v2_module_get_attributive_node (name, "variables")) && node->svalue) {
    variables = str2set (':', node->svalue);
   }

   if (scriptpath) {
    ssize_t nclen = strlen (scriptpath) + strlen (action) + 2; /* one for a space and one for \0 */
    char *ncommand = emalloc (nclen);

    esprintf (ncommand, nclen, "%s %s", scriptpath, action);

    returnvalue = pexec (ncommand, (const char **)variables, 0, 0, user, group, myenvironment, status);

    free (ncommand);

    free (scriptpath);
   }

   if (variables) free (variables);
   if (group) free (group);
   if (user) free (user);
  }
 }

 if (myenvironment) free (myenvironment);

 return returnvalue;
}

struct dexecinfo *module_xml_v2_module_get_daemon_action (char *name) {
 struct cfgnode *node = module_xml_v2_module_get_node (name, "daemon");
 struct stree *cnode = module_xml_v2_daemons ? streefind (module_xml_v2_daemons, name, tree_find_first) : NULL;
 struct dexecinfo *dx = cnode ? cnode->value : NULL;

 if (!cnode) {
  dx = ecalloc (1, sizeof (struct dexecinfo));

  module_xml_v2_daemons = streeadd (module_xml_v2_daemons, name, dx, SET_NOALLOC, NULL);
 }

 if (node) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "code")) {
    if (dx->command) free (dx->command);
    dx->command = estrdup(node->arbattrs[i+1]);
   } else if (strmatch (node->arbattrs[i], "user")) {
    if (dx->user) free (dx->user);
    dx->user = estrdup(node->arbattrs[i+1]);
   } else if (strmatch (node->arbattrs[i], "group")) {
    if (dx->group) free (dx->group);
    dx->group = estrdup (node->arbattrs[i+1]);
   }
  }
 }

 if ((node = module_xml_v2_module_get_node (name, "prepare"))) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "code")) {
    if (dx->prepare) free (dx->prepare);
    dx->prepare = estrdup(node->arbattrs[i+1]);
   }
  }
 }

 if ((node = module_xml_v2_module_get_node (name, "cleanup"))) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "code")) {
    if (dx->cleanup) free (dx->cleanup);
    dx->cleanup = estrdup(node->arbattrs[i+1]);
   }
  }
 }

 if ((node = module_xml_v2_module_get_node (name, "is-up"))) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "code")) {
    if (dx->is_up) free (dx->is_up);
    dx->is_up = estrdup(node->arbattrs[i+1]);
   }
  }
 }

 if ((node = module_xml_v2_module_get_node (name, "is-down"))) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "code")) {
    if (dx->is_down) free (dx->is_down);
    dx->is_down = estrdup(node->arbattrs[i+1]);
   }
  }
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "restart"))) {
  dx->restart = node->flag;
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "pidfile")) && node->svalue) {
  if (dx->pidfile) free (dx->pidfile);
  dx->pidfile = estrdup (node->svalue);
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "variables")) && node->svalue) {
  if (dx->variables) free (dx->variables);
  dx->variables = str2set (':', node->svalue);
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "need-files")) && node->svalue) {
  if (dx->need_files) free (dx->need_files);
  dx->need_files = str2set (':', node->svalue);
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "environment")) && node->arbattrs) {
  int i = 0;

  if (dx->environment) {
   free (dx->environment);
   dx->environment = NULL;
  }

  for (; node->arbattrs[i]; i+=2) {
   dx->environment = straddtoenviron (dx->environment, node->arbattrs[i], node->arbattrs[i+1]);
  }
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "script")) && node->arbattrs) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "file")) {
    if (dx->script) free (dx->script);
    dx->script = estrdup (node->arbattrs[i+1]);
   } else if (strmatch (node->arbattrs[i], "actions")) {
    if (dx->script_actions) free (dx->script_actions);
    dx->script_actions = str2set (':', node->arbattrs[i+1]);
   }
  }
 }

 if ((node = module_xml_v2_module_get_attributive_node (name, "options")) && node->svalue) {
  char **opt = str2set (':', node->svalue);
  uint32_t ri = 0;

  for (; opt[ri]; ri++) {
   if (strmatch (opt[ri], "forking"))
    dx->options |= daemon_model_forking;
  }

  free (opt);
 }

 return dx;
}

int module_xml_v2_module_enable (char *name, struct einit_event *status) {
 if (module_xml_v2_module_have_action (name, "daemon")) {
/* daemon code here */
  struct dexecinfo *dexec = module_xml_v2_module_get_daemon_action (name);

  if (dexec) {
   return startdaemon (dexec, status);
  }
 } else {
  struct cfgnode *node = NULL;
  if ((node = module_xml_v2_module_get_attributive_node (name, "pidfile")) && node->svalue) {
   unlink (node->svalue);
  }

  if (module_xml_v2_module_have_action (name, "prepare") &&
      (module_xml_v2_module_custom_action (name, "prepare", status) == status_failed)) {
   return status_failed;
  }

  return module_xml_v2_module_custom_action (name, "enable", status);
 }

 return status_failed;
}

int module_xml_v2_module_disable (char *name, struct einit_event *status) {
 if (module_xml_v2_module_have_action (name, "daemon")) {
/* daemon code here */
  struct dexecinfo *dexec = module_xml_v2_module_get_daemon_action (name);

  if (dexec) {
   return stopdaemon (dexec, status);
  }
 } else {
  if (module_xml_v2_module_custom_action (name, "disable", status) == status_ok) {
   struct cfgnode *node = NULL;

   if (module_xml_v2_module_have_action (name, "cleanup") &&
       (module_xml_v2_module_custom_action (name, "cleanup", status) == status_failed)) {
    return status_failed;
   }

   if ((node = module_xml_v2_module_get_attributive_node (name, "pidfile")) && node->svalue) {
    unlink (node->svalue);
   }

   return status_ok;
  }
 }

 return status_failed;
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
/*    notice (1, "processing id=%s", cur->key + MODULES_PREFIX_LENGTH); */

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

      if ((node = module_xml_v2_module_get_attributive_node (new_sm->rid, "options")) && node->svalue) {
       char **opt = str2set (':', node->svalue);
       uint32_t ri = 0;

       for (; opt[ri]; ri++) {
        if (strmatch (opt[ri], "feedback"))
         new_sm->mode |= einit_module_feedback;
        else if (strmatch (opt[ri], "deprecated"))
         new_sm->mode |= einit_module_deprecated;
       }

       free (opt);
      }

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

void module_xml_v2_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_scheduled) || (ev->type == einit_power_reset_scheduled)) {
  struct stree *modules = module_xml_v2_modules;

  while (modules) {
   if (module_xml_v2_module_have_action (modules->key, "on-shutdown")) {
#if 0
    struct einit_event status = evstaticinit (einit_feedback_module_status);
	status.integer = ((struct lmodule *)(modules->value))->fbseq+1;

    module_xml_v2_module_custom_action (modules->key, "on-shutdown", &status);

    evstaticdestroy (status);
#endif
    mod (einit_module_custom, modules->value, "on-shutdown");
   }
   modules = streenext (modules);
  }
 }
}

void module_xml_v2_preload() {
 struct stree *s = module_xml_v2_modules;
 struct cfgnode *node;

 while (s) {
  if ((node = module_xml_v2_module_get_attributive_node (s->key, "need-files")) && node->svalue) {
   char **files = str2set (':', node->svalue);

   if (files && !fork()) {
    int i = 0;
    void *dh;

    notice (4, "pre-loading files for module %s.", s->key);

    for (; files[i]; i++) {
     if (files[i][0] == '/') {
      if ((dh = dlopen (files[i], RTLD_NOW))) {
       dlclose (dh);
      }
     } else {
      char **w = which (files[i]);
      if (w) {
       int n = 0;

       for (; w[n]; n++) {
        if ((dh = dlopen (w[n], RTLD_NOW))) {
         dlclose (dh);
        }
       }
       free (w);
      }
     }
    }

    free (files);
    _exit (EXIT_SUCCESS);
   }

   if (files) free (files);
  }

  s = streenext (s);
 }
}

void module_xml_v2_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   module_xml_v2_preload();
   break;
  default: break;
 }
}

int module_xml_v2_cleanup (struct lmodule *pa) {
 exec_cleanup (pa);

 event_ignore (einit_event_subsystem_power, module_xml_v2_power_event_handler);
 event_ignore (einit_event_subsystem_boot, module_xml_v2_boot_event_handler);

 return 0;
}

int module_xml_v2_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 pa->scanmodules = module_xml_v2_scanmodules;
 pa->cleanup = module_xml_v2_cleanup;

 event_listen (einit_event_subsystem_power, module_xml_v2_power_event_handler);
 event_listen (einit_event_subsystem_boot, module_xml_v2_boot_event_handler);

 return 0;
}

