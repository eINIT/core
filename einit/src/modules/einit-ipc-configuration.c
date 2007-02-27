/*
 *  einit-ipc-configuration.c
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

#define _MODULE

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

const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "IPC Command Library: Mode Configuration",
 .rid       = "einit-ipc-configuration",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

void einit_ipc_handler (struct einit_event *);

struct lmodule *this;

void ipc_event_handler (struct einit_event *ev) {
 char **argv = NULL;
 ssize_t argc = 0;

 if (ev && ev->para) {
  argv = (char **)ev->set;
  argc = setcount ((void **)argv);
 }

 if (argc > 1) {
  if ((argc > 3) && (!strcmp (argv[0], "set") && !strcmp (argv[1], "variable"))) {
   char *t = estrdup (argv[2]), *x, *subattr = NULL;
   struct cfgnode newnode, *onode = NULL;

   eprintf ((FILE *)ev->para, " >> setting variable \"%s\".\n", t);
   ev->flag = 1;

   if ((x = strchr (t, '/'))) {
    *x = 0;
    subattr = x+1;
   }

   onode = cfg_getnode (t, NULL);
   memset (&newnode, 0, sizeof (struct cfgnode));

   newnode.nodetype =
     onode && onode->nodetype ? onode->nodetype : EI_NODETYPE_CONFIG;
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
   newnode.source = self.rid;
   newnode.source_file = NULL;
   newnode.options =
     (onode && onode->options ? onode->options : 0) | EINIT_CFGNODE_ONLINE_MODIFICATION;

   char tflag = parse_boolean (argv[3]);
   long int tvalue = parse_integer (argv[3]);

   if (!subattr) {
    if (tflag || !strcmp (argv[3], "false") || !strcmp (argv[3], "disabled") || !strcmp (argv[3], "no")) {
     subattr = "b";
     newnode.flag = tflag;
    } else if (tvalue || !strcmp (argv[3], "0")) {
     subattr = "i";
     newnode.value = tvalue;
    } else
     subattr = "s";
   }

   if (subattr) {
    char match = 0;
    char **attrs = (char **)setdup ((void **)newnode.arbattrs, SET_TYPE_STRING);

    if (attrs) {
     uint32_t ind = 0;
     for (; attrs[ind]; ind += 2) {
      if (!strcmp (subattr, attrs[ind])) {
       char **tmpadup = attrs;

       attrs[ind+1] = argv[3];
       attrs = (char **)setdup ((void **)attrs, SET_TYPE_STRING);
       newnode.arbattrs = attrs;
       match = 1;

       free (tmpadup);

       if (!strcmp ("s", subattr)) {
        newnode.svalue = attrs[ind+1];
       }

       break;
      }
     }
    }

    if (!match) {
     attrs = (char **)setadd ((void **)attrs, (void *)subattr, SET_TYPE_STRING);
     attrs = (char **)setadd ((void **)attrs, (void *)argv[3], SET_TYPE_STRING);

     if (!strcmp ("s", subattr)) {
      newnode.svalue = attrs[setcount((void **)attrs)-1];
     }

     newnode.arbattrs = attrs;
    }
   }

   cfg_addnode (&newnode);

   free (t);
  } else if (!strcmp (argv[0], "save") && !strcmp (argv[1], "configuration")) {
   struct stree *tmptree = cfg_filter (".*", EINIT_CFGNODE_ONLINE_MODIFICATION);
   char *buffer = NULL;
   cfg_string_converter conv;
   char *targetfile = argv[2] ? argv[2] :
     cfg_getstring ("core-settings-configuration-on-line-modifications/save-to", NULL);

   if (tmptree) {
    if (targetfile) {
     if ((conv = (cfg_string_converter)function_find_one ("einit-configuration-converter-xml", 1, NULL)))
      buffer = conv(tmptree);

     if (buffer) {
      FILE *target;

      eprintf ((FILE *)ev->para, " >> configuration changed on-line, saving modifications to %s.\n", targetfile);

      if ((target = fopen(targetfile, "w+"))) {
       eputs (buffer, target);
       fclose (target);
      } else {
       eprintf ((FILE *)ev->para, " >> could not open \"%s\".\n", targetfile);
      }
     }
    } else {
     eputs (" >> where should i save to?\n", (FILE *)ev->para);
    }
   }

   ev->flag = 1;
  }
 }
}

int configure (struct lmodule *r) {
 this = r;

 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);

 return 0;
}

int cleanup (struct lmodule *irr) {
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);

 return 0;
}

/* passive module, no enable/disable/etc */
