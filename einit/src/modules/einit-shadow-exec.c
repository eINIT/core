/*
 *  einit-shadow-exec.c
 *  einit
 *
 *  Created by Magnus Deininger on 09/03/2007.
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

#define _MODULE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <einit/module.h>
#include <einit/config.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/bitch.h>
#include <einit-modules/exec.h>

#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int _einit_shadow_exec_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)
const struct smodule _einit_shadow_exec_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Shadow Module Support",
 .rid       = "einit-shadow-exec",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _einit_shadow_exec_configure
};

module_register(_einit_shadow_exec_self);

#endif

struct shadow_descriptor {
 char *before_enable,
      *after_enable,
      *before_disable,
      *after_disable,
      *before_reset,
      *after_reset,
      *before_reload,
      *after_reload;
};

struct cfgnode *ecmode = NULL;
struct stree *shadows = NULL;

pthread_mutex_t shadow_mutex = PTHREAD_MUTEX_INITIALIZER;

void update_shadows(struct cfgnode *xmode) {
 emutex_lock(&shadow_mutex);

 if (ecmode != xmode) {
  char *tmp = cfg_getstring("shadows", xmode);

  if (shadows) {
//   streefree (shadows);
   shadows = NULL;
  }

  if (tmp) {
   char **tmps = str2set (':', tmp);

   if (tmps) {
    struct cfgnode *cur = NULL;

    while ((cur = cfg_findnode ("services-shadow", 0, cur))) {
     if (cur->idattr && inset ((const void **)tmps, (void *)cur->idattr, SET_TYPE_STRING)) {
      ssize_t i = 0;
      char **nserv = NULL;
      struct shadow_descriptor nshadow;

      memset (&nshadow, 0, sizeof(struct shadow_descriptor));
      for (; cur->arbattrs[i]; i+=2) {
       if (strmatch (cur->arbattrs[i], "service"))
        nserv = str2set(':', cur->arbattrs[i+1]);
       else if (strmatch (cur->arbattrs[i], "before-enable"))
        nshadow.before_enable = cur->arbattrs[i+1];
       else if (strmatch (cur->arbattrs[i], "before-disable"))
        nshadow.before_disable = cur->arbattrs[i+1];
       else if (strmatch (cur->arbattrs[i], "before-reset"))
        nshadow.before_reset = cur->arbattrs[i+1];
       else if (strmatch (cur->arbattrs[i], "before-reload"))
        nshadow.before_reload = cur->arbattrs[i+1];
       else if (strmatch (cur->arbattrs[i], "after-enable"))
        nshadow.after_enable = cur->arbattrs[i+1];
       else if (strmatch (cur->arbattrs[i], "after-disable"))
        nshadow.after_disable = cur->arbattrs[i+1];
       else if (strmatch (cur->arbattrs[i], "after-reset"))
        nshadow.after_reset = cur->arbattrs[i+1];
       else if (strmatch (cur->arbattrs[i], "after-reload"))
        nshadow.after_reload = cur->arbattrs[i+1];
      }

      if (nserv) {
       for (i = 0; nserv[i]; i++) {
        shadows = streeadd (shadows, nserv[i], &nshadow, sizeof(struct shadow_descriptor), NULL);
       }

       free (nserv);
      }
     }
    }

    free (tmps);
   }
  }

  ecmode = xmode;
 }

 emutex_unlock(&shadow_mutex);
}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_UPDATE_CONFIGURATION) {
  update_shadows(cmode);
 } else if (ev->type == EVE_SWITCHING_MODE) {
  update_shadows(ev->para);
 } else if (ev->type == EVE_SERVICE_UPDATE) {
  if (shadows && ev->set) {
   ssize_t i = 0;

   for (; ev->set[i]; i++) {
    struct stree *cur = streefind(shadows, (char *)ev->set[i], TREE_FIND_FIRST);

    while (cur) {
     struct shadow_descriptor *sd = cur->value;

     if (ev->task & MOD_ENABLE) {
      if (ev->status == STATUS_WORKING) {
       if (sd->before_enable)
        pexec (sd->before_enable, NULL, 0, 0, NULL, NULL, NULL, NULL);
      } else if (ev->status & STATUS_ENABLED) {
       if (sd->after_enable)
        pexec (sd->after_enable, NULL, 0, 0, NULL, NULL, NULL, NULL);
      }
     } else if (ev->task & MOD_DISABLE) {
      if (ev->status == STATUS_WORKING) {
       if (sd->before_disable)
        pexec (sd->before_disable, NULL, 0, 0, NULL, NULL, NULL, NULL);
      } else if (ev->status & STATUS_DISABLED) {
       if (sd->after_disable)
        pexec (sd->after_disable, NULL, 0, 0, NULL, NULL, NULL, NULL);
      }
     } else if (ev->task & MOD_RESET) {
      if (ev->status == STATUS_WORKING) {
       if (sd->before_reset)
        pexec (sd->before_reset, NULL, 0, 0, NULL, NULL, NULL, NULL);
      } else if (ev->status & STATUS_ENABLED) {
       if (sd->after_reset)
        pexec (sd->after_reset, NULL, 0, 0, NULL, NULL, NULL, NULL);
      }
     } else if (ev->task & MOD_RELOAD) {
      if (ev->status == STATUS_WORKING) {
       if (sd->before_reload)
        pexec (sd->before_reload, NULL, 0, 0, NULL, NULL, NULL, NULL);
      } else if (ev->status & STATUS_ENABLED) {
       if (sd->after_reload)
        pexec (sd->after_reload, NULL, 0, 0, NULL, NULL, NULL, NULL);
      }
     }

     cur = streefind(cur, (char *)ev->set[i], TREE_FIND_NEXT);
    }
   }
  }
 }
}

int _einit_shadow_exec_cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);

 exec_cleanup(this);

 return 0;
}

int _einit_shadow_exec_configure (struct lmodule *this) {
 module_init (this);

 thismodule->cleanup = _einit_shadow_exec_cleanup;

 exec_configure(this);

 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);

 return 0;
}
