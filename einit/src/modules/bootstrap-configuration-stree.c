/*
 *  bootstrap-configuration-stree.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Split from config-xml-expat.c on 22/10/2006
 *  Renamed/moved from config.c on 20/03/2007
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
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/event.h>

#ifdef POSIXREGEX
#include <regex.h>
#endif

int _bootstrap_einit_configuration_stree_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)
const struct smodule _bootstrap_einit_configuration_stree_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Core Configuration Storage and Retrieval (stree-based)",
 .rid       = "bootstrap-configuration-stree",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _bootstrap_einit_configuration_stree_configure
};

module_register(_bootstrap_einit_configuration_stree_self);

#endif

int cfg_free () {
 struct stree *cur = hconfiguration;
 struct cfgnode *node = NULL;
 while (cur) {
  if ((node = (struct cfgnode *)cur->value)) {
   if (node->id)
    free (node->id);
   if (node->path)
    free (node->path);
  }
  cur = streenext (cur);
 }
 streefree (hconfiguration);
 hconfiguration = NULL;
 return 1;
}

int __cfg_addnode (struct cfgnode *node) {
 if (!node || !node->id) return -1;
 struct stree *cur = hconfiguration;
 char doop = 1;
 char *template = NULL;

 if (node->arbattrs) {
  uint32_t r = 0;
  for (; node->arbattrs[r]; r+=2) {
   if (strmatch ("id", node->arbattrs[r])) node->idattr = node->arbattrs[r+1];
   else if (strmatch ("based-on-template", node->arbattrs[r])) template = node->arbattrs[r+1];
  }
 }

 if (template) {
  char **attrs = NULL, *nodename = NULL;
  uint32_t ii = 0, idn = 0;
  struct cfgnode *tnode = NULL;

  nodename = emalloc (strlen(node->id) + 10);
  *nodename = 0;
  strcat (nodename, node->id);
  strcat (nodename, "-template");

  while ((tnode = cfg_findnode (nodename, 0, tnode))) {
   if (tnode->idattr && strmatch (tnode->idattr, template)) break; // found a matching template
  }
  if (!tnode || !tnode->arbattrs) goto no_template;

  for (ii = 0; node->arbattrs[ii]; ii+=2) {
   attrs = (char **)setadd ((void **)attrs, (void *)node->arbattrs[ii], SET_TYPE_STRING);
   attrs = (char **)setadd ((void **)attrs, (void *)node->arbattrs[ii+1], SET_TYPE_STRING);
   if (strmatch (node->arbattrs[ii], "id")) idn = ii+1;
  }
  for (ii = 0; tnode->arbattrs[ii]; ii+=2) if (!inset ((const void **)attrs, (void *)tnode->arbattrs[ii], SET_TYPE_STRING)) {
   char *tmp = apply_variables (tnode->arbattrs[ii+1], (const char **)node->arbattrs);
   attrs = (char **)setadd ((void **)attrs, (void *)tnode->arbattrs[ii], SET_TYPE_STRING);
   attrs = (char **)setadd ((void **)attrs, (void *)tmp, SET_TYPE_STRING);
   if (strmatch (tnode->arbattrs[ii], "id")) idn = ii+1;
   free (tmp);
  }

  free (node->arbattrs);
  node->arbattrs = attrs;
  node->idattr = attrs[idn];
 }
 no_template:

 if (node->nodetype & EI_NODETYPE_MODE) {
/* mode definitions only need to be modified -- it doesn't matter if there's more than one, but
  only the first one would be used anyway. */
  if (cur) cur = streefind (cur, node->id, TREE_FIND_FIRST);
  while (cur) {
   if (cur->value && !(((struct cfgnode *)cur->value)->nodetype ^ EI_NODETYPE_MODE)) {
// this means we found something that looks like it
    void *bsl = cur->luggage;

// we risk not being atomic at this point but... it really is unlikely to go weird.
    ((struct cfgnode *)cur->value)->arbattrs = node->arbattrs;
    cur->luggage = node->arbattrs;

    free (bsl);

    doop = 0;

    break;
   }
//   cur = streenext (cur);
   cur = streefind (cur, node->id, TREE_FIND_NEXT);
  }
 } else {
/* look for other definitions that are exactly the same, only marginally different or that sport a
   matching id="" attribute */

  if (cur) cur = streefind (cur, node->id, TREE_FIND_FIRST);
  while (cur) {
// this means we found a node wit the same path
   if (cur->value && ((struct cfgnode *)cur->value)->idattr && node->idattr &&
       strmatch (((struct cfgnode *)cur->value)->idattr, node->idattr)) {
// NTS: implement checks to figure out if the node is similar

// this means we found something that looks like it
    void *bsl = cur->luggage;
    ((struct cfgnode *)cur->value)->arbattrs = node->arbattrs;
    cur->luggage = node->arbattrs;
//    if (bsl) free (bsl);

    ((struct cfgnode *)cur->value)->nodetype    = node->nodetype;
    ((struct cfgnode *)cur->value)->mode        = node->mode;
    ((struct cfgnode *)cur->value)->flag        = node->flag;
    ((struct cfgnode *)cur->value)->value       = node->value;
    ((struct cfgnode *)cur->value)->svalue      = node->svalue;
    ((struct cfgnode *)cur->value)->idattr      = node->idattr;
    bsl = (void *)((struct cfgnode *)cur->value)->path;
    ((struct cfgnode *)cur->value)->path        = node->path;
//    if (bsl) free (bsl);
    bsl = (void *)((struct cfgnode *)cur->value)->source;
    ((struct cfgnode *)cur->value)->source      = node->source;
//    if (bsl) free (bsl);
    bsl = (void *)((struct cfgnode *)cur->value)->source_file;
    ((struct cfgnode *)cur->value)->source_file = node->source_file;
//    if (bsl) free (bsl);

    doop = 0;

    break;
   }
//   cur = streenext (cur);
   cur = streefind (cur, node->id, TREE_FIND_NEXT);
  }
 }

 if (doop)
  hconfiguration = streeadd (hconfiguration, node->id, node, sizeof(struct cfgnode), node->arbattrs);

 return 0;
}

struct cfgnode *__cfg_findnode (const char *id, const unsigned int type, const struct cfgnode *base) {
 struct stree *cur = hconfiguration;
 if (!id) return NULL;

 if (base) {
  if (cur) cur = streefind (cur, id, TREE_FIND_FIRST);
  while (cur) {
   if (cur->value == base) {
    cur = streefind (cur, id, TREE_FIND_NEXT);
    break;
   }
//   cur = streenext (cur);
    cur = streefind (cur, id, TREE_FIND_NEXT);
  }
 } else if (cur) {
  cur = streefind (cur, id, TREE_FIND_FIRST);
 }

 while (cur) {
  if (cur->value && (!type || !(((struct cfgnode *)cur->value)->nodetype ^ type))) {
   return cur->value;
  }
  cur = streefind (cur, id, TREE_FIND_NEXT);
 }

 return NULL;
}

// get string (by id)
char *__cfg_getstring (const char *id, const struct cfgnode *mode) {
 struct cfgnode *node = NULL;
 char *ret = NULL, **sub;
 uint32_t i;

 if (!id) return NULL;
 mode = mode ? mode : cmode;

 if (strchr (id, '/')) {
  char f = 0;
  sub = str2set ('/', id);
  if (!sub[1]) {
   node = cfg_getnode (id, mode);
   if (node)
    ret = node->svalue;

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

  free (sub);
 } else {
  node = cfg_getnode (id, mode);
  if (node)
   ret = node->svalue;
 }

 return ret;
}

// get node (by id)
struct cfgnode *__cfg_getnode (const char *id, const struct cfgnode *mode) {
 struct cfgnode *node = NULL;
 struct cfgnode *ret = NULL;

 if (!id) return NULL;
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

  free (tmpnodename);
 }

 if (!ret && (node = cfg_findnode (id, 0, NULL)))
  ret = node;

 return ret;
}

// return a new stree with the filter applied
struct stree *__cfg_filter (const char *filter, const uint32_t node_options) {
 struct stree *retval = NULL;

#ifdef POSIXREGEX
 if (filter) {
  struct stree *cur = hconfiguration;
  regex_t pattern;
  if (!eregcomp(&pattern, filter)) {
   while (cur) {
    if (!regexec (&pattern, cur->key, 0, NULL, 0) &&
        (!node_options || (((struct cfgnode *)(cur->value))->options & node_options))) {
     retval = streeadd (retval, cur->key, cur->value, SET_NOALLOC, NULL);
    }
    cur = streenext (cur);
   }
  }
 }
#endif

 return retval;
}

// return a new stree with a certain prefix applied
struct stree *__cfg_prefix (const char *prefix) {
 struct stree *retval = NULL;

#ifdef POSIXREGEX
 if (prefix) {
  struct stree *cur = hconfiguration;
  while (cur) {
   if (strstr(cur->key, prefix) == cur->key) {
    retval = streeadd (retval, cur->key, cur->value, SET_NOALLOC, NULL);
   }
   cur = streenext (cur);
  }
 }
#endif

 return retval;
}

/* those i-could've-sworn-there-were-library-functions-for-that functions */
char *__cfg_getpath (const char *id) {
 int mplen;
 char *svpath = cfg_getstring (id, NULL);
 if (!svpath) return NULL;
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

void _bootstrap_einit_configuration_stree_einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_CONFIGURATION_UPDATE) {
// update global environment here
  char **env = einit_global_environment;
  einit_global_environment = NULL;
  struct cfgnode *node = NULL;
  free (env);

  env = NULL;
  while ((node = cfg_findnode ("configuration-environment-global", 0, node))) {
   if (node->idattr && node->svalue) {
    env = straddtoenviron (env, node->idattr, node->svalue);
   }
  }
  einit_global_environment = env;
 }
}

int _bootstrap_einit_configuration_stree_cleanup (struct lmodule *tm) {
 cfg_free();

 event_ignore (EVENT_SUBSYSTEM_EINIT, _bootstrap_einit_configuration_stree_einit_event_handler);

 function_unregister ("einit-configuration-node-add", 1, __cfg_addnode);
 function_unregister ("einit-configuration-node-get", 1, __cfg_getnode);
 function_unregister ("einit-configuration-node-get-string", 1, __cfg_getstring);
 function_unregister ("einit-configuration-node-get-find", 1, __cfg_findnode);
 function_unregister ("einit-configuration-node-get-filter", 1, __cfg_filter);
 function_unregister ("einit-configuration-node-get-path", 1, __cfg_getpath);
 function_unregister ("einit-configuration-node-get-prefix", 1, __cfg_prefix);

 return 0;
}

int _bootstrap_einit_configuration_stree_configure (struct lmodule *tm) {
 module_init (tm);

 thismodule->cleanup = _bootstrap_einit_configuration_stree_cleanup;

 event_listen (EVENT_SUBSYSTEM_EINIT, _bootstrap_einit_configuration_stree_einit_event_handler);

 function_register ("einit-configuration-node-add", 1, __cfg_addnode);
 function_register ("einit-configuration-node-get", 1, __cfg_getnode);
 function_register ("einit-configuration-node-get-string", 1, __cfg_getstring);
 function_register ("einit-configuration-node-get-find", 1, __cfg_findnode);
 function_register ("einit-configuration-node-get-filter", 1, __cfg_filter);
 function_register ("einit-configuration-node-get-path", 1, __cfg_getpath);
 function_register ("einit-configuration-node-get-prefix", 1, __cfg_prefix);

 return 0;
}
