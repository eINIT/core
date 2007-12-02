/*
 *  bootstrap-configuration-xml-expat.c
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

#include <einit-modules/exec.h>


#define ECXE_MASTERTAG_EINIT   0x00000001
#define ECXE_MASTERTAG_MODULE  0x00000002
#define ECXE_MASTERTAG_NETWORK 0x00000004

#define MODULE_IMPLIED_PREFIX "services-virtual-module"
#define NETWORK_IMPLIED_PREFIX "configuration-network"

#define ECXE_MASTERTAG ( ECXE_MASTERTAG_EINIT | ECXE_MASTERTAG_MODULE | ECXE_MASTERTAG_NETWORK)

int bootstrap_einit_configuration_xml_expat_configure (struct lmodule *);

void einit_config_xml_expat_event_handler_core_update_configuration (struct einit_event *);
void einit_config_xml_expat_ipc_event_handler (struct einit_event *);
char *einit_config_xml_cfg_to_xml (struct stree *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule bootstrap_einit_configuration_xml_expat_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "Configuration Parser (XML, Expat)",
 .rid       = "einit-bootstrap-configuration-xml-expat",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = bootstrap_einit_configuration_xml_expat_configure
};

module_register(bootstrap_einit_configuration_xml_expat_self);

#endif

struct cfgnode *curmode = NULL;

char **xml_configuration_files = NULL;
time_t xml_configuration_files_highest_mtime = 0;

struct ecx_resume_data {
 char **xml_configuration_files;
 time_t xml_configuration_files_highest_mtime;
};

char bootstrap_einit_configuration_xml_expat_usage = 0;

int bootstrap_einit_configuration_xml_expat_cleanup (struct lmodule *this) {
 function_unregister ("einit-configuration-converter-xml", 1, einit_config_xml_cfg_to_xml);

 event_ignore (einit_event_subsystem_ipc, einit_config_xml_expat_ipc_event_handler);
 event_ignore (einit_core_update_configuration, einit_config_xml_expat_event_handler_core_update_configuration);

 exec_cleanup (this);

 return 0;
}

int bootstrap_einit_configuration_xml_expat_suspend (struct lmodule *this) {
 if (!bootstrap_einit_configuration_xml_expat_usage) {
  function_unregister ("einit-configuration-converter-xml", 1, einit_config_xml_cfg_to_xml);

  event_ignore (einit_event_subsystem_ipc, einit_config_xml_expat_ipc_event_handler);
  event_ignore (einit_core_update_configuration, einit_config_xml_expat_event_handler_core_update_configuration);

  struct ecx_resume_data *rd = ecalloc (1, sizeof (struct ecx_resume_data));
  rd->xml_configuration_files = xml_configuration_files;
  rd->xml_configuration_files_highest_mtime = xml_configuration_files_highest_mtime;
  this->resumedata = rd;

  exec_cleanup (this);

  event_wakeup (einit_core_update_configuration, this);
  event_wakeup (einit_event_subsystem_ipc, this);

  return status_ok;
 } else {
  return status_failed;
 }
}

int bootstrap_einit_configuration_xml_expat_resume (struct lmodule *this) {
 struct ecx_resume_data *rd = this->resumedata;
 if (rd) {
  xml_configuration_files = rd->xml_configuration_files;
  xml_configuration_files_highest_mtime = rd->xml_configuration_files_highest_mtime;
  this->resumedata = NULL;
  efree (rd);
 }

 return status_ok;
}

int bootstrap_einit_configuration_xml_expat_configure (struct lmodule *this) {
 module_init(this);
 exec_configure (this);

 thismodule->cleanup = bootstrap_einit_configuration_xml_expat_cleanup;
 thismodule->suspend = bootstrap_einit_configuration_xml_expat_suspend;
 thismodule->resume = bootstrap_einit_configuration_xml_expat_resume;

 event_listen (einit_core_update_configuration, einit_config_xml_expat_event_handler_core_update_configuration);
 event_listen (einit_event_subsystem_ipc, einit_config_xml_expat_ipc_event_handler);

 function_register ("einit-configuration-converter-xml", 1, einit_config_xml_cfg_to_xml);

 return 0;
}

struct einit_xml_expat_user_data {
 uint32_t options;
 char *file, *prefix;
 enum einit_cfg_node_options type;
 uint32_t adds;
};

char xml_parser_auto_create_missing_directories = 0;

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
 int nlen = strlen (name);
 if (!(((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG)) {
  if (strmatch (name, "einit")) {
   uint32_t i = 0;
   ((struct einit_xml_expat_user_data *)userData)->options |= ECXE_MASTERTAG_EINIT;

   if (atts) {
    for (; atts[i]; i += 2) {
     if (!strcmp (atts[i], "prefix")) {
      ((struct einit_xml_expat_user_data *)userData)->prefix = emalloc (strlen (atts[i+1])+1);
      *(((struct einit_xml_expat_user_data *)userData)->prefix) = 0;
      strcat (((struct einit_xml_expat_user_data *)userData)->prefix, atts[i+1]);
     }
    }
   }
  } else if (strmatch (name, "module")) {
   ((struct einit_xml_expat_user_data *)userData)->options |= ECXE_MASTERTAG_MODULE;

   ((struct einit_xml_expat_user_data *)userData)->prefix = emalloc (strlen (MODULE_IMPLIED_PREFIX)+1);
   *(((struct einit_xml_expat_user_data *)userData)->prefix) = 0;
   strcat (((struct einit_xml_expat_user_data *)userData)->prefix, MODULE_IMPLIED_PREFIX);
  } else if (strmatch (name, "network")) {
   ((struct einit_xml_expat_user_data *)userData)->options |= ECXE_MASTERTAG_NETWORK;

   ((struct einit_xml_expat_user_data *)userData)->prefix = emalloc (strlen (NETWORK_IMPLIED_PREFIX)+1);
   *(((struct einit_xml_expat_user_data *)userData)->prefix) = 0;
   strcat (((struct einit_xml_expat_user_data *)userData)->prefix, NETWORK_IMPLIED_PREFIX);
  }

  return;
 }

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
   newnode->type = ((struct einit_xml_expat_user_data *)userData)->type;

   newnode->type |= einit_node_mode;
   newnode->arbattrs = (char **)setdup ((const void **)atts, SET_TYPE_STRING);
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
    if (cfg_addnode (newnode) != -1)
     ((struct einit_xml_expat_user_data *)userData)->adds++;
    curmode = NULL;
    curmode = cfg_findnode (id, einit_node_mode, curmode);
    efree (newnode);
   }
  } else {
/* parse the information presented in the element as a variable */
   struct cfgnode *newnode = ecalloc (1, sizeof (struct cfgnode));
   newnode->type = ((struct einit_xml_expat_user_data *)userData)->type;

   newnode->type |= einit_node_regular;

   newnode->id = estrdup (((struct einit_xml_expat_user_data *)userData)->prefix);
   newnode->mode = curmode;
   newnode->arbattrs = (char **)setdup ((const void **)atts, SET_TYPE_STRING);
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
   if (cfg_addnode (newnode) != -1)
    ((struct einit_xml_expat_user_data *)userData)->adds++;
   efree (newnode);
  }
}

void cfg_xml_handler_tag_end (void *userData, const XML_Char *name) {
 if (!(((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG)) return;

 if (strmatch (name, "einit") && (((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG_EINIT)) {
  ((struct einit_xml_expat_user_data *)userData)->options ^= ECXE_MASTERTAG_EINIT;
  return;
 } else if (strmatch (name, "module") && (((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG_MODULE) && ((struct einit_xml_expat_user_data *)userData)->prefix && strmatch (((struct einit_xml_expat_user_data *)userData)->prefix, MODULE_IMPLIED_PREFIX)) {
  ((struct einit_xml_expat_user_data *)userData)->options ^= ECXE_MASTERTAG_MODULE;
  return;
 } else if (strmatch (name, "network") && (((struct einit_xml_expat_user_data *)userData)->options & ECXE_MASTERTAG_NETWORK) && ((struct einit_xml_expat_user_data *)userData)->prefix && strmatch (((struct einit_xml_expat_user_data *)userData)->prefix, NETWORK_IMPLIED_PREFIX)) {
  ((struct einit_xml_expat_user_data *)userData)->options ^= ECXE_MASTERTAG_NETWORK;
  return;
 }


  if (((struct einit_xml_expat_user_data *)userData)->prefix) {
   int tlen = strlen(name)+1;
   char *last = strrchr (((struct einit_xml_expat_user_data *)userData)->prefix, 0);
   if ((last-tlen) > ((struct einit_xml_expat_user_data *)userData)->prefix) *(last-tlen) = 0;
   else {
    efree (((struct einit_xml_expat_user_data *)userData)->prefix);
    ((struct einit_xml_expat_user_data *)userData)->prefix = NULL;
   }
  }

  if (strmatch (name, "mode"))
   curmode = NULL;
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
  .prefix = NULL,
  .type =
    ((tmps = cfg_getstring ("core-settings-configuration-on-line-modifications/save-to", NULL)) &&
    strmatch (configfile, tmps) ?
    einit_node_modified : 0),
  .adds = 0
 };

 if ((data = readfile (configfile))) {
  notice (9, "parsing \"%s\".\n", configfile);

#ifdef DEBUG
  time_t currenttime = time(NULL);
  if (st.st_mtime > currenttime) {// sanity check mtime
   notice (5, "file \"%s\" has mtime in the future\n", configfile);
   if (st.st_mtime > xml_configuration_files_highest_mtime)
    xml_configuration_files_highest_mtime = st.st_mtime;
  } else if (st.st_mtime > xml_configuration_files_highest_mtime) // update combined mtime
   xml_configuration_files_highest_mtime = st.st_mtime;
#else
  if (st.st_mtime > xml_configuration_files_highest_mtime)
   xml_configuration_files_highest_mtime = st.st_mtime;
#endif

  blen = strlen(data)+1;
  par = XML_ParserCreate (NULL);
  if (par != NULL) {
   XML_SetUserData (par, (void *)&expatuserdata);
   XML_SetElementHandler (par, cfg_xml_handler_tag_start, cfg_xml_handler_tag_end);
   if (XML_Parse (par, data, blen-1, 1) == XML_STATUS_ERROR) {
    uint32_t line = XML_GetCurrentLineNumber (par);
    char **tx = str2set ('\n', data);

    notice (2, "einit_config_xml_expat_parse_configuration_file(): XML_Parse():\n * in %s, line %i, character %i\n", configfile, line, (int)XML_GetCurrentColumnNumber (par));

    if (tx) {
     if (setcount ((const void **)tx) >= line) {
      notice (2, " * offending line:\n%s\n", tx[line-1]);
     }
     efree (tx);
    }

    bitch (bitch_expat, 0, XML_ErrorString (XML_GetErrorCode (par)));
   }
   if (!inset ((const void **)xml_configuration_files, (void *)configfile, SET_TYPE_STRING))
    xml_configuration_files = (char **)setadd ((void **)xml_configuration_files,
                                               (void *)configfile, SET_TYPE_STRING);
   XML_ParserFree (par);
  } else {
   bitch (bitch_expat, 0, "XML Parser could not be created");
  }
  efree (data);

  xml_parser_auto_create_missing_directories =
    (node = cfg_getnode("core-settings-xml-parser-auto-create-missing-directories", NULL)) && node->flag;

  if (!recursion) {
   confpath = cfg_getpath ("core-settings-configuration-path");
   if (!confpath) confpath = "/etc/einit/";
   if (coremode & einit_mode_sandbox) {
    if (confpath[0] == '/') confpath++;
   }

   cfgplen = strlen(confpath) +1;
   rescan_node:
   hnode = hconfiguration;

   while (hconfiguration && (hnode = streefind (hconfiguration, "core-commands-include-directory", tree_find_first))) {
    node = (struct cfgnode *)hnode->value;
    char **files = readdirfilter (node, NULL, "\\.xml$", NULL, 0);

    if (files) {
     int ixx = 0;
     setsort ((void **)files, set_sort_order_string_lexical, NULL);

     for (; files[ixx]; ixx++) {
//      fprintf (stderr, " * %s\n", files[ixx]);
      recursion++;
      einit_config_xml_expat_parse_configuration_file (files[ixx]);
      recursion--;
     }

     efree (files);
    }

    streedel (hnode);
    goto rescan_node;
   }

   while (hconfiguration && (hnode = streefind (hconfiguration, "core-commands-include-file", tree_find_first))) {
    node = (struct cfgnode *)hnode->value;
    if (node->svalue) {
     char *includefile = ecalloc (1, sizeof(char)*(cfgplen+strlen(node->svalue)));
     includefile = strcat (includefile, confpath);
     includefile = strcat (includefile, node->svalue);
     recursion++;

     einit_config_xml_expat_parse_configuration_file (includefile);
     recursion--;
     efree (includefile);
     if (node->id) efree (node->id);
//     streedel (hconfiguration, hnode);
     streedel (hnode);
     goto rescan_node;
    }
   }
  }

  if (expatuserdata.prefix) efree (expatuserdata.prefix);

  return hconfiguration != NULL;
 } else if (errno) {
  notice (3, "could not read file \"%s\": %s\n", configfile, strerror (errno));

  if (expatuserdata.prefix) efree (expatuserdata.prefix);

  return errno;
 }

 if (expatuserdata.prefix) efree (expatuserdata.prefix);

 return hconfiguration != NULL;
}

void einit_config_xml_expat_event_handler_core_update_configuration (struct einit_event *ev) {
 bootstrap_einit_configuration_xml_expat_usage++;

 if (!ev->string && xml_configuration_files) {
  struct stat st;
  uint32_t i = 0;

  for (; xml_configuration_files && xml_configuration_files [i]; i++) {
   stat (xml_configuration_files [i], &st); // see if any files were modified
   if (st.st_mtime > xml_configuration_files_highest_mtime) { // need to update configuration
    for (i = 0; xml_configuration_files && xml_configuration_files [i]; i++) {
     einit_config_xml_expat_parse_configuration_file (xml_configuration_files [i]);
    }
    ev->chain_type = einit_core_configuration_update;

    break;
   }
  }
 }

 if (ev->string) {
  einit_config_xml_expat_parse_configuration_file (ev->string);
  ev->chain_type = einit_core_configuration_update;
 }

 bootstrap_einit_configuration_xml_expat_usage--;
}

void einit_config_xml_expat_ipc_event_handler (struct einit_event *ev) {
 bootstrap_einit_configuration_xml_expat_usage++;

 if ((ev->argc >= 2) && strmatch (ev->argv[0], "examine") && strmatch (ev->argv[1], "configuration")) {
  char *command = cfg_getstring ("core-xml-validator/command", NULL);

  if (command) {
   char *xmlfiles = set2str (' ', (const char **)xml_configuration_files);
   struct einit_event feedback_ev = evstaticinit (einit_feedback_module_status);
   char **myenvironment = straddtoenviron (NULL, "files", xmlfiles);
   myenvironment = straddtoenviron (myenvironment, "rnc_schema", EINIT_LIB_BASE "/schemata/einit.rnc");

   feedback_ev.para = (void *)thismodule;

   pexec (command, NULL, 0, 0, NULL, NULL, myenvironment, &feedback_ev);

   evstaticdestroy (feedback_ev);

   efree (myenvironment);
   efree (xmlfiles);
  }
 }

 bootstrap_einit_configuration_xml_expat_usage--;
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

     efree (ytmp);
     efree (value);
    }
   }
  }

  if (xattributes) {
   if (cur->key && xattributes) {
    ssize_t rxlen = strlen (cur->key) + strlen (xattributes) +7;
    xtmp = emalloc (rxlen);
    esprintf (xtmp, rxlen, " <%s %s/>\n", cur->key, xattributes);
   }
   efree (xattributes);
  }

  if (xtmp) {
   if (retval) retval = erealloc (retval, strlen (retval) + strlen (xtmp) +1);
   else {
    retval = emalloc (strlen (xtmp) +1);
    *retval = 0;
   }

   retval = strcat (retval, xtmp);

   efree (xtmp);
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
