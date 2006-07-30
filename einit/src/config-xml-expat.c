/*
 *  config-xml-expat.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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
#include <expat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>

#define PATH_MODULES 1
#define FEEDBACK_MODULE 1

struct uhash *hconfiguration = NULL;
struct cfgnode *curmode = NULL;

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
 int i = 0;
 if (curmode) {
  if (!strcmp (name, "enable")) {
   for (; atts[i] != NULL; i+=2) {
    if (!strcmp (atts[i], "mod")) {
     curmode->enable = str2set (':', (char *)atts[i+1]);
    }
   }
  }
 }
 if (!strcmp (name, "mode")) {
  struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
  newnode->nodetype = EI_NODETYPE_MODE;
  curmode = newnode;
  for (; atts[i] != NULL; i+=2) {
   if (!strcmp (atts[i], "id")) {
    newnode->id = estrdup ((char *)atts[i+1]);
   } else if (!strcmp (atts[i], "base")) {
    newnode->base = str2set (':', (char *)atts[i+1]);
   }
  }
  cfg_addnode (newnode);
 } else {
  struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
  newnode->id = estrdup ((char *)name);
  newnode->nodetype = EI_NODETYPE_CONFIG;
  newnode->mode = curmode;
  for (; atts[i] != NULL; i+=2) {
   if (!strcmp (atts[i], "s"))
    newnode->svalue = estrdup ((char *)atts[i+1]);
   else if (!strcmp (atts[i], "i"))
    newnode->value = atoi (atts[i+1]);
   else if (!strcmp (atts[i], "bi"))
    newnode->value = strtol (atts[i+1], (char **)NULL, 2);
   else if (!strcmp (atts[i], "oi"))
    newnode->value = strtol (atts[i+1], (char **)NULL, 8);
   else if (!strcmp (atts[i], "b")) {
    int j = i+1;
    newnode->flag = (!strcmp (atts[j], "true") ||
                     !strcmp (atts[j], "enabled") ||
                     !strcmp (atts[j], "yes"));
   }
  }
  newnode->arbattrs = (char **)setdup ((void **)atts, SET_TYPE_STRING);
  cfg_addnode (newnode);
 }
}

void cfg_xml_handler_tag_end (void *userData, const XML_Char *name) {
 if (!strcmp (name, "mode"))
  curmode = NULL;
}

int cfg_load (char *configfile) {
 static char recursion = 0;
 int cfgfd, e, blen, cfgplen;
 char * buf, * data;
 struct uhash *hnode;
 ssize_t rn;
 struct cfgnode *node = NULL, *last = NULL;
 char *confpath = NULL;
 XML_Parser par;
 if (!configfile) return 0;
 cfgfd = open (configfile, O_RDONLY);
 if (cfgfd != -1) {
  buf = emalloc (BUFFERSIZE*sizeof(char));
  blen = 0;
  do {
   buf = erealloc (buf, blen + BUFFERSIZE);
   if (buf == NULL) return bitch(BTCH_ERRNO);
   rn = read (cfgfd, (char *)(buf + blen), BUFFERSIZE);
   blen = blen + rn;
  } while (rn > 0);
  close (cfgfd);
  data = erealloc (buf, blen);
  par = XML_ParserCreate (NULL);
  if (par != NULL) {
   XML_SetElementHandler (par, cfg_xml_handler_tag_start, cfg_xml_handler_tag_end);
   if (XML_Parse (par, data, blen, 1) == XML_STATUS_ERROR) {
    puts ("cfg_load(): XML_Parse() failed:");
    puts (XML_ErrorString (XML_GetErrorCode (par)));
   }
   XML_ParserFree (par);
  }
  free (data);

  if (!recursion) {
   confpath = cfg_getpath ("configuration-path");
   if (!confpath) confpath = "/etc/einit/";
   cfgplen = strlen(confpath) +1;
/*   for (hnode = configuration, last = NULL; hnode; hnode = hashnext (hnode)) {
    rescan_node:
	node = (struct cfgnode *)hnode->value;
    if (!node) break;
    if (node->svalue && !strcmp (node->id, "include")) {
     char *includefile = ecalloc (1, sizeof(char)*(cfgplen+strlen(node->svalue)));
     last->next = node->next;
     includefile = strcat (includefile, confpath);
     includefile = strcat (includefile, node->svalue);
     free (node);
     node = last->next;
     recursion++;
     cfg_load (includefile);
     recursion--;
     free (includefile);
     goto rescan_node;
    }
    last = node;
   }*/
   rescan_node:
   hnode = hconfiguration;
   while (hnode = hashfind (hnode, "include")) {
	node = (struct cfgnode *)hnode->value;
	if (node->svalue) {
     char *includefile = ecalloc (1, sizeof(char)*(cfgplen+strlen(node->svalue)));
     includefile = strcat (includefile, confpath);
     includefile = strcat (includefile, node->svalue);
     recursion++;
     cfg_load (includefile);
     recursion--;
     free (includefile);
	 hashdel (hconfiguration, hnode);
     goto rescan_node;
    }
   }
  }
  return hconfiguration != NULL;
 } else {
  return bitch(BTCH_ERRNO);
 }
}

int cfg_free () {
/* if (sconfiguration == NULL)
  return 1;
 if (sconfiguration->node != NULL)
  cfg_freenode (sconfiguration->node);

 free (sconfiguration);*/
 hashfree (hconfiguration);
 return 1;
}

int cfg_freenode (struct cfgnode *node) {
// if (node->next)
//  cfg_freenode (node->next);

 if (node->arbattrs)
  free (node->arbattrs);
 if (node->base)
  free (node->base);
 if (node->enable)
  free (node->enable);

 if (node->id)
  free (node->id);
 if (node->custom)
  free (node->custom);

 free (node);
 return;
}

int cfg_addnode (struct cfgnode *node) {
 if (!node) return;
// hconfiguration = hashadd (hconfiguration, node->id, node, sizeof(struct cfgnode *));
 hconfiguration = hashadd (hconfiguration, node->id, node, -1);
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
// puts (".");
/* if (!strcmp(cur->id, id)) return cur;

 while (cur) {
  if (!strcmp(cur->id, id) && (!type || !(cur->nodetype ^ type)))
   return cur;
  cur = cur->next;
 }*/
// puts (id);
 while (cur = hashfind (cur, id)) {
//  puts (((struct cfgnode *)cur->value)->id);
//  puts (cur->key);
  if (cur->value && (!type || !(((struct cfgnode *)cur->value)->nodetype ^ type)))
   return cur->value;
  cur = hashnext (cur);
 }
 return NULL;
}

/* those i-could've-sworn-there-were-library-functions-for-that functions */

char *cfg_getpath (char *id) {
 int mplen;
 struct cfgnode *svpath = cfg_findnode (id, 0, NULL);
 if (!svpath || !svpath->svalue) return NULL;
 mplen = strlen (svpath->svalue) +1;
 if (svpath->svalue[mplen-2] != '/') {
  char *tmpsvpath = (char *)erealloc (svpath->svalue, mplen+1);

  tmpsvpath[mplen-1] = '/';
  tmpsvpath[mplen] = 0;
  svpath->svalue = tmpsvpath;
 }
 return svpath->svalue;
}
