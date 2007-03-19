/*
 *  einit-ipc-core-helpers.c
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

struct lmodule *mlist;

int _einit_ipc_core_helpers_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)

const struct smodule _einit_ipc_core_helpers_self = {
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
 },
 .configure = _einit_ipc_core_helpers_configure
};

module_register(_einit_ipc_core_helpers_self);

#endif

void _einit_ipc_core_helpers_ipc_event_handler (struct einit_event *);

#define STATUS2STRING(status)\
 (status == STATUS_IDLE ? "idle" : \
 (status & STATUS_WORKING ? "working" : \
 (status & STATUS_ENABLED ? "enabled" : "disabled")))
#define STATUS2STRING_SHORT(status)\
 (status == STATUS_IDLE ? "I" : \
 (status & STATUS_WORKING ? "W" : \
 (status & STATUS_ENABLED ? "E" : "D")))

void _einit_ipc_core_helpers_ipc_event_handler (struct einit_event *ev) {
 if (!ev || !ev->set) return;
 char **argv = (char **) ev->set;
 int argc = setcount ((const void **)ev->set);
 uint32_t options = ev->status;

 if (argc >= 2) {
  if (strmatch (argv[0], "list")) {
   if (strmatch (argv[1], "modules")) {
    struct lmodule *cur = mlist;

    if (!ev->flag) ev->flag = 1;

    while (cur) {
     if ((cur->module && !(options & EIPC_ONLY_RELEVANT)) || (cur->status != STATUS_IDLE)) {
      if (options & EIPC_OUTPUT_XML) {
       eprintf ((FILE *)ev->para, " <module id=\"%s\" name=\"%s\"\n  status=\"%s\"",
                 (cur->module->rid ? cur->module->rid : "unknown"), (cur->module->name ? cur->module->name : "unknown"), STATUS2STRING(cur->status));
      } else {
       eprintf ((FILE *)ev->para, "[%s] %s (%s)",
                 STATUS2STRING_SHORT(cur->status), (cur->module->rid ? cur->module->rid : "unknown"), (cur->module->name ? cur->module->name : "unknown"));
      }

      if (cur->si) {
       if (cur->si->provides) {
        if (options & EIPC_OUTPUT_XML) {
         eprintf ((FILE *)ev->para, "\n  provides=\"%s\"", set2str(':', (const char **)cur->si->provides));
        } else {
         eprintf ((FILE *)ev->para, "\n > provides: %s", set2str(' ', (const char **)cur->si->provides));
        }
       }
       if (cur->si->requires) {
        if (options & EIPC_OUTPUT_XML) {
         eprintf ((FILE *)ev->para, "\n  requires=\"%s\"", set2str(':', (const char **)cur->si->requires));
        } else {
         eprintf ((FILE *)ev->para, "\n > requires: %s", set2str(' ', (const char **)cur->si->requires));
        }
       }
       if (cur->si->after) {
        if (options & EIPC_OUTPUT_XML) {
         eprintf ((FILE *)ev->para, "\n  after=\"%s\"", set2str(':', (const char **)cur->si->after));
        } else {
         eprintf ((FILE *)ev->para, "\n > after: %s", set2str(' ', (const char **)cur->si->after));
        }
       }
       if (cur->si->before) {
        if (options & EIPC_OUTPUT_XML) {
         eprintf ((FILE *)ev->para, "\n  before=\"%s\"", set2str(':', (const char **)cur->si->before));
        } else {
         eprintf ((FILE *)ev->para, "\n > before: %s", set2str(' ', (const char **)cur->si->before));
        }
       }
      }

      if (options & EIPC_OUTPUT_XML) {
       eputs (" />\n", (FILE *)ev->para);
      } else {
       eputs ("\n", (FILE *)ev->para);
      }
     }
     cur = cur->next;
    }
   } else if (strmatch (argv[1], "services")) {
    struct lmodule *cur = mlist;
    struct stree *serv = NULL;
    struct stree *modes = NULL;
    struct cfgnode *cfgn = cfg_findnode ("mode-enable", 0, NULL);

//    emutex_lock (&modules_update_mutex);

    while (cur) {
     uint32_t i = 0;
     if (cur->si && cur->si->provides) {
      for (i = 0; cur->si->provides[i]; i++) {
       struct stree *curserv = serv ? streefind (serv, cur->si->provides[i], TREE_FIND_FIRST) : NULL;
       if (curserv) {
        curserv->value = (void *)setadd ((void **)curserv->value, (void *)cur, SET_NOALLOC);
        curserv->luggage = curserv->value;
       } else {
        void **nvalue = setadd ((void **)NULL, (void *)cur, SET_NOALLOC);
        serv = streeadd (serv, cur->si->provides[i], nvalue, SET_NOALLOC, nvalue);
       }
      }
     }

     cur = cur->next;
    }

    while (cfgn) {
     if (cfgn->arbattrs && cfgn->mode && cfgn->mode->id && (!modes || !streefind (modes, cfgn->mode->id, TREE_FIND_FIRST))) {
      uint32_t i = 0;
      for (i = 0; cfgn->arbattrs[i]; i+=2) {
       if (strmatch(cfgn->arbattrs[i], "services")) {
        char **tmps = str2set (':', cfgn->arbattrs[i+1]);

        modes = streeadd (modes, cfgn->mode->id, tmps, SET_NOALLOC, tmps);

        break;
       }
      }
     }

     cfgn = cfg_findnode ("mode-enable", 0, cfgn);
    }

    if (serv) {
     struct stree *scur = serv;
     while (scur) {
      char **inmodes = NULL;
      struct stree *mcur = modes;

      while (mcur) {
       if (inset ((const void **)mcur->value, (void *)scur->key, SET_TYPE_STRING)) {
        inmodes = (char **)setadd((void **)inmodes, (void *)mcur->key, SET_TYPE_STRING);
       }

       mcur = streenext(mcur);
      }

      if (inmodes) {
       char *modestr;
       if (options & EIPC_OUTPUT_XML) {
        modestr = set2str (':', (const char **)inmodes);
        eprintf ((FILE *)ev->para, " <service id=\"%s\" used-in=\"%s\">\n", scur->key, modestr);
       } else {
        modestr = set2str (' ', (const char **)inmodes);
        eprintf ((FILE *)ev->para, (options & EIPC_OUTPUT_ANSI) ?
                                "\e[1mservice \"%s\" (%s)\n\e[0m" :
                                  "service \"%s\" (%s)\n",
                                scur->key, modestr);
       }
       free (modestr);
       free (inmodes);
      } else if (!(options & EIPC_ONLY_RELEVANT)) {
       if (options & EIPC_OUTPUT_XML) {
        eprintf ((FILE *)ev->para, " <service id=\"%s\">\n", scur->key);
       } else {
        eprintf ((FILE *)ev->para, (options & EIPC_OUTPUT_ANSI) ?
                                "\e[1mservice \"%s\" (not in any mode)\e[0m\n" :
                                  "service \"%s\" (not in any mode)\n",
                                scur->key);
       }
      }

      if (inmodes || (!(options & EIPC_ONLY_RELEVANT))) {
       if (options & EIPC_OUTPUT_XML) {
        if (scur->value) {
         struct lmodule **xs = scur->value;
         uint32_t u = 0;
         for (u = 0; xs[u]; u++) {
          eprintf ((FILE *)ev->para, "  <module id=\"%s\" name=\"%s\" />\n",
                    xs[u]->module && xs[u]->module->rid ? xs[u]->module->rid : "unknown",
                    xs[u]->module && xs[u]->module->name ? xs[u]->module->name : "unknown");
         }
        }

        eputs (" </service>\n", (FILE*)ev->para);
       } else {
        if (scur->value) {
         struct lmodule **xs = scur->value;
         uint32_t u = 0;
         for (u = 0; xs[u]; u++) {
          eprintf ((FILE *)ev->para, (options & EIPC_OUTPUT_ANSI) ?
            ((xs[u]->module && (xs[u]->module->options & EINIT_MOD_DEPRECATED)) ?
                                  " \e[31m- \e[0mcandidate \"%s\" (%s)\n" :
                                  " \e[33m* \e[0mcandidate \"%s\" (%s)\n") :
              " * candidate \"%s\" (%s)\n",
            xs[u]->module && xs[u]->module->rid ? xs[u]->module->rid : "unknown",
            xs[u]->module && xs[u]->module->name ? xs[u]->module->name : "unknown");
         }
        }
       }
      }

      scur = streenext (scur);
     }

     streefree (serv);
    }
    if (modes) streefree (modes);

//    emutex_unlock (&modules_update_mutex);

    if (!ev->flag) ev->flag = 1;
   }
  }
 }
}


int _einit_ipc_core_helpers_cleanup (struct lmodule *irr) {
 event_ignore (EVENT_SUBSYSTEM_IPC, _einit_ipc_core_helpers_ipc_event_handler);

 return 0;
}

int _einit_ipc_core_helpers_configure (struct lmodule *r) {
 module_init (r);

 thismodule->cleanup = _einit_ipc_core_helpers_cleanup;

 event_listen (EVENT_SUBSYSTEM_IPC, _einit_ipc_core_helpers_ipc_event_handler);

 return 0;
}

/* passive module, no enable/disable/etc */
