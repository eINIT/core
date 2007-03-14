/*
 *  config-xml-expat.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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
#include <einit-modules/configuration.h>

#define ECXE_MASTERTAG 0x00000001
#define IF_OK          0x1

#if ( EINIT_MODULES_XML_EXPAT == 'm' )
void einit_config_xml_expat_event_handler (struct einit_event *);
char *einit_config_xml_cfg_to_xml (struct stree *);

const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Configuration Parser (XML, Expat)",
 .rid       = "einit-configuration-xml-expat",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

int configure (struct lmodule *this) {
 event_listen (EVENT_SUBSYSTEM_EINIT, einit_config_xml_expat_event_handler);

 function_register ("einit-configuration-converter-xml", 1, einit_config_xml_cfg_to_xml);

 return 0;
}

int cleanup (struct lmodule *this) {
 function_unregister ("einit-configuration-converter-xml", 1, einit_config_xml_cfg_to_xml);

 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_config_xml_expat_event_handler);

 return 0;
}
#endif

struct cfgnode *curmode = NULL;
char **xml_configuration_files = NULL;
time_t xml_configuration_files_highest_mtime = 0;

struct einit_xml_expat_user_data {
 uint32_t options,
          if_level,
          if_results;
 char *file, *prefix;
 uint32_t target_options;
};

char xml_parser_auto_create_missing_directories = 0;

char *xml_source_identifier = "xml-expat";

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
 int nlen = strlen (name);
 if (strmatch (name, "einit")) {
  ((struct einit_xml_expat_user_data *)userData)->options |= ECXE_MASTERTAG;
  return;
 }

 if (!(((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG)) return;

 if (strmatch (name, "if")) {
  (((struct einit_xml_expat_user_data *)userData)->if_level)++;
/* shift results to the left -- make room for another result */
  (((struct einit_xml_expat_user_data *)userData)->if_results) <<= 1;

/* clear the result bit for this one -- might not be necessary */
  (((struct einit_xml_expat_user_data *)userData)->if_results) |= IF_OK;
  (((struct einit_xml_expat_user_data *)userData)->if_results) ^= IF_OK;

  if (atts) {
   uint32_t i = 0;

   for (; atts[i]; i+=2) {
    if (strmatch (atts[i], "match")) { // condition is a literal string match
     char **mt = str2set (':', (char *)(atts[i+1]));
     if (mt && mt[0] && mt[1]) {
      if (strmatch (mt[0], "core-mode")) { // literal match is against the einit core mode (gmode)
       mt[0] = ((gmode == EINIT_GMODE_INIT) ? "init" :
               ((gmode == EINIT_GMODE_METADAEMON) ? "metadaemon" :
               ((gmode == EINIT_GMODE_SANDBOX) ? "sandbox" : "undefined")));
      }

      if (strmatch (mt[0], mt[1]))
       (((struct einit_xml_expat_user_data *)userData)->if_results) |= IF_OK;
     }

     if (mt) free (mt);
    } else if (strmatch (atts[i], "file-exists")) { // does this file exist?
     struct stat stbuf;

     if (!stat (atts[i+1], &stbuf))
      (((struct einit_xml_expat_user_data *)userData)->if_results) |= IF_OK;
    }
   }
  }

  return;
 } else if (strmatch (name, "else") && (((struct einit_xml_expat_user_data *)userData)->if_level)) {
  (((struct einit_xml_expat_user_data *)userData)->if_results) ^= IF_OK;

  return;
 }

 if (!(((struct einit_xml_expat_user_data *)userData)->if_level) ||
      ((((struct einit_xml_expat_user_data *)userData)->if_results) & IF_OK)) {

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

  int i = 0;
  if (strmatch (name, "mode")) {
/* parse the information presented in the element as a mode-definition */
   struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
   newnode->options = ((struct einit_xml_expat_user_data *)userData)->target_options;

   newnode->nodetype = EI_NODETYPE_MODE;
   newnode->arbattrs = (char **)setdup ((const void **)atts, SET_TYPE_STRING);
   newnode->source = xml_source_identifier;
   newnode->source_file = ((struct einit_xml_expat_user_data *)userData)->file;
   for (; newnode->arbattrs[i] != NULL; i+=2) {
    if (strmatch (newnode->arbattrs[i], "id")) {
     newnode->id = estrdup((char *)newnode->arbattrs[i+1]);
    }
/*    else if (strmatch (newnode->arbattrs[i], "base")) {
     newnode->base = str2set (':', (char *)newnode->arbattrs[i+1]);
    }*/
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
   newnode->options = ((struct einit_xml_expat_user_data *)userData)->target_options;

   newnode->id = estrdup (((struct einit_xml_expat_user_data *)userData)->prefix);
   newnode->nodetype = EI_NODETYPE_CONFIG;
   newnode->mode = curmode;
   newnode->arbattrs = (char **)setdup ((const void **)atts, SET_TYPE_STRING);
   newnode->source = xml_source_identifier;
   newnode->source_file = ((struct einit_xml_expat_user_data *)userData)->file;
   if (newnode->arbattrs)
    for (; newnode->arbattrs[i] != NULL; i+=2) {
     if (strmatch (newnode->arbattrs[i], "s"))
      newnode->svalue = (char *)newnode->arbattrs[i+1];
     else if (strmatch (newnode->arbattrs[i], "i"))
      newnode->value = parse_integer (newnode->arbattrs[i+1]);
     else if (strmatch (newnode->arbattrs[i], "b")) {
      newnode->flag = parse_boolean (newnode->arbattrs[i+1]);
     }
    }
   cfg_addnode (newnode);
   free (newnode);
  }
 }
}

void cfg_xml_handler_tag_end (void *userData, const XML_Char *name) {
 if (!(((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG)) return;

 if (strmatch (name, "einit")) {
  ((struct einit_xml_expat_user_data *)userData)->options ^= ECXE_MASTERTAG;
  return;
 } else if (strmatch (name, "if")) {
  (((struct einit_xml_expat_user_data *)userData)->if_level)--;
  (((struct einit_xml_expat_user_data *)userData)->if_results) >>= 1;
  return;
 } else if (strmatch (name, "else")) {
  if  (((struct einit_xml_expat_user_data *)userData)->if_level)
   (((struct einit_xml_expat_user_data *)userData)->if_results) ^= IF_OK;

  return;
 }

 if (!(((struct einit_xml_expat_user_data *)userData)->if_level) ||
      ((((struct einit_xml_expat_user_data *)userData)->if_results) & IF_OK)) {
  if (((struct einit_xml_expat_user_data *)userData)->prefix) {
   int tlen = strlen(name)+1;
   char *last = strrchr (((struct einit_xml_expat_user_data *)userData)->prefix, 0);
   if ((last-tlen) > ((struct einit_xml_expat_user_data *)userData)->prefix) *(last-tlen) = 0;
   else {
    free (((struct einit_xml_expat_user_data *)userData)->prefix);
    ((struct einit_xml_expat_user_data *)userData)->prefix = NULL;
   }
  }

  if (strmatch (name, "mode"))
   curmode = NULL;
 }
}

int einit_config_xml_expat_parse_configuration_file (char *configfile) {
 static char recursion = 0;
 int blen, cfgplen;
 char *data;
 struct stree *hnode;
 struct cfgnode *node = NULL;
 char *confpath = NULL;
 XML_Parser par;
 struct stat st;
 char *tmps = NULL;

 if (!configfile || stat (configfile, &st)) return 0;

 struct einit_xml_expat_user_data expatuserdata = {
  .options = 0,
  .if_level = 0,
  .file = configfile,
  .prefix = NULL,
  .target_options =
    (tmps = cfg_getstring ("core-settings-configuration-on-line-modifications/save-to", NULL)) &&
    strmatch (configfile, tmps) ?
    EINIT_CFGNODE_ONLINE_MODIFICATION : 0
 };

 if ((data = readfile (configfile))) {
  eprintf (stderr, " >> parsing \"%s\".\n", configfile);

  time_t currenttime = time(NULL);
  if (st.st_mtime > currenttime) {// sanity check mtime
   eprintf (stderr, " >> warning: file \"%s\" has mtime in the future\n", configfile);
   xml_configuration_files_highest_mtime = st.st_mtime;
  } else if (st.st_mtime > xml_configuration_files_highest_mtime) // update combined mtime
   xml_configuration_files_highest_mtime = st.st_mtime;

  blen = strlen(data)+1;
  par = XML_ParserCreate (NULL);
  XML_SetUserData (par, (void *)&expatuserdata);
  if (par != NULL) {
   XML_SetElementHandler (par, cfg_xml_handler_tag_start, cfg_xml_handler_tag_end);
   if (XML_Parse (par, data, blen-1, 1) == XML_STATUS_ERROR) {
    uint32_t line = XML_GetCurrentLineNumber (par);
    char **tx = str2set ('\n', data);

    eprintf (stderr, "einit_config_xml_expat_parse_configuration_file(): XML_Parse():\n * in %s, line %i, character %i\n", configfile, line, XML_GetCurrentColumnNumber (par));

    if (tx) {
     if (setcount ((const void **)tx) >= line) {
      eprintf (stderr, " * offending line:\n%s\n", tx[line-1]);
     }
     free (tx);
    }

    bitch (BITCH_EXPAT, 0, XML_ErrorString (XML_GetErrorCode (par)));
   }
   if (!inset ((const void **)xml_configuration_files, (void *)configfile, SET_TYPE_STRING))
    xml_configuration_files = (char **)setadd ((void **)xml_configuration_files,
                                               (void *)configfile, SET_TYPE_STRING);
   XML_ParserFree (par);
  } else {
   bitch (BITCH_EXPAT, 0, "XML Parser could not be created");
  }
  free (data);

  xml_parser_auto_create_missing_directories =
    (node = cfg_getnode("core-settings-xml-parser-auto-create-missing-directories", NULL)) && node->flag;

  if (!recursion) {
   confpath = cfg_getpath ("core-settings-configuration-path");
   if (!confpath) confpath = "/etc/einit/";
   cfgplen = strlen(confpath) +1;
   rescan_node:
   hnode = hconfiguration;

   while (hconfiguration && (hnode = streefind (hconfiguration, "core-commands-include-file", TREE_FIND_FIRST))) {
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

   while (hconfiguration && (hnode = streefind (hconfiguration, "core-commands-include-directory", TREE_FIND_FIRST))) {
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
      if ((tb = (node->svalue[bdlen-2] == '/')))
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

     if ((dir = eopendir (includedir))) {
      char *includefile = (char *)emalloc (bdlen);
      memcpy (includefile, includedir, bdlen-1);
      struct stat statres;

      while ((entry = ereaddir (dir))) {
       includefile[bdlen-1] = 0;

       includefile = erealloc (includefile, bdlen+strlen(entry->d_name));
       strcat (includefile, entry->d_name);

       if (!stat (includefile, &statres) && !S_ISDIR(statres.st_mode)) {
        recursion++;
        einit_config_xml_expat_parse_configuration_file (includefile);
        recursion--;
       }
      }
      eclosedir (dir);
     } else {
      if (xml_parser_auto_create_missing_directories) {
       if (mkdir (includedir, 0777)) {
        bitch(BITCH_STDIO, errno, (char *)includedir);
       } else {
        eprintf (stderr, " >> created missing directory \"%s\"\n", includedir);
       }
      }
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

  return hconfiguration != NULL;
 } else if (errno) {
  eprintf (stderr, " >> could not read file \"%s\": %s\n", configfile, strerror (errno));

  return errno;
 }

 return hconfiguration != NULL;
}

void einit_config_xml_expat_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_UPDATE_CONFIGURATION) {
  if (!ev->string && xml_configuration_files) {
   struct stat st;
   uint32_t i = 0;

   for (; xml_configuration_files && xml_configuration_files [i]; i++) {
    stat (xml_configuration_files [i], &st); // see if any files were modified
    if (st.st_mtime > xml_configuration_files_highest_mtime) { // need to update configuration
     for (i = 0; xml_configuration_files && xml_configuration_files [i]; i++) {
      einit_config_xml_expat_parse_configuration_file (xml_configuration_files [i]);
     }
     ev->chain_type = EVE_CONFIGURATION_UPDATE;

     break;
    }
   }
  }

  if (ev->string) {
   einit_config_xml_expat_parse_configuration_file (ev->string);
   ev->chain_type = EVE_CONFIGURATION_UPDATE;
  }
 }
}

char *einit_config_xml_cfg_to_xml (struct stree *configuration) {
 char *ret = NULL;
 char *retval = NULL;
 char *xtemplate = "<?xml version=\"1.1\" encoding=\"UTF-8\" ?>\n<einit>\n%s</einit>\n";
 ssize_t sxlen;
 struct stree *cur = configuration;

 while (cur) {
  char *xtmp = NULL, *xattributes = NULL;

  if (cur->value) {
   struct cfgnode *node = cur->value;

   if (node->arbattrs) {
    ssize_t x = 0;
    for (x = 0; node->arbattrs[x]; x+=2) {
     char *key = node->arbattrs[x],
          *value = escape_xml(node->arbattrs[x+1]);
     ssize_t clen = strlen (key) + strlen(value) + 5;
     char *ytmp = emalloc (clen);

     esprintf (ytmp, clen, "%s=\"%s\" ", key, value);

     if (xattributes) xattributes = erealloc (xattributes, strlen (xattributes) + strlen (ytmp) +1);
     else {
      xattributes = emalloc (strlen (ytmp) +1);
      *xattributes = 0;
     }

     xattributes = strcat (xattributes, ytmp);

     free (ytmp);
     free (value);
    }
   }
  }

  if (xattributes) {
   if (cur->key && xattributes) {
    ssize_t rxlen = strlen (cur->key) + strlen (xattributes) +7;
    xtmp = emalloc (rxlen);
    esprintf (xtmp, rxlen, " <%s %s/>\n", cur->key, xattributes);
   }
   free (xattributes);
  }

  if (xtmp) {
   if (retval) retval = erealloc (retval, strlen (retval) + strlen (xtmp) +1);
   else {
    retval = emalloc (strlen (xtmp) +1);
    *retval = 0;
   }

   retval = strcat (retval, xtmp);

   free (xtmp);
  }

  cur = streenext(cur);
 }

 if (!retval)
  retval = "";

 sxlen = strlen (retval) + strlen (xtemplate) +1;
 ret = emalloc (sxlen);
 esprintf (ret, sxlen, xtemplate, retval);

 return ret;
}
