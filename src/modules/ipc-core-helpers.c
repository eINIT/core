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
#include <einit/event.h>
#include <errno.h>
#include <string.h>

#include <einit-modules/ipc.h>

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
 .rid       = "einit-ipc-core-helpers",
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
int einit_ipc_core_helpers_event_usage = 0;

#define STATUS2STRING(status)\
 (status == status_idle ? "idle" : \
 (status & status_working ? "working" : \
 (status & status_enabled ? "enabled" : "disabled")))
#define STATUS2STRING_SHORT(status)\
 (status == status_idle ? "I" : \
 (status & status_working ? "W" : \
 (status & status_enabled ? "E" : "D")))

void *einit_ipc_core_helpers_detached_module_action (char **argv) {
 einit_ipc_core_helpers_event_usage++;
 struct lmodule *cur = mlist;
 enum einit_module_task task = 0;

 char *custom = NULL;

 if (strmatch (argv[2], "enable")) task = einit_module_enable;
 else if (strmatch (argv[2], "disable")) task = einit_module_disable;
 else {
  task = einit_module_custom;
  custom = argv[2];
 }

 while (cur) {
  if (strmatch (cur->module->rid, argv[1])) {
   mod (task, cur, custom);
  }

  cur = cur->next;
 }

 efree (argv);

 einit_ipc_core_helpers_event_usage--;

 return NULL;
}

void einit_ipc_core_helpers_ipc_event_handler (struct einit_event *ev) {
 einit_ipc_core_helpers_event_usage++;
 if (!ev || !ev->argv) goto done;

 if ((ev->argc >= 2) && ev->argv[0] && ev->argv[1]) {
  if (strmatch (ev->argv[0], "list")) {
   if (strmatch (ev->argv[1], "modules")) {
    struct lmodule *cur = mlist;

    ev->implemented = 1;

    while (cur) {
     if ((cur->module && !(ev->ipc_options & einit_ipc_only_relevant)) || (cur->status != status_idle)) {
      if (ev->ipc_options & einit_ipc_output_xml) {
       char *name = escape_xml(cur->module->name ? cur->module->name : "unknown");
       char *id = escape_xml(cur->module->rid);
       char *status = escape_xml (STATUS2STRING(cur->status));

       eprintf (ev->output, " <module id=\"%s\" name=\"%s\"\n  status=\"%s\"",
                 id, name, status);

       efree (name);
       efree (id);
       efree (status);
      } else {
       eprintf (ev->output, "[%s] %s (%s)",
                 STATUS2STRING_SHORT(cur->status), (cur->module->rid ? cur->module->rid : "unknown"), (cur->module->name ? cur->module->name : "unknown"));
      }

      if (cur->si) {
       if (cur->si->provides) {
        char *x = set2str(':', (const char **)cur->si->provides);
        if (ev->ipc_options & einit_ipc_output_xml) {
         char *y = escape_xml (x);
         eprintf (ev->output, "\n  provides=\"%s\"", y);
         efree (y);
        } else {
         eprintf (ev->output, "\n > provides: %s", x);
        }
        efree (x);
       }
       if (cur->si->requires) {
        char *x = set2str(':', (const char **)cur->si->requires);
        if (ev->ipc_options & einit_ipc_output_xml) {
         char *y = escape_xml (x);
         eprintf (ev->output, "\n  requires=\"%s\"", y);
         efree (y);
        } else {
         eprintf (ev->output, "\n > requires: %s", x);
        }
        efree (x);
       }
       if (cur->si->after) {
        char *x = set2str(':', (const char **)cur->si->after);
        if (ev->ipc_options & einit_ipc_output_xml) {
         char *y = escape_xml (x);
         eprintf (ev->output, "\n  after=\"%s\"", y);
         efree (y);
        } else {
         eprintf (ev->output, "\n > after: %s", x);
        }
        efree (x);
       }
       if (cur->si->before) {
        char *x = set2str(':', (const char **)cur->si->before);
        if (ev->ipc_options & einit_ipc_output_xml) {
         char *y = escape_xml (x);
         eprintf (ev->output, "\n  before=\"%s\"", y);
         efree (y);
        } else {
         eprintf (ev->output, "\n > before: %s", x);
        }
        efree (x);
       }
      }

      if (ev->ipc_options & einit_ipc_output_xml) {
       char **functions = (char **)setdup ((const void **)cur->functions, SET_TYPE_STRING);
       if (cur->enable) functions = set_str_add (functions, "enable");
       if (cur->disable) functions = set_str_add (functions, "disable");
       functions = set_str_add (functions, "zap");

       if (functions) {
        char *x = set2str(':', (const char **)functions);
        char *y = escape_xml (x);
        eprintf (ev->output, "\n  functions=\"%s\"", y);
        efree (y);
        efree (x);

        efree (functions);
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

 done:
 einit_ipc_core_helpers_event_usage--;
 return;
}

void einit_ipc_core_helpers_ipc_read (struct einit_event *ev) {
 char **path = ev->para;

 struct ipc_fs_node n;

 if (!path) {
  n.name = estrdup ("modules");
  n.is_file = 0;
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
 } if (path && path[0] && strmatch (path[0], "modules")) {
  if (!path[1]) {
   n.name = estrdup ("enabled");
   n.is_file = 0;
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
   n.name = estrdup ("all");
   n.is_file = 0;
   ev->set = set_fix_add (ev->set, &n, sizeof (n));
  } else if (strmatch (path[1], "all") && !path[2]) {
   n.is_file = 0;

   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     n.name = estrdup (cur->module->rid);
     ev->set = set_fix_add (ev->set, &n, sizeof (n));
    }

    cur = cur->next;
   }
  } else if (strmatch (path[1], "enabled") && !path[2]) {
   n.is_file = 0;

   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid && (cur->status & status_enabled)) {
     n.name = estrdup (cur->module->rid);
     ev->set = set_fix_add (ev->set, &n, sizeof (n));
    }

    cur = cur->next;
   }
  } else if (path[2] && !path[3]) {
   n.is_file = 1;

   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      n.name = estrdup ("name");
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
      n.name = estrdup ("status");
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
      n.name = estrdup ("provides");
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
      n.name = estrdup ("requires");
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
      n.name = estrdup ("after");
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
      n.name = estrdup ("before");
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
      break;
     }
    }

    cur = cur->next;
   }
  } else if (path[2] && path[3] && strmatch (path[3], "status")) {
   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      if (cur->status == status_idle)
       ev->stringset = set_str_add (ev->stringset, "idle");
      else {
       if (cur->status & status_enabled)
        ev->stringset = set_str_add (ev->stringset, "enabled");
       if (cur->status & status_working)
        ev->stringset = set_str_add (ev->stringset, "working");
       if (cur->status & status_disabled)
        ev->stringset = set_str_add (ev->stringset, "disabled");
       if (cur->status & status_suspended)
        ev->stringset = set_str_add (ev->stringset, "suspended");
      }

      break;
     }
    }

    cur = cur->next;
   }
  } else if (path[2] && path[3] && strmatch (path[3], "name")) {
   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      ev->stringset = set_str_add (ev->stringset, cur->module->name);
     }
    }

    cur = cur->next;
   }
  } else if (path[2] && path[3] && strmatch (path[3], "provides")) {
   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      if (cur->si && cur->si->provides) {
       int i = 0;

       for (; cur->si->provides[i]; i++) {
        ev->stringset = set_str_add (ev->stringset, cur->si->provides[i]);
       }
      } else {
       ev->stringset = set_str_add (ev->stringset, "none");
      }
     }
    }

    cur = cur->next;
   }
  } else if (path[2] && path[3] && strmatch (path[3], "requires")) {
   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      if (cur->si && cur->si->requires) {
       int i = 0;

       for (; cur->si->requires[i]; i++) {
        ev->stringset = set_str_add (ev->stringset, cur->si->requires[i]);
       }
      } else {
       ev->stringset = set_str_add (ev->stringset, "none");
      }
     }
    }

    cur = cur->next;
   }
  } else if (path[2] && path[3] && strmatch (path[3], "before")) {
   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      if (cur->si && cur->si->before) {
       int i = 0;

       for (; cur->si->before[i]; i++) {
        ev->stringset = set_str_add (ev->stringset, cur->si->before[i]);
       }
      } else {
       ev->stringset = set_str_add (ev->stringset, "none");
      }
     }
    }

    cur = cur->next;
   }
  } else if (path[2] && path[3] && strmatch (path[3], "after")) {
   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      if (cur->si && cur->si->requires) {
       int i = 0;

       for (; cur->si->after[i]; i++) {
        ev->stringset = set_str_add (ev->stringset, cur->si->after[i]);
       }
      } else {
       ev->stringset = set_str_add (ev->stringset, "none");
      }
     }
    }

    cur = cur->next;
   }
  }
 }
}

void einit_ipc_core_helpers_ipc_stat (struct einit_event *ev) {
 char **path = ev->para;

 if (path && path[0] && strmatch (path[0], "modules")) {
  ev->flag = (path[1] && path[2] && path[3] ? 1 : 0);
 }
}

int einit_ipc_core_helpers_cleanup (struct lmodule *irr) {
 ipc_cleanup(r);

 event_ignore (einit_ipc_request_generic, einit_ipc_core_helpers_ipc_event_handler);
 event_ignore (einit_ipc_read, einit_ipc_core_helpers_ipc_read);
 event_ignore (einit_ipc_stat, einit_ipc_core_helpers_ipc_stat);

 return 0;
}

int einit_ipc_core_helpers_suspend (struct lmodule *irr) {
 if (!einit_ipc_core_helpers_event_usage) {
  event_wakeup (einit_ipc_request_generic, irr);
  event_ignore (einit_ipc_request_generic, einit_ipc_core_helpers_ipc_event_handler);

  return status_ok;
 } else
  return status_failed;
}

int einit_ipc_core_helpers_resume (struct lmodule *irr) {
 event_wakeup_cancel (einit_ipc_request_generic, irr);

 return status_ok;
}

int einit_ipc_core_helpers_configure (struct lmodule *r) {
 module_init (r);
 ipc_configure(r);

 thismodule->cleanup = einit_ipc_core_helpers_cleanup;
 thismodule->resume = einit_ipc_core_helpers_resume;
 thismodule->suspend = einit_ipc_core_helpers_suspend;

 event_listen (einit_ipc_request_generic, einit_ipc_core_helpers_ipc_event_handler);
 event_listen (einit_ipc_read, einit_ipc_core_helpers_ipc_read);
 event_listen (einit_ipc_stat, einit_ipc_core_helpers_ipc_stat);

 return 0;
}

/* passive module, no enable/disable/etc */
