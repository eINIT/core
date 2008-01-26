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

#define STATUS2STRING(status)\
 (status == status_idle ? "idle" : \
 (status & status_working ? "working" : \
 (status & status_enabled ? "enabled" : "disabled")))
#define STATUS2STRING_SHORT(status)\
 (status == status_idle ? "I" : \
 (status & status_working ? "W" : \
 (status & status_enabled ? "E" : "D")))

void *einit_ipc_core_helpers_detached_module_action (char **argv) {
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

 return NULL;
}

void einit_ipc_core_helpers_ipc_event_handler (struct einit_event *ev) {
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
 return;
}

void einit_ipc_core_helpers_ipc_read (struct einit_event *ev) {
 char **path = ev->para;
 if (path && path[0] && path[1] && path[2] && path[3] && path[4] && strmatch (path[0], "services") && (strmatch (path[3], "users") || strmatch (path[3], "modules") || strmatch (path[3], "providers"))) {
  char **pn = set_str_add (NULL, "modules");

  pn = set_str_add (pn, "all");
  int i = 4;
  for (; path[i]; i++) {
   pn = set_str_add (pn, path[i]);
  }

  path = pn;
 }

 struct ipc_fs_node n;

 if (!path) {
  n.name = estrdup ("modules");
  n.is_file = 0;
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
  n.name = estrdup ("mode");
  n.is_file = 1;
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
 } if (path && path[0]) {
  if (strmatch (path[0], "modules")) {
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
       n.name = estrdup ("actions");
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
       break;
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
       break;
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
       break;
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
       break;
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
       break;
      }
     }

     cur = cur->next;
    }
   } else if (path[2] && path[3] && strmatch (path[3], "actions")) {
    struct lmodule *cur = mlist;

    while (cur) {
     if (cur->module && cur->module->rid) {
      if (strmatch (path[2], cur->module->rid)) {
       if (cur->enable) {
        ev->stringset = set_str_add (ev->stringset, "enable");
       }
       if (cur->disable) {
        ev->stringset = set_str_add (ev->stringset, "disable");
       }
       if (cur->enable && cur->disable) {
        ev->stringset = set_str_add (ev->stringset, "zap");
       }

       if (cur->functions) {
        int i = 0;

        for (; cur->functions[i]; i++) {
         ev->stringset = set_str_add (ev->stringset, cur->functions[i]);
        }
       } else if (cur->custom) {
        ev->stringset = set_str_add (ev->stringset, "*");
       }

       if (!ev->stringset) {
        ev->stringset = set_str_add (ev->stringset, "none");
       }
       break;
      }
     }

     cur = cur->next;
    }
   }
  } else if (!path[1] && strmatch (path[0], "mode")) {
   if (cmode) {
    if (cmode->idattr) {
     ev->stringset = set_str_add (ev->stringset, cmode->idattr);
    } else {
     ev->stringset = set_str_add (ev->stringset, "unknown");
    }
   } else {
    ev->stringset = set_str_add (ev->stringset, "none");
   }
  }
 }

 if (path != ev->para) efree (path);
}

struct ch_thread_data {
 int a;
 struct lmodule *b;
 char *c;
};

void *einit_ipc_core_helpers_ipc_write_detach_action (struct ch_thread_data *da) {
 mod (da->a, da->b, da->c);

 if (da->c) {
  efree (da->c);
 }
 efree (da);

 return NULL;
}

void einit_ipc_core_helpers_ipc_write (struct einit_event *ev) {
 char **path = ev->para;
 if (path && path[0] && path[1] && path[2] && path[3] && path[4] && strmatch (path[0], "services") && (strmatch (path[3], "users") || strmatch (path[3], "modules") || strmatch (path[3], "providers"))) {
  char **pn = set_str_add (NULL, "modules");

  pn = set_str_add (pn, "all");
  int i = 4;
  for (; path[i]; i++) {
   pn = set_str_add (pn, path[i]);
  }

  path = pn;
 }

 if (path && ev->set && ev->set[0] && path[0]) {
  if (strmatch (path[0], "mode")) {
   struct einit_event ee = evstaticinit(einit_core_switch_mode);
   ee.string = estrdup (ev->set[0]);
   event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
   evstaticdestroy(ee);
  } else if (path[1] && path[2] && path[3] && strmatch (path[0], "modules") && strmatch (path[3], "status")) {
   struct lmodule *cur = mlist;

   while (cur) {
    if (cur->module && cur->module->rid) {
     if (strmatch (path[2], cur->module->rid)) {
      struct ch_thread_data *da = emalloc (sizeof (struct ch_thread_data));
      da->b = cur;
      da->c = NULL;

      if (strmatch (ev->set[0], "enable")) {
       da->a = einit_module_enable;
      } else if (strmatch (ev->set[0], "disable")) {
       da->a = einit_module_disable;
      } else {
       da->a = einit_module_custom;
       da->c = estrdup (ev->set[0]);
      }

      ethread_spawn_detached ((void *(*)(void *))einit_ipc_core_helpers_ipc_write_detach_action, da);

      break;
     }
    }

    cur = cur->next;
   }
  }
 }

 if (path != ev->para) efree (path);
}

void einit_ipc_core_helpers_ipc_stat (struct einit_event *ev) {
 char **path = ev->para;
 if (path && path[0] && path[1] && path[2] && path[3] && path[4] && strmatch (path[0], "services") && (strmatch (path[3], "users") || strmatch (path[3], "modules") || strmatch (path[3], "providers"))) {
  char **pn = set_str_add (NULL, "modules");

  pn = set_str_add (pn, "all");
  int i = 4;
  for (; path[i]; i++) {
   pn = set_str_add (pn, path[i]);
  }

  path = pn;
 }

 if (path && path[0]) {
  if (strmatch (path[0], "modules")) {
   ev->flag = (path[1] && path[2] && path[3] ? 1 : 0);
  } else if (!path[1] && strmatch (path[0], "mode")) {
   ev->flag = 1;
  }
 }

 if (path != ev->para) efree (path);
}

int einit_ipc_core_helpers_cleanup (struct lmodule *irr) {
 ipc_cleanup(r);

 event_ignore (einit_ipc_request_generic, einit_ipc_core_helpers_ipc_event_handler);
 event_ignore (einit_ipc_read, einit_ipc_core_helpers_ipc_read);
 event_ignore (einit_ipc_stat, einit_ipc_core_helpers_ipc_stat);
 event_ignore (einit_ipc_write, einit_ipc_core_helpers_ipc_write);

 return 0;
}

int einit_ipc_core_helpers_configure (struct lmodule *r) {
 module_init (r);
 ipc_configure(r);

 thismodule->cleanup = einit_ipc_core_helpers_cleanup;

 event_listen (einit_ipc_request_generic, einit_ipc_core_helpers_ipc_event_handler);
 event_listen (einit_ipc_read, einit_ipc_core_helpers_ipc_read);
 event_listen (einit_ipc_stat, einit_ipc_core_helpers_ipc_stat);
 event_listen (einit_ipc_write, einit_ipc_core_helpers_ipc_write);

 return 0;
}

/* passive module, no enable/disable/etc */
