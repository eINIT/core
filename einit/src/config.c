/*
 *  config.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Split from config-xml-expat.c on 22/10/2006
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
#include <einit/event.h>

struct uhash *hconfiguration = NULL;
char **einit_global_environment = NULL;

int cfg_free () {
 struct uhash *cur = hconfiguration;
 struct cfgnode *node = NULL;
 while (cur) {
  if (node = (struct cfgnode *)cur->value) {
   if (node->base)
    free (node->base);

//   if (node->custom)
//    free (node->custom);
   if (node->id)
    free (node->id);
   if (node->path)
    free (node->path);
  }
  cur = hashnext (cur);
 }
 hashfree (hconfiguration);
 hconfiguration = NULL;
 return 1;
}

int cfg_addnode (struct cfgnode *node) {
 if (!node || !node->id) return;
 struct uhash *cur = hconfiguration;
 char doop = 1;

 if (node->arbattrs) {
  uint32_t r = 0;
  for (; node->arbattrs[r]; r+=2) {
   if (!strcmp ("id", node->arbattrs[r])) node->idattr = node->arbattrs[r+1];
  }
 }

 if (node->nodetype & EI_NODETYPE_MODE) {
/* mode definitions only need to be modified -- it doesn't matter if there's more than one, but
  only the first one would be used anyway. */
  while (cur = hashfind (cur, node->id)) {
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
   cur = hashnext (cur);
  }
 } else {
/* look for other definitions that are exactly the same, only marginally different or that sport a
   matching id="" attribute */

  while (cur = hashfind (cur, node->id)) {
// this means we found a node wit the same path
   if (cur->value && ((struct cfgnode *)cur->value)->idattr && node->idattr &&
       !strcmp (((struct cfgnode *)cur->value)->idattr, node->idattr)) {
// NTS: implement checks to figure out if the node is similar

// this means we found something that looks like it
    void *bsl = cur->luggage;
    ((struct cfgnode *)cur->value)->arbattrs = node->arbattrs;
    cur->luggage = node->arbattrs;
    if (bsl) free (bsl);

    ((struct cfgnode *)cur->value)->nodetype    = node->nodetype;
    ((struct cfgnode *)cur->value)->mode        = node->mode;
    ((struct cfgnode *)cur->value)->flag        = node->flag;
    ((struct cfgnode *)cur->value)->value       = node->value;
    ((struct cfgnode *)cur->value)->svalue      = node->svalue;
    ((struct cfgnode *)cur->value)->idattr      = node->idattr;
    bsl = (void *)((struct cfgnode *)cur->value)->path;
    ((struct cfgnode *)cur->value)->path        = node->path;
    if (bsl) free (bsl);
    bsl = (void *)((struct cfgnode *)cur->value)->source;
    ((struct cfgnode *)cur->value)->source      = node->source;
//    if (bsl) free (bsl);
    bsl = (void *)((struct cfgnode *)cur->value)->source_file;
    ((struct cfgnode *)cur->value)->source_file = node->source_file;
//    if (bsl) free (bsl);

    doop = 0;
//    fprintf (stderr, "configuration: found match for %s\n", node->id);

    break;
   }
   cur = hashnext (cur);
  }
 }

 if (doop)
  hconfiguration = hashadd (hconfiguration, node->id, node, sizeof(struct cfgnode), node->arbattrs);
}

struct cfgnode *cfg_findnode (char *id, unsigned int type, struct cfgnode *base) {
 struct uhash *cur = hconfiguration;
 if (base) {
  while (cur) {
   if (cur->value == base) {
    cur = hashnext (cur);
    break;
   }
   cur = hashnext (cur);
  }
 }
 if (!cur || !id) return NULL;
 while (cur = hashfind (cur, id)) {
  if (cur->value && (!type || !(((struct cfgnode *)cur->value)->nodetype ^ type)))
   return cur->value;
  cur = hashnext (cur);
 }
 return NULL;
}

// get string (by id)
char *cfg_getstring (char *id, struct cfgnode *mode) {
 struct cfgnode *node = NULL;
 char *ret = NULL, **sub;
 uint32_t i;

 if (!id) return NULL;
 mode = mode ? mode : cmode;

 if (strchr (id, '/')) {
  char f = 0;
  sub = str2set ('/', id);
  node = cfg_getnode (sub[0], mode);
  if (node && node->arbattrs && node->arbattrs[0]) {
   for (i = 0; node->arbattrs[i]; i+=2) {
    if (f = (!strcmp(node->arbattrs[i], sub[1]))) {
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
struct cfgnode *cfg_getnode (char *id, struct cfgnode *mode) {
 struct cfgnode *node = NULL;
 struct cfgnode *ret = NULL;
 char *tmpnodename = NULL;

 if (!id) return NULL;
 mode = mode ? mode : cmode;

 tmpnodename = emalloc (6+strlen (id));
 *tmpnodename = 0;

 strcat (tmpnodename, "mode-");
 strcat (tmpnodename, id);

 while (node = cfg_findnode (tmpnodename, 0, node)) {
  if (node->mode == mode) {
   ret = node;
   break;
  }
 }

 if (!ret && (node = cfg_findnode (id, 0, NULL)))
  ret = node;

 free (tmpnodename);
 return ret;
}

/* those i-could've-sworn-there-were-library-functions-for-that functions */
char *cfg_getpath (char *id) {
 int mplen;
 struct cfgnode *svpath = cfg_findnode (id, 0, NULL);
 if (!svpath || !svpath->svalue) return NULL;
 mplen = strlen (svpath->svalue) +1;
 if (svpath->svalue[mplen-2] != '/') {
  if (svpath->path) return svpath->path;
  char *tmpsvpath = (char *)emalloc (mplen+1);
  tmpsvpath[0] = 0;

  strcat (tmpsvpath, svpath->svalue);
  tmpsvpath[mplen-1] = '/';
  tmpsvpath[mplen] = 0;
//  svpath->svalue = tmpsvpath;
  svpath->path = tmpsvpath;
  return tmpsvpath;
 }
 return svpath->svalue;
}

void einit_config_event_handler (struct einit_event *ev) {
/* if ((ev->type == EVE_UPDATE_CONFIGURATION) && ev->string) {

 } else*/ if (ev->type == EVE_CONFIGURATION_UPDATE) {
// update global environment here
  char **env = einit_global_environment;
  einit_global_environment = NULL;
  struct cfgnode *node = NULL;
  free (env);

  env = NULL;
  while (node = cfg_findnode ("configuration-environment-global", 0, node)) {
   if (node->idattr && node->svalue) {
    env = straddtoenviron (env, node->idattr, node->svalue);
   }
  }
  einit_global_environment = env;
 }
}
