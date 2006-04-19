/***************************************************************************
 *            config-xml-expat.c
 *
 *  Mon Feb  6 15:42:42 2006
 *  Copyright  2006  Magnus Deininger
 *  dma05@web.de
 ****************************************************************************/
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

char *configfile = "/etc/einit/default.xml";
struct sconfiguration *sconfiguration = NULL;
struct cfgnode *curmode = NULL;

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
 int i = 0;
 if (curmode) {
  if (!strcmp (name, "enable")) {
   for (; atts[i] != NULL; i+=2) {
    if (!strcmp (atts[i], "mod")) {
     char *modlist = strdup (atts[i+1]);
     if (!modlist) {
      bitch (BTCH_ERRNO);
      return;
	 }
	 curmode->enable = str2slist (':', modlist);
    }
   }
  }
 }
 if (!strcmp (name, "mode")) {
  struct cfgnode *newnode = calloc (1, sizeof (struct cfgnode));
  if (!newnode) {
   bitch (BTCH_ERRNO);
   return;
  }
  newnode->nodetype = EI_NODETYPE_MODE;
  cfg_addnode (newnode);
  curmode = newnode;
  for (; atts[i] != NULL; i+=2) {
   if (!strcmp (atts[i], "id")) {
    newnode->id = strdup (atts[i+1]);
    if (!newnode->id) {
     free (newnode);
     bitch (BTCH_ERRNO);
     return;
	}
   } else if (!strcmp (atts[i], "base")) {
	char *tmp = strdup (atts[i+1]);
    if (!tmp) {
     free (tmp);
     bitch (BTCH_ERRNO);
     return;
	}
	newnode->base = str2slist (':', tmp);
	if (!newnode->base) {
     free (tmp);
	 return;
    }
   }
  }
 } else {
  struct cfgnode *newnode = calloc (1, sizeof (struct cfgnode));
  if (!newnode) {
   bitch (BTCH_ERRNO);
   return;
  }
  newnode->id = strdup (name);
  newnode->nodetype = EI_NODETYPE_CONFIG;
  if (!newnode->id) {
   free (newnode);
   bitch (BTCH_ERRNO);
   return;
  }
  for (; atts[i] != NULL; i+=2) {
   errno = 0;
   if (!strcmp (atts[i], "s"))
    newnode->svalue = strdup (atts[i+1]);
   else if (!strcmp (atts[i], "i"))
    newnode->value = atoi (atts[i+1]);
   else if (!strcmp (atts[i], "b")) {
	int j = i+1;
    newnode->flag = (!strcmp (atts[j], "true") ||
	                 !strcmp (atts[j], "enabled") ||
	                 !strcmp (atts[j], "yes"));
   }
   if (errno) {
    free (newnode->id);
    free (newnode);
    bitch (BTCH_ERRNO);
    return;
   }
  }
  newnode->arbattrs = calloc (i+1,sizeof (char *));
  for (i=0; atts[i] != NULL; i++)
   newnode->arbattrs [i] = strdup (atts[i]);
  cfg_addnode (newnode);
 }
}

void cfg_xml_handler_tag_end (void *userData, const XML_Char *name) {
 if (!strcmp (name, "mode"))
  curmode = NULL;
}

int cfg_load () {
 int cfgfd, e, blen;
 char * buf, * data;
 ssize_t rn;
 sconfiguration = calloc (1, sizeof(struct sconfiguration));
 if (!sconfiguration) return bitch(BTCH_ERRNO);
 XML_Parser par;
 cfgfd = open (configfile, O_RDONLY);
 if (cfgfd != -1) {
  buf = malloc (BUFFERSIZE*sizeof(char));
  blen = 0;
  do {
   buf = realloc (buf, blen + BUFFERSIZE);
   if (buf == NULL) return bitch(BTCH_ERRNO);
   rn = read (cfgfd, (char *)(buf + blen), BUFFERSIZE);
   blen = blen + rn;
  } while (rn > 0);
  close (cfgfd);
  data = realloc (buf, blen);
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
  return 1;
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

 while (cur->next) {
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
