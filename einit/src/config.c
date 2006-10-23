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

int cfg_free () {
 struct uhash *cur = hconfiguration;
 struct cfgnode *node = NULL;
 while (cur) {
  if (node = (struct cfgnode *)cur->value) {
   if (node->base)
    free (node->base);

   if (node->custom)
    free (node->custom);
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
 if (!node) return;
 hconfiguration = hashadd (hconfiguration, node->id, node, sizeof(struct cfgnode), node->arbattrs);
// hconfiguration = hashadd (hconfiguration, node->id, node, -1);
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

/* new, event-based configuration-file-loader */
int cfg_load (char *configfile) {
 struct einit_event ev = evstaticinit(EVE_UPDATE_CONFIGURATION);

 ev.string = configfile;

 event_emit (&ev, EINIT_EVENT_FLAG_BROADCAST);

 evstaticdestroy(ev);
}