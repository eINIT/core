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
#include <einit/event.h>

struct cfgnode *curmode = NULL;

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
 int i = 0;
 if (!strcmp (name, "mode")) {
  struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
  newnode->nodetype = EI_NODETYPE_MODE;
  newnode->arbattrs = (char **)setdup ((void **)atts, SET_TYPE_STRING);
  newnode->source = "xml-expat";
  newnode->source_file = (char *)userData;
  for (; newnode->arbattrs[i] != NULL; i+=2) {
   if (!strcmp (newnode->arbattrs[i], "id")) {
    newnode->id = estrdup((char *)newnode->arbattrs[i+1]);
   } else if (!strcmp (newnode->arbattrs[i], "base")) {
    newnode->base = str2set (':', (char *)newnode->arbattrs[i+1]);
   }
  }
  if (newnode->id) {
   char *id = newnode->id;
   cfg_addnode (newnode);
   free (newnode);
/* this is admittedly a tad more complicated than necessary, however its the only way to find the
   last addition to the hash with this id */
   curmode = NULL;
   while (curmode = cfg_findnode (id, EI_NODETYPE_MODE, curmode)) {
    newnode = curmode;
   }
   curmode = newnode;
  }
 } else {
  struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
  newnode->id = estrdup ((char *)name);
  newnode->nodetype = EI_NODETYPE_CONFIG;
  newnode->mode = curmode;
  newnode->arbattrs = (char **)setdup ((void **)atts, SET_TYPE_STRING);
  newnode->source = "xml-expat";
  newnode->source_file = (char *)userData;
  if (newnode->arbattrs)
   for (; newnode->arbattrs[i] != NULL; i+=2) {
    if (!strcmp (newnode->arbattrs[i], "s"))
     newnode->svalue = (char *)newnode->arbattrs[i+1];
    else if (!strcmp (newnode->arbattrs[i], "i"))
     newnode->value = atoi (newnode->arbattrs[i+1]);
    else if (!strcmp (newnode->arbattrs[i], "bi"))
     newnode->value = strtol (newnode->arbattrs[i+1], (char **)NULL, 2);
    else if (!strcmp (newnode->arbattrs[i], "oi"))
     newnode->value = strtol (newnode->arbattrs[i+1], (char **)NULL, 8);
    else if (!strcmp (newnode->arbattrs[i], "b")) {
     int j = i+1;
     newnode->flag = (!strcmp (newnode->arbattrs[j], "true") ||
                      !strcmp (newnode->arbattrs[j], "enabled") ||
                      !strcmp (newnode->arbattrs[j], "yes"));
    }
   }
  cfg_addnode (newnode);
  free (newnode);
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
  XML_SetUserData (par, (void *)configfile);
  if (par != NULL) {
   XML_SetElementHandler (par, cfg_xml_handler_tag_start, cfg_xml_handler_tag_end);
   if (XML_Parse (par, data, blen, 1) == XML_STATUS_ERROR) {
    uint32_t line = XML_GetCurrentLineNumber (par);
    char **tx = str2set ('\n', data);

    fprintf (stderr, "cfg_load(): XML_Parse() failed: %s\n", XML_ErrorString (XML_GetErrorCode (par)));
    fprintf (stderr, " * in %s, line %i, character %i\n", configfile, line, XML_GetCurrentColumnNumber (par));

    if (tx) {
     if (setcount ((void **)tx) >= line)
      fprintf (stderr, " * offending line:\n%s\n", tx[line-1]);
     free (tx);
    }

    if (check_configuration) check_configuration++;
   }
   XML_ParserFree (par);
  } else {
   fputs ("cfg_load(): XML Parser could not be created\n", stderr);
  }
  free (data);

  if (!recursion) {
   confpath = cfg_getpath ("configuration-path");
   if (!confpath) confpath = "/etc/einit/";
   cfgplen = strlen(confpath) +1;
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
     if (node->id) free (node->id);
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

void einit_config_xml_expat_event_handler (struct einit_event *ev) {
}
