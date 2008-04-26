/*
 *  module-xml.c
 *  einit
 *
 *  Created by Magnus Deininger on 16/10/2007.
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
#include <libgen.h>

#include <dlfcn.h>
#include <sys/wait.h>

#include <einit/exec.h>
#include <einit/process.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif


#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_xml_v2_configure (struct lmodule *);

const struct smodule module_xml_v2_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
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

#define MODULES_PREFIX "services-virtual-module-"
#define MODULES_PREFIX_LENGTH (sizeof(MODULES_PREFIX) -1)

#define MODULES_EXECUTE_NODE_TEMPLATE MODULES_PREFIX "%s-execute"
#define MODULES_ARBITRARY_NODE_TEMPLATE MODULES_PREFIX "%s-%s"

struct stree *module_xml_v2_modules = NULL;

struct cfgnode *module_xml_v2_module_get_node (char *name, char *action) {
 if (name && action) {
  char buffer[BUFFERSIZE];

  esprintf (buffer, BUFFERSIZE, MODULES_EXECUTE_NODE_TEMPLATE, name);

  struct stree *st = cfg_match (buffer);
  struct stree *cur = streelinear_prepare(st);
  if (st) {
      while (cur) {
          struct cfgnode *node = cur->value;

          if (node->idattr && strmatch (node->idattr, action)) {
              return node;
          }

          cur = streenext(cur);
      }
      streefree(st);
  }
 }

 return NULL;
}

struct cfgnode *module_xml_v2_module_get_attributive_node (char *name, char *attribute) {
 if (name && attribute) {
  char buffer[BUFFERSIZE];

  esprintf (buffer, BUFFERSIZE, MODULES_ARBITRARY_NODE_TEMPLATE, name, attribute);

  return cfg_getnode (buffer);
 }

 return NULL;
}

char module_xml_v2_module_have_action (char *name, char *action) {
 if (module_xml_v2_module_get_node (name, action)) {
  return 1;
 }

 return 0;
}

char module_xml_v2_check_files (char *name) {
 struct cfgnode *node = NULL;

 if ((node = module_xml_v2_module_get_attributive_node (name, "need-files")) && node->svalue) {
  char **files = str2set (':', node->svalue);

  if (files) {
   char rv = check_files(files);

   efree (files);
   return rv;
  }
 }

 return 1;
}

int module_xml_v2_module_configure (struct lmodule *pa) {
 module_xml_v2_modules = streeadd (module_xml_v2_modules, pa->module->rid, pa, SET_NOALLOC, NULL);

 return 0;
}

char **module_xml_v2_add_fs (char **xt, char *s) {
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

char *module_xml_v2_generate_defer_fs (char **tmpxt) {
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

void module_xml_v2_scanmodules (struct einit_event *ev) {
 struct stree *modules_to_update = streelinear_prepare(module_xml_v2_modules);
 int new_modules = 0;

 while (modules_to_update) {
  mod_update (modules_to_update->value);

  modules_to_update = streenext (modules_to_update);
 }

 struct stree *module_nodes = cfg_prefix(MODULES_PREFIX);

 if (module_nodes) {
  struct stree *cur = streelinear_prepare(module_nodes);

  for (; cur; cur = streenext (cur)) {
    struct cfgnode *node = cur->value;
/*    notice (1, "processing id=%s", cur->key + MODULES_PREFIX_LENGTH); */

    if ((!module_xml_v2_modules || !streefind (module_xml_v2_modules, cur->key + MODULES_PREFIX_LENGTH, tree_find_first)) && node->arbattrs) {
     int i = 0;
     char *name = NULL, *requires = NULL, *provides = NULL, **after = NULL, *before = NULL, **fs = NULL;
     struct cfgnode *xnode;

     for (; node->arbattrs[i]; i+=2) {
      if (strmatch (node->arbattrs[i], "name")) {
       name = node->arbattrs[i+1];
      } else if (strmatch (node->arbattrs[i], "provides")) {
       provides = node->arbattrs[i+1];
      } else if (strmatch (node->arbattrs[i], "requires")) {
       requires = node->arbattrs[i+1];
      } else if (strmatch (node->arbattrs[i], "after")) {
       after = str2set (':', node->arbattrs[i+1]);
      } else if (strmatch (node->arbattrs[i], "before")) {
       before = node->arbattrs[i+1];
      }
     }

     if ((xnode = module_xml_v2_module_get_attributive_node (cur->key + MODULES_PREFIX_LENGTH, "pidfile")) && xnode->svalue) {
      fs = module_xml_v2_add_fs(fs, xnode->svalue);
     }

     if ((xnode = module_xml_v2_module_get_attributive_node (cur->key + MODULES_PREFIX_LENGTH, "need-files")) && xnode->svalue) {
      char **sx = str2set (':', xnode->svalue);
      int ix = 0;

      for (; sx[ix]; ix++) {
       if (sx[ix][0] == '/') {
        fs = module_xml_v2_add_fs(fs, sx[ix]);
       }
      }

      efree (sx);
     }

     if (fs) {
      char *a = module_xml_v2_generate_defer_fs(fs);

      if (a) {
       after = set_str_add (after, a);

       efree (a);
      }
     }

     if (name && provides && module_xml_v2_check_files (cur->key + MODULES_PREFIX_LENGTH)) {
      struct smodule *new_sm = emalloc (sizeof (struct smodule));
      memset (new_sm, 0, sizeof (struct smodule));

      new_modules++;

      new_sm->rid = (char *)str_stabilise (cur->key + MODULES_PREFIX_LENGTH);
      new_sm->name = (char *)str_stabilise (name);

      new_sm->eiversion = EINIT_VERSION;
      new_sm->eibuild = BUILDNUMBER;
      new_sm->version = 1;
      new_sm->mode = einit_module | einit_module_event_actions;

      new_sm->si.provides = str2set (':', provides);
      if (requires) new_sm->si.requires = str2set (':', requires);
      if (after) new_sm->si.after = after;
      if (before) new_sm->si.before = str2set (':', before);

      new_sm->configure = module_xml_v2_module_configure;

      if ((node = module_xml_v2_module_get_attributive_node (new_sm->rid, "options")) && node->svalue) {
       char **opt = str2set (':', node->svalue);
       uint32_t ri = 0;

       for (; opt[ri]; ri++) {
        if (strmatch (opt[ri], "deprecated"))
         new_sm->mode |= einit_module_deprecated;
        else if (strmatch (opt[ri], "run-once"))
         new_sm->mode |= einit_feedback_job;
       }

       efree (opt);
      }

      mod_add (NULL, new_sm);

      continue;
     }
    }

  }

  streefree (module_nodes);
 }

 return;
}


void module_xml_v2_auto_enable (char *mode) {
 if (!mode) return;

 char **automod = NULL;

 struct stree *modules = streelinear_prepare(module_xml_v2_modules);

 while (modules) {
  struct cfgnode *s = module_xml_v2_module_get_attributive_node(modules->key, "auto-enable");
  if (s && s->svalue) {
   char **sp = str2set (':', s->svalue);

   if (sp) {
    if (inset ((const void **)sp, mode, SET_TYPE_STRING)) {
     automod = set_str_add (automod, modules->key);
    }

    efree (sp);
   }
  }

  modules = streenext (modules);
 }

 if (automod) {
  struct einit_event eml = evstaticinit(einit_core_manipulate_services);
  eml.stringset = automod;
  eml.task = einit_module_enable;

  event_emit (&eml, 0);
  evstaticdestroy(eml);

  efree (automod);
 }
}

void module_xml_v2_core_event_handler_mode_switching (struct einit_event *ev) {
 if (ev->para)
  module_xml_v2_auto_enable (((struct cfgnode *)ev->para)->id);
}

void module_xml_v2_power_event_handler (struct einit_event *ev) {
 struct stree *modules = streelinear_prepare(module_xml_v2_modules);

 while (modules) {
  if (module_xml_v2_module_have_action (modules->key, "on-shutdown")) {
   struct lmodule *mo = modules->value;
   if (mo && (mo->status & status_enabled))
    mod (einit_module_custom, mo, "on-shutdown");
  }
  modules = streenext (modules);
 }
}

void module_xml_v2_core_event_handler_action_execute_step (char *task, char *rid);

int module_xml_v2_core_event_handler_status(int status) {
 if (WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS)) return status_ok;

 return status_failed;
}

void module_xml_v2_core_event_handler_action_false (char *task, char *rid) {
/* if (strmatch (task, "enable")) {
  mod_complete (rid, einit_module_enable, status_disabled | status_failed);
 } else if (strmatch (task, "cleanup")) {
  mod_complete (rid, einit_module_enable, status_enabled | status_failed);
 } else {*/
  struct lmodule *lm = mod_lookup_rid (rid);

  mod_complete (rid, einit_module_custom, ((lm->status & (~status_ok)) | status_failed));
// }
}

void module_xml_v2_core_event_handler_action_true (char *task, char *rid) {
 if (strmatch (task, "prepare")) {
  module_xml_v2_core_event_handler_action_execute_step ("enable", rid);
 } else if (strmatch (task, "disable")) {
  module_xml_v2_core_event_handler_action_execute_step ("cleanup", rid);
 } else if (strmatch (task, "enable")) {
  if (module_xml_v2_module_have_action (rid, "daemon")) {
   struct lmodule *lm;
   struct cfgnode *node;

   if ((lm = mod_lookup_rid (rid)) && (node = module_xml_v2_module_get_attributive_node (rid, "pidfile")) && node->svalue) {
    lm->pidfile = node->svalue;
   }

   module_xml_v2_core_event_handler_action_execute_step ("is-up", rid);
   return;
  }

  mod_complete (rid, einit_module_enable, status_enabled | status_ok);
 } else if (strmatch (task, "is-up")) {
  mod_complete (rid, einit_module_enable, status_enabled | status_ok);
 } else if (strmatch (task, "cleanup")) {
  mod_complete (rid, einit_module_disable, status_disabled | status_ok);
 } else {
  struct lmodule *lm = mod_lookup_rid (rid);

  mod_complete (rid, einit_module_custom, ((lm->status & (~status_failed)) | status_ok));
 }
}

void module_xml_v2_core_event_handler_dead_process(struct einit_exec_data *x) {
 char *a = x->custom;

// fprintf (stderr, "module_xml_v2_core_event_handler_dead_process(%s, %s, %i)\n", x->custom, x->rid, x->pid);

 if (module_xml_v2_core_event_handler_status (x->status) & status_ok)
  module_xml_v2_core_event_handler_action_true (a, x->rid);
 else
  module_xml_v2_core_event_handler_action_false (a, x->rid);
}

void module_xml_v2_core_event_handler_action_execute_step (char *task, char *rid) {
 struct cfgnode *node = NULL;
 char **myenvironment = NULL;
 char daemonise = 0;

// fprintf (stderr, "module_xml_v2_core_event_handler_action_execute_step(%s, %s)\n", task, rid);

 if (!module_xml_v2_check_files (rid)) {
  if ((node = module_xml_v2_module_get_node (rid, task))) {
   int x = 0;

   for (; node->arbattrs[x]; x+=2) {
    if (strmatch (node->arbattrs[x], "code")) {
     if (strmatch (node->arbattrs[x+1], "true")) {
      goto handle_nocode;
     }
    }
   }

   module_xml_v2_core_event_handler_action_false (task, rid);
   return;
  }
 }

 if ((node = module_xml_v2_module_get_attributive_node (rid, "environment")) && node->arbattrs) {
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   myenvironment = straddtoenviron (myenvironment, node->arbattrs[i], node->arbattrs[i+1]);
  }
 }

 char *pidfile = NULL;;

 if ((node = module_xml_v2_module_get_attributive_node (rid, "pidfile")) && node->svalue) {
  pidfile = node->svalue;
 }

 if (strmatch (task, "enable") && pidfile) {
  struct stat st;

  if (!stat (pidfile, &st)) { /* unlink the pidfile if it exists */
   unlink (pidfile);
  }
 }

 node = NULL;

 if (module_xml_v2_module_have_action (rid, "daemon")) {
  if (strmatch(task, "enable")) {
   daemonise = 1;

   if ((node = module_xml_v2_module_get_attributive_node (rid, "options")) && node->svalue) {
    char **opt = str2set (':', node->svalue);
    uint32_t ri = 0;

    for (; opt[ri]; ri++) {
     if (strmatch (opt[ri], "forking"))
      daemonise = 0;
    }

    efree (opt);
   }

   node = module_xml_v2_module_get_node (rid, "daemon");
  } else if (strmatch(task, "disable")) {
   pid_t pid = 0;

   char *pidbuffer = NULL;
   if (pidfile) {
    struct stat st;

    if (stat (pidfile, &st) || !(pidbuffer = readfile (pidfile))) { /* just return OK if the pidfile doesn't exist */
     goto handle_nocode;
    }
   }

   if (pidbuffer) {
    pid = parse_integer (pidbuffer);
    efree (pidbuffer);
   }

   struct lmodule *lm = mod_lookup_rid (rid);

   if (!pid) { /* if we don't have a pid yet, try to find one in the module's descriptor */
    if (!lm) { /* uh, wtf!? */
     goto handle_nocode;
    }

    pid = lm->pid;
   }

   if (!pid) { /* if we still don't have a pid, something's fucked up good... */
    goto handle_nocode;
   }

   if (!process_alive_p(pid)) { /* goody the pid's dead already, go scram */
    goto handle_nocode;
   }

   lm->pid = pid; /* make sure the pid is set for the module */
   lm->status |= status_death_pending;

   if (kill (pid, SIGTERM)) { /* looks like we weren't allowed to SIGKILL this bugger... ah, whatever then */
    goto handle_nocode;
   }

   return; /* okay, we return now so we get informed of this bugger's death later */
  }
 }

 if (node || (node = module_xml_v2_module_get_node (rid, task))) {
  int x = 0;
  char *code = NULL, *user = NULL, *group = NULL, *options = NULL;

  for (; node->arbattrs[x]; x+=2) {
   if (strmatch (node->arbattrs[x], "code")) code = node->arbattrs[x+1];
   else if (strmatch (node->arbattrs[x], "user")) user = node->arbattrs[x+1];
   else if (strmatch (node->arbattrs[x], "group")) group = node->arbattrs[x+1];
   else if (strmatch (node->arbattrs[x], "options")) options = node->arbattrs[x+1];
  }

  if (code) {
   if (strmatch (code, "true")) {
//    fprintf (stderr, "code is a plain call to true (%s, %s)\n", task, rid);
    goto handle_nocode;
   } else if (strmatch (code, "false")) {
//    fprintf (stderr, "code is a plain call to false (%s, %s)\n", task, rid);
    module_xml_v2_core_event_handler_action_false (task, rid);

    return;
   }

   struct einit_exec_data * xd = einit_exec_create_exec_data_from_string (code);
   xd->rid = rid;
   xd->module = mod_lookup_rid (rid);
   xd->custom = (void *)str_stabilise (task);
   xd->handle_dead_process = module_xml_v2_core_event_handler_dead_process;

   struct cfgnode *vnode = module_xml_v2_module_get_attributive_node (rid, "variables");
   char *variables = vnode ? vnode->svalue : NULL; 

   if (variables) {
    char **split_variables;

    split_variables = str2set (':', variables);

    xd->variables = set_str_dup_stable(split_variables);

    efree (split_variables);
   }

   if (options) {
    char **opts = str2set (':', options);

    if (inset ((const void **)opts, "keep-stdin", SET_TYPE_STRING)) {
     xd->options |= einit_exec_keep_stdin;
    }

    if (inset ((const void **)opts, "no-pipe", SET_TYPE_STRING)) {
     xd->options |= einit_exec_no_pipe;
    }

    efree (opts);
   }

   if (daemonise)
    xd->options |= einit_exec_daemonise;

//   fprintf (stderr, "executing code (%s, %s)\n", task, rid);
   einit_exec (xd);
//   fprintf (stderr, "done executing code (%s, %s)\n", task, rid);
   return;
  }
 } else {
//  fprintf (stderr, "no code (%s, %s)\n", task, rid);
 }

 handle_nocode: /* if we dont have code, just assume everything went goody */
// fprintf (stderr, "no code/true by default (%s, %s)\n", task, rid);

 if (myenvironment) efree (myenvironment);

 module_xml_v2_core_event_handler_action_true (task, rid);
}

void module_xml_v2_core_event_handler_action_execute (struct einit_event *ev) {
 if (!ev->rid || !ev->string) return;

 if (module_xml_v2_modules && streefind (module_xml_v2_modules, ev->rid, tree_find_first)) {
  if (strmatch (ev->string, "enable"))
   module_xml_v2_core_event_handler_action_execute_step ("prepare", ev->rid);
  else
   module_xml_v2_core_event_handler_action_execute_step (ev->string, ev->rid);
 }
}

void module_xml_v2_einit_process_died (struct einit_event *ev) {
// fprintf (stderr, "pid has died: %i, %s\n", ev->integer, ev->rid);

 if (ev->rid && module_xml_v2_modules && streefind (module_xml_v2_modules, ev->rid, tree_find_first)) {
  struct lmodule *lm = mod_lookup_rid (ev->rid);

  if (!lm) { /* err...? */
   return;
  }

  if (lm->status & status_death_pending) { /* finish the daemon up */
   lm->status &= ~status_death_pending;
   module_xml_v2_core_event_handler_action_execute_step ("cleanup", ev->rid);
  } else { /* not supposed to die just yet, figure out if we should respawn it */
   struct cfgnode *node;

   if ((node = module_xml_v2_module_get_attributive_node (ev->rid, "restart")) && node->flag) {
    /* do respawn */
    module_xml_v2_core_event_handler_action_execute_step ("enable", ev->rid);
   } else {
    /* don't respawn */
    module_xml_v2_core_event_handler_action_execute_step ("cleanup", ev->rid);
   }
  }
 }
}

int module_xml_v2_configure (struct lmodule *pa) {
 module_init (pa);

 event_listen (einit_core_update_modules, module_xml_v2_scanmodules);

 event_listen (einit_power_reset_scheduled, module_xml_v2_power_event_handler);
 event_listen (einit_power_down_scheduled, module_xml_v2_power_event_handler);
 event_listen (einit_core_mode_switching, module_xml_v2_core_event_handler_mode_switching);

 event_listen (einit_core_module_action_execute, module_xml_v2_core_event_handler_action_execute);

 event_listen (einit_process_died, module_xml_v2_einit_process_died);

 module_xml_v2_scanmodules(NULL);

 return 0;
}
