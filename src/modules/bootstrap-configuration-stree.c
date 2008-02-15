/*
 *  bootstrap-configuration-stree.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Split from config-xml-expat.c on 22/10/2006
 *  Renamed/moved from config.c on 20/03/2007
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/event.h>
#include <einit-modules/ipc.h>

#include <regex.h>

int bootstrap_einit_configuration_stree_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule bootstrap_einit_configuration_stree_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "Core Configuration Storage and Retrieval (stree-based)",
 .rid       = "einit-bootstrap-configuration-stree",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = bootstrap_einit_configuration_stree_configure
};

module_register(bootstrap_einit_configuration_stree_self);

#endif

struct {
 void **chunks;
} cfg_stree_garbage = {
 .chunks = NULL
};

struct stree *hconfiguration = NULL;

pthread_mutex_t cfg_stree_garbage_mutex = PTHREAD_MUTEX_INITIALIZER;

void cfg_stree_garbage_add_chunk (void *chunk) {
 if (!chunk) return;
 emutex_lock (&cfg_stree_garbage_mutex);
 if (!cfg_stree_garbage.chunks || (!inset ((const void **)cfg_stree_garbage.chunks, chunk, SET_NOALLOC)))
  cfg_stree_garbage.chunks = set_noa_add (cfg_stree_garbage.chunks, chunk);
 emutex_unlock (&cfg_stree_garbage_mutex);
}

void cfg_stree_garbage_free () {
 emutex_lock (&cfg_stree_garbage_mutex);
 if (cfg_stree_garbage.chunks) {
  int i = 0;

  for (; cfg_stree_garbage.chunks[i]; i++) {
   efree (cfg_stree_garbage.chunks[i]);
  }

  efree (cfg_stree_garbage.chunks);
  cfg_stree_garbage.chunks = NULL;
 }
 emutex_unlock (&cfg_stree_garbage_mutex);
}

time_t bootstrap_einit_configuration_stree_garbage_free_timer = 0;

void bootstrap_einit_configuration_stree_einit_event_handler_timer_tick(struct einit_event *ev) {
 if (ev->integer == bootstrap_einit_configuration_stree_garbage_free_timer) {
//  cfg_stree_garbage_free();
  ethread_prune_thread_pool();
 }
}

void bootstrap_einit_configuration_stree_einit_event_handler_core_done_switching(struct einit_event *ev) {
 if (bootstrap_einit_configuration_stree_garbage_free_timer) {
  event_timer_cancel (bootstrap_einit_configuration_stree_garbage_free_timer);
 }
 bootstrap_einit_configuration_stree_garbage_free_timer = event_timer_register_timeout (20);
}

int cfg_free () {
 struct stree *cur = streelinear_prepare(hconfiguration);
 struct cfgnode *node = NULL;
 while (cur) {
  if ((node = (struct cfgnode *)cur->value)) {
   if (node->id)
    efree (node->id);
  }
  cur = streenext (cur);
 }
 streefree (hconfiguration);
 hconfiguration = NULL;

 return 1;
}

#include <regex.h>

regex_t cfg_storage_allowed_duplicates;

int cfg_addnode_f (struct cfgnode *node) {
 if (!node || !node->id) {
  return -1;
 }

 if (strmatch (node->id, "core-settings-configuration-multi-node-variables")) {
  if (!node->arbattrs) {
/* without arbattrs, this node is invalid */
   return -1;
  } else {
   int i = 0;
   for (; node->arbattrs[i]; i+=2) {
    if (strmatch (node->arbattrs[i], "allow")) {
//     fprintf (stderr, " ** new: %s\n", node->arbattrs[i+1]);
//     fflush (stderr);

//     sleep (1);

     eregfree (&cfg_storage_allowed_duplicates);
     if (eregcomp (&cfg_storage_allowed_duplicates, node->arbattrs[i+1])) {
//      fprintf (stderr, " ** backup: .*\n");
//      fflush (stderr);

//      sleep (1);

      eregcomp (&cfg_storage_allowed_duplicates, ".*");
     }

//     fprintf (stderr, " ** done\n");
//     fflush (stderr);
    }
   }
  }
 }

 struct stree *cur = hconfiguration;
 char doop = 1;

 if (node->arbattrs) {
  uint32_t r = 0;
  for (; node->arbattrs[r]; r+=2) {
   if (strmatch ("id", node->arbattrs[r])) node->idattr = node->arbattrs[r+1];
  }
 }

 if (node->type & einit_node_mode) {
/* mode definitions only need to be modified -- it doesn't matter if there's more than one, but
  only the first one would be used anyway. */
  if (cur) cur = streefind (cur, node->id, tree_find_first);
  while (cur) {
   if (cur->value && !(((struct cfgnode *)cur->value)->type ^ einit_node_mode)) {
// this means we found something that looks like it
    void *bsl = cur->luggage;

// we risk not being atomic at this point but... it really is unlikely to go weird.
    ((struct cfgnode *)cur->value)->arbattrs = node->arbattrs;
    cur->luggage = node->arbattrs;

    efree (bsl);

    doop = 0;

    break;
   }
//   cur = streenext (cur);
   cur = streefind (cur, node->id, tree_find_next);
  }
 } else {
/* look for other definitions that are exactly the same, only marginally different or that sport a
   matching id="" attribute */

  if (cur) cur = streefind (cur, node->id, tree_find_first);
  while (cur) {
// this means we found a node wit the same path
   char allow_multi = 0;
   char id_match = 0;

   if ((((struct cfgnode *)cur->value)->mode != node->mode)) {
    cur = streefind (cur, node->id, tree_find_next);
    continue;
   }

//   fprintf (stderr, " ** multicheck: %s*\n", node->id);
   if (regexec (&cfg_storage_allowed_duplicates, node->id, 0, NULL, 0) != REG_NOMATCH) {
//    fprintf (stderr, "allow multi: %s; %i %i %i\n", node->id, allow_multi, node->idattr ? 1 : 0, id_match);
    allow_multi = 1;
   }/* else {
    fprintf (stderr, " ** not multi*\n");
   }
   fflush (stderr);*/

   if (cur->value && ((struct cfgnode *)cur->value)->idattr && node->idattr &&
       strmatch (((struct cfgnode *)cur->value)->idattr, node->idattr)) {
    id_match = 1;
   }

   if (((!allow_multi && (!node->idattr)) || id_match)) {
// this means we found something that looks like it
//    fprintf (stderr, "replacing old config: %s; %i %i %i\n", node->id, allow_multi, node->idattr ? 1 : 0, id_match);
//    fflush (stderr);

    cfg_stree_garbage_add_chunk (cur->luggage);
    cfg_stree_garbage_add_chunk (((struct cfgnode *)cur->value)->arbattrs);

    ((struct cfgnode *)cur->value)->arbattrs = node->arbattrs;
    cur->luggage = node->arbattrs;

    ((struct cfgnode *)cur->value)->type        = node->type;
    ((struct cfgnode *)cur->value)->mode        = node->mode;
    ((struct cfgnode *)cur->value)->flag        = node->flag;
    ((struct cfgnode *)cur->value)->value       = node->value;
    ((struct cfgnode *)cur->value)->svalue      = node->svalue;
    ((struct cfgnode *)cur->value)->idattr      = node->idattr;

    doop = 0;

    break;
   } else

//   if (allow_multi || node->idattr) {
//   cur = streenext (cur);
    cur = streefind (cur, node->id, tree_find_next);
//   }
  }
 }

 if (doop) {
  hconfiguration = streeadd (hconfiguration, node->id, node, sizeof(struct cfgnode), node->arbattrs);

  einit_new_node = 1;
 }

/* hmmm.... */
/* cfg_stree_garbage_add_chunk (node->arbattrs);*/
 cfg_stree_garbage_add_chunk (node->id);
 return 0;
}

struct cfgnode *cfg_findnode_f (const char *id, enum einit_cfg_node_options type, const struct cfgnode *base) {
 struct stree *cur = hconfiguration;
 if (!id) {
  return NULL;
 }

 if (base) {
  if (cur) cur = streefind (cur, id, tree_find_first);
  while (cur) {
   if (cur->value == base) {
    cur = streefind (cur, id, tree_find_next);
    break;
   }
//   cur = streenext (cur);
    cur = streefind (cur, id, tree_find_next);
  }
 } else if (cur) {
  cur = streefind (cur, id, tree_find_first);
 }

 while (cur) {
  if (cur->value && (!type || !(((struct cfgnode *)cur->value)->type ^ type))) {
   return cur->value;
  }
  cur = streefind (cur, id, tree_find_next);
 }
 return NULL;
}

// get string (by id)
char *cfg_getstring_f (const char *id, const struct cfgnode *mode) {
 struct cfgnode *node = NULL;
 char *ret = NULL, **sub;
 uint32_t i;

 if (!id) {
  return NULL;
 }
 mode = mode ? mode : cmode;

 if (strchr (id, '/')) {
  char f = 0;
  sub = str2set ('/', id);
  if (!sub[1]) {
   node = cfg_getnode (id, mode);
   if (node)
    ret = node->svalue;

   efree (sub);
   return ret;
  }

  node = cfg_getnode (sub[0], mode);
  if (node && node->arbattrs && node->arbattrs[0]) {
   if (node->arbattrs)

   for (i = 0; node->arbattrs[i]; i+=2) {
    if ((f = (strmatch(node->arbattrs[i], sub[1])))) {
     ret = node->arbattrs[i+1];
     break;
    }
   }
  }

  efree (sub);
 } else {
  node = cfg_getnode (id, mode);
  if (node)
   ret = node->svalue;
 }

 return ret;
}

// get node (by id)
struct cfgnode *cfg_getnode_f (const char *id, const struct cfgnode *mode) {
 struct cfgnode *node = NULL;
 struct cfgnode *ret = NULL;

 if (!id) {
  return NULL;
 }
 mode = mode ? mode : cmode;

 if (mode) {
  char *tmpnodename = NULL;
  tmpnodename = emalloc (6+strlen (id));
  *tmpnodename = 0;

  strcat (tmpnodename, "mode-");
  strcat (tmpnodename, id);

  while ((node = cfg_findnode (tmpnodename, 0, node))) {
   if (node->mode == mode) {
    ret = node;
    break;
   }
  }

  efree (tmpnodename);

  tmpnodename = emalloc (16+strlen (id));
  *tmpnodename = 0;

  strcat (tmpnodename, "mode-overrides-");
  strcat (tmpnodename, id);

  while ((node = cfg_findnode (tmpnodename, 0, node))) {
   if (node->mode == mode) {
    ret = node;
    break;
   }
  }

  efree (tmpnodename);
 }

 if (!ret && (node = cfg_findnode (id, 0, NULL)))
  ret = node;

 return ret;
}

// return a new stree with the filter applied
struct stree *cfg_filter_f (const char *filter, enum einit_cfg_node_options type) {
 struct stree *retval = NULL;

 if (filter) {
  struct stree *cur = streelinear_prepare(hconfiguration);
  regex_t pattern;
  if (!eregcomp(&pattern, filter)) {
   while (cur) {
    if (!regexec (&pattern, cur->key, 0, NULL, 0) &&
        (!type || (((struct cfgnode *)(cur->value))->type & type))) {
     retval = streeadd (retval, cur->key, cur->value, SET_NOALLOC, NULL);
    }
    cur = streenext (cur);
   }

   eregfree (&pattern);
  }
 }

 return retval;
}

// return a new stree with a certain prefix applied
struct stree *cfg_prefix_f (const char *prefix) {
 struct stree *retval = NULL;

 if (prefix) {
  struct stree *cur = streelinear_prepare(hconfiguration);
  while (cur) {
   if (strprefix(cur->key, prefix)) {
    retval = streeadd (retval, cur->key, cur->value, SET_NOALLOC, NULL);
   }
   cur = streenext (cur);
  }
 }

 return retval;
}

/* those i-could've-sworn-there-were-library-functions-for-that functions */
char *cfg_getpath_f (const char *id) {
 int mplen;
 char *svpath = cfg_getstring (id, NULL);
 if (!svpath) {
  return NULL;
 }
 mplen = strlen (svpath) +1;
 if (svpath[mplen-2] != '/') {
//  if (svpath->path) return svpath->path;
  char *tmpsvpath = (char *)emalloc (mplen+1);
  tmpsvpath[0] = 0;

  strcat (tmpsvpath, svpath);
  tmpsvpath[mplen-1] = '/';
  tmpsvpath[mplen] = 0;
//  svpath->svalue = tmpsvpath;
//  svpath->path = tmpsvpath;
  return tmpsvpath;
 }
 return svpath;
}

void bootstrap_einit_configuration_stree_einit_event_handler_core_configuration_update (struct einit_event *ev) {
// update global environment here
 char **env = einit_global_environment;
 einit_global_environment = NULL;
 struct cfgnode *node = NULL;
 efree (env);

 env = NULL;
 while ((node = cfg_findnode ("configuration-environment-global", 0, node))) {
  if (node->idattr && node->svalue) {
   env = straddtoenviron (env, node->idattr, node->svalue);
  }
 }
 einit_global_environment = env;
}

void bootstrap_einit_configuration_stree_ipc_read (struct einit_event *ev) {
 char **path = ev->para;

 struct ipc_fs_node n;

 if (!path) {
  n.name = (char *)str_stabilise ("configuration");
  n.is_file = 0;
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
 } else if (path && path[0] && strmatch(path[0], "configuration")) {
  n.name = (char *)str_stabilise ("update");
  n.is_file = 1;
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
 }
}

void bootstrap_einit_configuration_stree_ipc_write (struct einit_event *ev) {
 char **path = ev->para;

 if (path && ev->set && ev->set[0] && path[0] && path[1] && strmatch (path[0], "configuration") && strmatch (path[0], "update")) {
  struct einit_event nev = evstaticinit(einit_core_update_configuration);

  if (strmatch (ev->set[0], "update")) {
   notice (4, "event-subsystem: updating configuration");
   nev.string = NULL;
  } else {
   notice (4, "updating configuration with file %s", ev->set[0]);
   nev.string = ev->set[0];
  }

  event_emit (&nev, einit_event_flag_broadcast | einit_event_flag_spawn_thread);

  evstaticdestroy(nev);
 }
}

void bootstrap_einit_configuration_stree_ipc_stat (struct einit_event *ev) {
 char **path = ev->para;

 if (path && path[0]) {
  if (strmatch (path[0], "configuration")) {
   ev->flag = (path[1] && strmatch (path[1], "configuration") ? 1 : 0);
  }
 }
}


int bootstrap_einit_configuration_stree_cleanup (struct lmodule *tm) {
 cfg_free();

 event_ignore (einit_core_configuration_update, bootstrap_einit_configuration_stree_einit_event_handler_core_configuration_update);
 event_ignore (einit_core_done_switching, bootstrap_einit_configuration_stree_einit_event_handler_core_done_switching);
 event_ignore (einit_timer_tick, bootstrap_einit_configuration_stree_einit_event_handler_timer_tick);

 function_unregister ("einit-configuration-node-add", 1, cfg_addnode_f);
 function_unregister ("einit-configuration-node-get", 1, cfg_getnode_f);
 function_unregister ("einit-configuration-node-get-string", 1, cfg_getstring_f);
 function_unregister ("einit-configuration-node-get-find", 1, cfg_findnode_f);
 function_unregister ("einit-configuration-node-get-filter", 1, cfg_filter_f);
 function_unregister ("einit-configuration-node-get-path", 1, cfg_getpath_f);
 function_unregister ("einit-configuration-node-get-prefix", 1, cfg_prefix_f);

 event_ignore (einit_ipc_read, bootstrap_einit_configuration_stree_ipc_read);
 event_ignore (einit_ipc_stat, bootstrap_einit_configuration_stree_ipc_stat);
 event_ignore (einit_ipc_write, bootstrap_einit_configuration_stree_ipc_write);

 return 0;
}

int bootstrap_einit_configuration_stree_configure (struct lmodule *tm) {
 module_init (tm);

 thismodule->cleanup = bootstrap_einit_configuration_stree_cleanup;

 event_listen (einit_core_configuration_update, bootstrap_einit_configuration_stree_einit_event_handler_core_configuration_update);
 event_listen (einit_core_done_switching, bootstrap_einit_configuration_stree_einit_event_handler_core_done_switching);
 event_listen (einit_timer_tick, bootstrap_einit_configuration_stree_einit_event_handler_timer_tick);

 function_register ("einit-configuration-node-add", 1, cfg_addnode_f);
 function_register ("einit-configuration-node-get", 1, cfg_getnode_f);
 function_register ("einit-configuration-node-get-string", 1, cfg_getstring_f);
 function_register ("einit-configuration-node-get-find", 1, cfg_findnode_f);
 function_register ("einit-configuration-node-get-filter", 1, cfg_filter_f);
 function_register ("einit-configuration-node-get-path", 1, cfg_getpath_f);
 function_register ("einit-configuration-node-get-prefix", 1, cfg_prefix_f);

 event_listen (einit_ipc_read, bootstrap_einit_configuration_stree_ipc_read);
 event_listen (einit_ipc_stat, bootstrap_einit_configuration_stree_ipc_stat);
 event_listen (einit_ipc_write, bootstrap_einit_configuration_stree_ipc_write);

 eregcomp (&cfg_storage_allowed_duplicates, ".*");

 return 0;
}
