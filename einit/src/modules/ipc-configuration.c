/*
 *  ipc-configuration.c
 *  einit
 *
 *  Created by Magnus Deininger on 08/02/2007.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_ipc_configuration_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_ipc_configuration_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "IPC Command Library: Mode Configuration",
 .rid       = "einit-ipc-configuration",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_ipc_configuration_configure
};

module_register(einit_ipc_configuration_self);

#endif

int einit_ipc_configuration_ipc_event_usage = 0;

void einit_ipc_configuration_ipc_event_handler (struct einit_event *);

void einit_ipc_configuration_ipc_event_handler (struct einit_event *ev) {
 einit_ipc_configuration_ipc_event_usage++;
 if (ev->argc > 1) {
  if (strmatch (ev->argv[0], "update") && strmatch (ev->argv[1], "configuration")) {
   struct einit_event nev = evstaticinit(einit_core_update_configuration);
   nev.string = ev->argv[2];

   if (nev.string) {
    eprintf (stderr, "event-subsystem: updating configuration with file %s\n", ev->argv[2]);
   } else {
    eputs ("event-subsystem: updating configuration\n", stderr);
   }
   event_emit (&nev, einit_event_flag_broadcast);

//   if (nev.type != einit_core_configuration_update) { // force update-info
//   nev.type = einit_core_configuration_update;
//   event_emit (&nev, einit_event_flag_broadcast);
//   }

   evstaticdestroy(nev);

   ev->implemented = 1;
  }

  if (strmatch ("list", ev->argv[0]) && ev->argv[1] && strmatch ("configuration", ev->argv[1])) {
   struct stree *otree = NULL;
   char *buffer = NULL;
   cfg_string_converter conv;

   if (ev->argv[2]) {
    char *x = set2str (' ', (const char **) (ev->argv +2));
    if (x) {
     otree = cfg_filter (x, 0);

     free (x);
    }
   } else {
    otree = hconfiguration;
   }

   if (ev->ipc_options & einit_ipc_output_xml) {
    if ((conv = (cfg_string_converter)function_find_one ("einit-configuration-converter-xml", 1, NULL))) buffer = conv(otree);
   } else {
    char *rtset[] =
    { (ev->ipc_options & einit_ipc_output_ansi) ? "human-readable-ansi" : "human-readable",
    (ev->ipc_options & einit_ipc_output_ansi) ? "human-readable" : "xml",
    (ev->ipc_options & einit_ipc_output_ansi) ? "xml" : "human-readable-ansi", NULL };
    if ((conv = (cfg_string_converter)function_find_one ("einit-configuration-converter", 1, (const char **)rtset))) buffer = conv(otree);
   }

   if (buffer) {
    eputs (buffer, ev->output);
   }
   ev->implemented = 1;
  }

  if ((ev->argc > 3) && (strmatch (ev->argv[0], "set") && strmatch (ev->argv[1], "variable"))) {
   char *t = estrdup (ev->argv[2]), *x, *subattr = NULL;
   struct cfgnode newnode, *onode = NULL;

   eprintf (ev->output, " >> setting variable \"%s\".\n", t);
   ev->implemented = 1;

   if ((x = strchr (t, '/'))) {
    *x = 0;
    subattr = x+1;
   }

   onode = cfg_getnode (t, NULL);
   memset (&newnode, 0, sizeof (struct cfgnode));

   newnode.type =
     (onode && onode->type ? onode->type : einit_node_regular) | einit_node_modified;
   newnode.id =
     estrdup (onode && onode->id ? onode->id : t);
   newnode.mode =
     onode && onode->mode ? onode->mode : NULL;
   newnode.flag =
     onode && onode->flag ? onode->flag : 0;
   newnode.value =
     onode && onode->value ? onode->value : 0;
   newnode.svalue =
     onode && onode->svalue ? onode->svalue : NULL;
   newnode.arbattrs =
     onode && onode->arbattrs ? onode->arbattrs : NULL;
   newnode.path =
     onode && onode->path ? onode->path : NULL;

   char tflag = parse_boolean (ev->argv[3]);
   long int tvalue = parse_integer (ev->argv[3]);

   if (!subattr) {
    if (tflag || strmatch (ev->argv[3], "false") || strmatch (ev->argv[3], "disabled") || strmatch (ev->argv[3], "no")) {
     subattr = "b";
     newnode.flag = tflag;
    } else if (tvalue || strmatch (ev->argv[3], "0")) {
     subattr = "i";
     newnode.value = tvalue;
    } else
     subattr = "s";
   }

   if (subattr) {
    char match = 0;
    char **attrs = (char **)setdup ((const void **)newnode.arbattrs, SET_TYPE_STRING);

    if (attrs) {
     uint32_t ind = 0;
     for (; attrs[ind]; ind += 2) {
      if (strmatch (subattr, attrs[ind])) {
       char **tmpadup = attrs;

       attrs[ind+1] = ev->argv[3];
       attrs = (char **)setdup ((const void **)attrs, SET_TYPE_STRING);
       newnode.arbattrs = attrs;
       match = 1;

       free (tmpadup);

       if (strmatch ("s", subattr)) {
        newnode.svalue = attrs[ind+1];
       }

       break;
      }
     }
    }

    if (!match) {
     attrs = (char **)setadd ((void **)attrs, (void *)subattr, SET_TYPE_STRING);
     attrs = (char **)setadd ((void **)attrs, (void *)ev->argv[3], SET_TYPE_STRING);

     if (strmatch ("s", subattr)) {
      newnode.svalue = attrs[setcount((const void **)attrs)-1];
     }

     newnode.arbattrs = attrs;
    }
   }

   cfg_addnode (&newnode);

   free (t);
  } else if (strmatch (ev->argv[0], "save") && strmatch (ev->argv[1], "configuration")) {
   struct stree *tmptree = cfg_filter (".*", einit_node_modified);
   char *buffer = NULL;
   cfg_string_converter conv;
   char *targetfile = ev->argv[2] ? ev->argv[2] :
     cfg_getstring ("core-settings-configuration-on-line-modifications/save-to", NULL);

   if (tmptree) {
    if (targetfile) {
     if ((conv = (cfg_string_converter)function_find_one ("einit-configuration-converter-xml", 1, NULL)))
      buffer = conv(tmptree);

     if (buffer) {
      FILE *target;

      eprintf (ev->output, " >> configuration changed on-line, saving modifications to %s.\n", targetfile);

      if ((target = efopen(targetfile, "w+"))) {
       eputs (buffer, target);
       efclose (target);
      }
     }
    } else {
     eputs (" >> where should i save to?\n", ev->output);
    }
   }

   ev->implemented = 1;
  }
 }
 einit_ipc_configuration_ipc_event_usage--;
}

int einit_ipc_configuration_cleanup (struct lmodule *irr) {
 event_ignore (einit_ipc_request, einit_ipc_configuration_ipc_event_handler);

 return 0;
}

int einit_ipc_configuration_suspend (struct lmodule *irr) {
 if (!einit_ipc_configuration_ipc_event_usage) {
  event_wakeup (einit_ipc_request, irr);
  event_ignore (einit_ipc_request, einit_ipc_configuration_ipc_event_handler);

  return status_ok;
 } else
  return status_failed;
}

int einit_ipc_configuration_resume (struct lmodule *irr) {
 event_wakeup_cancel (einit_ipc_request, irr);

 return status_ok;
}

int einit_ipc_configuration_configure (struct lmodule *r) {
 module_init (r);

 thismodule->cleanup = einit_ipc_configuration_cleanup;

 thismodule->suspend = einit_ipc_configuration_suspend;
 thismodule->resume = einit_ipc_configuration_resume;

 event_listen (einit_ipc_request, einit_ipc_configuration_ipc_event_handler);

 return 0;
}

/* passive module, no enable/disable/etc */
