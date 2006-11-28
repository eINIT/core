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
#include <einit/tree.h>
#include <einit/event.h>
#include <dirent.h>
#include <sys/stat.h>

struct cfgnode *curmode = NULL;

#define ECXE_MASTERTAG 0x00000001

struct einit_xml_expat_user_data {
 uint32_t options;
 char *file, *prefix;
};

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
 int nlen = strlen (name);
 if (!strcmp (name, "einit")) {
  ((struct einit_xml_expat_user_data *)userData)->options |= ECXE_MASTERTAG;
  return;
 }

 if (!(((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG)) return;

 if (!((struct einit_xml_expat_user_data *)userData)->prefix) {
  ((struct einit_xml_expat_user_data *)userData)->prefix = emalloc (nlen+1);
  *(((struct einit_xml_expat_user_data *)userData)->prefix) = 0;
 } else {
  int plen = strlen (((struct einit_xml_expat_user_data *)userData)->prefix);
  ((struct einit_xml_expat_user_data *)userData)->prefix = erealloc (((struct einit_xml_expat_user_data *)userData)->prefix, plen + nlen+2);
  *((((struct einit_xml_expat_user_data *)userData)->prefix) + plen) = '-';
  *((((struct einit_xml_expat_user_data *)userData)->prefix) + plen + 1) = 0;
 }
 strcat (((struct einit_xml_expat_user_data *)userData)->prefix, name);

// if (((struct einit_xml_expat_user_data *)userData)->prefix) puts (((struct einit_xml_expat_user_data *)userData)->prefix);

 int i = 0;
 if (!strcmp (name, "mode")) {
/* parse the information presented in the element as a mode-definition */
  struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
  newnode->nodetype = EI_NODETYPE_MODE;
  newnode->arbattrs = (char **)setdup ((void **)atts, SET_TYPE_STRING);
  newnode->source = "xml-expat";
  newnode->source_file = ((struct einit_xml_expat_user_data *)userData)->file;
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
   curmode = NULL;
   curmode = cfg_findnode (id, EI_NODETYPE_MODE, curmode);
   free (newnode);
  }
 } else {
/* parse the information presented in the element as a variable */
  struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
  newnode->id = estrdup (((struct einit_xml_expat_user_data *)userData)->prefix);
  newnode->nodetype = EI_NODETYPE_CONFIG;
  newnode->mode = curmode;
  newnode->arbattrs = (char **)setdup ((void **)atts, SET_TYPE_STRING);
  newnode->source = "xml-expat";
  newnode->source_file = ((struct einit_xml_expat_user_data *)userData)->file;
  if (newnode->arbattrs)
   for (; newnode->arbattrs[i] != NULL; i+=2) {
    if (!strcmp (newnode->arbattrs[i], "s"))
     newnode->svalue = (char *)newnode->arbattrs[i+1];
    else if (!strcmp (newnode->arbattrs[i], "i"))
     newnode->value = parse_integer (newnode->arbattrs[i+1]);
    else if (!strcmp (newnode->arbattrs[i], "b")) {
     newnode->flag = parse_boolean (newnode->arbattrs[i+1]);
    }
   }
  cfg_addnode (newnode);
  free (newnode);
 }
}

void cfg_xml_handler_tag_end (void *userData, const XML_Char *name) {
 if (!(((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG)) return;

 if (!strcmp (name, "einit")) {
  ((struct einit_xml_expat_user_data *)userData)->options ^= ECXE_MASTERTAG;
  return;
 }

 if (((struct einit_xml_expat_user_data *)userData)->prefix) {
  int tlen = strlen(name)+1;
  char *last = strrchr (((struct einit_xml_expat_user_data *)userData)->prefix, 0);
  if ((last-tlen) > ((struct einit_xml_expat_user_data *)userData)->prefix) *(last-tlen) = 0;
  else {
   free (((struct einit_xml_expat_user_data *)userData)->prefix);
   ((struct einit_xml_expat_user_data *)userData)->prefix = NULL;
  }
 }

 if (!strcmp (name, "mode"))
  curmode = NULL;
}

int einit_config_xml_expat_parse_configuration_file (char *configfile) {
 static char recursion = 0;
 int e, blen, cfgplen;
 char * data;
 struct stree *hnode;
 ssize_t rn;
 struct cfgnode *node = NULL, *last = NULL;
 char *confpath = NULL;
 XML_Parser par;

 struct einit_xml_expat_user_data expatuserdata = {
  .options = 0,
  .file = configfile,
  .prefix = NULL
 };

 if (!configfile) return 0;
 if (data = readfile (configfile)) {
  blen = strlen(data)+1;
  par = XML_ParserCreate (NULL);
  XML_SetUserData (par, (void *)&expatuserdata);
  if (par != NULL) {
   XML_SetElementHandler (par, cfg_xml_handler_tag_start, cfg_xml_handler_tag_end);
   if (XML_Parse (par, data, blen-1, 1) == XML_STATUS_ERROR) {
    uint32_t line = XML_GetCurrentLineNumber (par);
    char **tx = str2set ('\n', data);

    fprintf (stderr, "einit_config_xml_expat_parse_configuration_file(): XML_Parse() failed: %s\n", XML_ErrorString (XML_GetErrorCode (par)));
    fprintf (stderr, " * in %s, line %i, character %i\n", configfile, line, XML_GetCurrentColumnNumber (par));

    if (tx) {
     if (setcount ((void **)tx) >= line)
      fprintf (stderr, " * offending line:\n%s\n", tx[line-1]);
     free (tx);
    }
   }
   XML_ParserFree (par);
  } else {
   fputs ("einit_config_xml_expat_parse_configuration_file(): XML Parser could not be created\n", stderr);
  }
  free (data);

  if (!recursion) {
   confpath = cfg_getpath ("core-settings-configuration-path");
   if (!confpath) confpath = "/etc/einit/";
   cfgplen = strlen(confpath) +1;
   rescan_node:
   hnode = hconfiguration;

   while (hnode = streefind (hconfiguration, "core-commands-include-file", TREE_FIND_FIRST)) {
    node = (struct cfgnode *)hnode->value;
    if (node->svalue) {
     char *includefile = ecalloc (1, sizeof(char)*(cfgplen+strlen(node->svalue)));
     includefile = strcat (includefile, confpath);
     includefile = strcat (includefile, node->svalue);
     recursion++;

     einit_config_xml_expat_parse_configuration_file (includefile);
     recursion--;
     free (includefile);
     if (node->id) free (node->id);
//     streedel (hconfiguration, hnode);
     streedel (hnode);
     goto rescan_node;
    }
   }

   while (hnode = streefind (hconfiguration, "core-commands-include-directory", TREE_FIND_FIRST)) {
    node = (struct cfgnode *)hnode->value;

    if (node->svalue) {
     char *includedir = NULL;
     DIR *dir;
     struct dirent *entry;
     uint32_t bdlen = strlen(node->svalue)+1;

     if (node->svalue[0] == '/') {
      if (node->svalue[bdlen-2] == '/')
       includedir = estrdup (node->svalue);
      else {
       bdlen++;
       includedir = ecalloc (1, sizeof(char)*(bdlen));
       includedir = strcat (includedir, node->svalue);
       includedir[bdlen-2] = '/';
       includedir[bdlen-1] = 0;
      }
     } else {
      char tb = 1;
      if (tb = (node->svalue[bdlen-2] == '/'))
       bdlen += cfgplen - 1;
      else
       bdlen += cfgplen;

      includedir = ecalloc (1, sizeof(char)*(bdlen));
      includedir = strcat (includedir, confpath);
      includedir = strcat (includedir, node->svalue);

      if (!tb) {
       includedir[bdlen-2] = '/';
       includedir[bdlen-1] = 0;
      }
     }

     dir = opendir (includedir);
     if (dir != NULL) {
      char *includefile = (char *)emalloc (bdlen);
      memcpy (includefile, includedir, bdlen-1);
      struct stat statres;

      while (entry = readdir (dir)) {
       includefile[bdlen-1] = 0;

       includefile = erealloc (includefile, bdlen+strlen(entry->d_name));
       strcat (includefile, entry->d_name);

       if (!stat (includefile, &statres) && !S_ISDIR(statres.st_mode)) {
        recursion++;
        einit_config_xml_expat_parse_configuration_file (includefile);
        recursion--;
       }
      }
      closedir (dir);
     } else {
      printf ("einit_config_xml_expat_parse_configuration_file(): cannot open directory \"%s\": %s\n", includedir, strerror(errno));
     }

     free (includedir);
     if (node->id) free (node->id);
//     streedel (hconfiguration, hnode);
     streedel (hnode);
     goto rescan_node;
    }
   }
  }

  if (expatuserdata.prefix) free (expatuserdata.prefix);

  {
   struct einit_event ee = evstaticinit (EVE_CONFIGURATION_UPDATE);
   event_emit (&ee, EINIT_EVENT_FLAG_BROADCAST);
   evstaticdestroy (ee);
  }

  return hconfiguration != NULL;
 } else {
  return bitch(BTCH_ERRNO);
 }
}

void einit_config_xml_expat_event_handler (struct einit_event *ev) {
 if ((ev->type == EVE_UPDATE_CONFIGURATION) && ev->string) {
  einit_config_xml_expat_parse_configuration_file (ev->string);
 }
}
