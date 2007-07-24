/*
 *  ipc-core-helpers.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/03/2007.
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

int einit_ipc_core_helpers_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_ipc_core_helpers_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "IPC Command Library: Core Helpers",
 .rid       = "ipc-core-helpers",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_ipc_core_helpers_configure
};

module_register(einit_ipc_core_helpers_self);

#endif

struct lmodule *mlist;

void einit_ipc_core_helpers_ipc_event_handler (struct einit_event *);

#define STATUS2STRING(status)\
 (status == status_idle ? "idle" : \
 (status & status_working ? "working" : \
 (status & status_enabled ? "enabled" : "disabled")))
#define STATUS2STRING_SHORT(status)\
 (status == status_idle ? "I" : \
 (status & status_working ? "W" : \
 (status & status_enabled ? "E" : "D")))

void einit_ipc_core_helpers_ipc_event_handler (struct einit_event *ev) {
 if (!ev || !ev->argv) return;

 if ((ev->argc >= 2) && ev->argv[0] && ev->argv[1]) {
  if (strmatch (ev->argv[0], "examine") && strmatch (ev->argv[1], "configuration")) {
   struct lmodule *cur = mlist;
   ev->implemented = 1;

   while (cur) {
    if (cur->module) {
     struct einit_cfgvar_info **variables = cur->module->configuration;

     if (variables) {
      uint32_t i = 0;

      for (i = 0; variables[i]; i++) {
       char *s = cfg_getstring (variables[i]->variable, NULL);

       if (!s) {
        eprintf (ev->output, " >> module \"%s\" (%s): variable %s not found (%s), description:\n  %s\n", cur->module->name, cur->module->rid, variables[i]->variable, variables[i]->options & eco_critical ? "critical" : "non-critical", variables[i]->description);
       }
      }
     }
    }

    cur = cur->next;
   }
  }

  if (strmatch (ev->argv[0], "list")) {
   if (strmatch (ev->argv[1], "modules")) {
    struct lmodule *cur = mlist;

    ev->implemented = 1;

    while (cur) {
     if ((cur->module && !(ev->ipc_options & einit_ipc_only_relevant)) || (cur->status != status_idle)) {
      if (ev->ipc_options & einit_ipc_output_xml) {
       eprintf (ev->output, " <module id=\"%s\" name=\"%s\"\n  status=\"%s\"",
                 (cur->module->rid ? cur->module->rid : "unknown"), (cur->module->name ? cur->module->name : "unknown"), STATUS2STRING(cur->status));
      } else {
       eprintf (ev->output, "[%s] %s (%s)",
                 STATUS2STRING_SHORT(cur->status), (cur->module->rid ? cur->module->rid : "unknown"), (cur->module->name ? cur->module->name : "unknown"));
      }

      if (cur->si) {
       if (cur->si->provides) {
        if (ev->ipc_options & einit_ipc_output_xml) {
         eprintf (ev->output, "\n  provides=\"%s\"", set2str(':', (const char **)cur->si->provides));
        } else {
         eprintf (ev->output, "\n > provides: %s", set2str(' ', (const char **)cur->si->provides));
        }
       }
       if (cur->si->requires) {
        if (ev->ipc_options & einit_ipc_output_xml) {
         eprintf (ev->output, "\n  requires=\"%s\"", set2str(':', (const char **)cur->si->requires));
        } else {
         eprintf (ev->output, "\n > requires: %s", set2str(' ', (const char **)cur->si->requires));
        }
       }
       if (cur->si->after) {
        if (ev->ipc_options & einit_ipc_output_xml) {
         eprintf (ev->output, "\n  after=\"%s\"", set2str(':', (const char **)cur->si->after));
        } else {
         eprintf (ev->output, "\n > after: %s", set2str(' ', (const char **)cur->si->after));
        }
       }
       if (cur->si->before) {
        if (ev->ipc_options & einit_ipc_output_xml) {
         eprintf (ev->output, "\n  before=\"%s\"", set2str(':', (const char **)cur->si->before));
        } else {
         eprintf (ev->output, "\n > before: %s", set2str(' ', (const char **)cur->si->before));
        }
       }
      }

      if (ev->ipc_options & einit_ipc_output_xml) {
       eputs (" />\n", ev->output);
      } else {
       eputs ("\n", ev->output);
      }
     }
     cur = cur->next;
    }
   }
  }
 }
}


int einit_ipc_core_helpers_cleanup (struct lmodule *irr) {
 event_ignore (einit_event_subsystem_ipc, einit_ipc_core_helpers_ipc_event_handler);

 return 0;
}

int einit_ipc_core_helpers_configure (struct lmodule *r) {
 module_init (r);

 thismodule->cleanup = einit_ipc_core_helpers_cleanup;

 event_listen (einit_event_subsystem_ipc, einit_ipc_core_helpers_ipc_event_handler);

 return 0;
}

/* passive module, no enable/disable/etc */
