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

struct sconfiguration *sconfiguration = NULL;
struct cfgnode *curmode = NULL;

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
 int i = 0;
 if (curmode) {
  if (!strcmp (name, "enable")) {
   for (; atts[i] != NULL; i+=2) {
    if (!strcmp (atts[i], "mod")) {
     char *modlist = estrdup ((char *)atts[i+1]);
	 curmode->enable = str2set (':', modlist);
    }
   }
  }
 }
 if (!strcmp (name, "mode")) {
  struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
  newnode->nodetype = EI_NODETYPE_MODE;
  cfg_addnode (newnode);
  curmode = newnode;
  for (; atts[i] != NULL; i+=2) {
   if (!strcmp (atts[i], "id")) {
    newnode->id = estrdup ((char *)atts[i+1]);
   } else if (!strcmp (atts[i], "base")) {
	char *tmp = estrdup ((char *)atts[i+1]);
	newnode->base = str2set (':', tmp);
	if (!newnode->base) {
     free (tmp);
	 return;
    }
   }
  }
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
  newnode->arbattrs = ecalloc (i+1,sizeof (char *));
  for (i=0; atts[i] != NULL; i++)
   newnode->arbattrs [i] = estrdup ((char *)atts[i]);
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
 ssize_t rn;
 struct cfgnode *node = NULL, *last = NULL;
 char *confpath = NULL;
 XML_Parser par;
 if (!configfile) return 0;
 if (!sconfiguration)
  sconfiguration = ecalloc (1, sizeof(struct sconfiguration));
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
   for (node = sconfiguration->node, last = NULL; node; node = node->next) {
    rescan_node:
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
   }
  }
  return sconfiguration != NULL;
 } else {
  free (sconfiguration);
  sconfiguration = NULL;
  return bitch(BTCH_ERRNO);
 }
}

int cfg_free () {
 if (sconfiguration == NULL)
  return 1;
 if (sconfiguration->node != NULL)
  cfg_freenode (sconfiguration->node);

 free (sconfiguration);
 return 1;
}

int cfg_freenode (struct cfgnode *node) {
 if (node->next)
  cfg_freenode (node->next);

 if (node->nodetype & EI_NODETYPE_CONFIG) {
  if (node->arbattrs) {
   int i = 0;
   for (; node->arbattrs[i] != NULL; i++)
    free (node->arbattrs[i]);
   free (node->arbattrs);
  }
 }

 if (node->id)
  free (node->id);
 if (node->custom)
  free (node->custom);

 free (node);
 return;
}

int cfg_addnode (struct cfgnode *node) {
 struct cfgnode *cur = sconfiguration->node;
 if (!cur)
  sconfiguration->node = node;
 else {
  while (cur->next)
   cur = cur->next;
  cur->next = node;
 }
}

int cfg_delnode (struct cfgnode *node) {
 struct cfgnode *cur = sconfiguration->node;
 if (!node || !cur) return -1;
 if (cur == node) {
  sconfiguration->node = node->next;
  goto cleanup;
/*  node->next = NULL;
  cfg_freenode (node);
  return 0;*/
 }
 while (cur->next) {
  if (cur->next == node) {
   cur->next = node->next;
   cleanup:
   node->next = NULL;
   cfg_freenode (node);
   return 0;
  }
  cur = cur->next;
 }
 return -1;
}

struct cfgnode *cfg_findnode (char *id, unsigned int type, struct cfgnode *base) {
 struct cfgnode *cur = base ? base->next : sconfiguration->node;
 if (!cur || !id) return NULL;
 if (!strcmp(cur->id, id)) return cur;

 while (cur) {
  if (!strcmp(cur->id, id) && (!type || !(cur->nodetype ^ type)))
   return cur;
  cur = cur->next;
 }
 return NULL;
}

int cfg_replacenode (struct cfgnode *old, struct cfgnode *new) {
 struct cfgnode *cur = sconfiguration->node;
 if (!old || !new || !cur) return -1;
 new->next = old->next;

 if (cur == old) {
  sconfiguration->node = new;
/*  old->next = NULL
  cfg_freenode (old);
  return 0;*/
  goto cleanup;
 }

 while (cur->next) {
  if (cur->next == old) {
   cur->next = new;
   cleanup:
   old->next = NULL;
   cfg_freenode (old);
   return 0;
  }
  cur = cur->next;
 }
 return -1;
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
