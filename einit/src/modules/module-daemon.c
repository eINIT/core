/*
 *  module-daemon.c
 *  einit
 *
 *  Created by Magnus Deininger on 03/05/2006.
 *  Renamed from einit-mod-daemon.c on 11/10/2006.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <einit-modules/scheduler.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include <inttypes.h>
#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int _einit_mod_daemon_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)

const struct smodule _einit_mod_daemon_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_LOADER,
 .options   = 0,
 .name      = "Module Support (Configuration, Daemon)",
 .rid       = "module-daemon",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _einit_mod_daemon_configure
};

module_register(_einit_mod_daemon_self);

#endif

int _einit_mod_daemon_scanmodules (struct lmodule *);
int _einit_mod_daemon_configure (struct lmodule *);
int _einit_mod_daemon_enable (struct dexecinfo *dexec, struct einit_event *status);
int _einit_mod_daemon_disable (struct dexecinfo *dexec, struct einit_event *status);

struct dexecinfo **_einit_mod_daemon_dxdata = NULL;

void _einit_mod_daemon_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && strmatch(ev->set[0], "examine") && strmatch(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-system-shell", NULL)) {
   eputs (" * configuration variable \"configuration-system-shell\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }

  if (_einit_mod_daemon_dxdata) {
   uint32_t i = 0;
   for (i = 0; _einit_mod_daemon_dxdata[i]; i++) {
    if (_einit_mod_daemon_dxdata[i]->variables) {
     check_variables (_einit_mod_daemon_dxdata[i]->id, (const char **)_einit_mod_daemon_dxdata[i]->variables, (FILE*)ev->para);
    }
   }
  }

  ev->flag = 1;
 }
}

int _einit_mod_daemon_cleanup (struct lmodule *this) {
 exec_cleanup(this);
 event_ignore (EVENT_SUBSYSTEM_IPC, _einit_mod_daemon_ipc_event_handler);

 return 0;
}

int _einit_mod_daemon_cleanup_after_module (struct lmodule *this) {
#if 0
 if (this->module) {
  if (this->module->provides)
   free (this->module->provides);
  if (this->module->requires)
   free (this->module->requires);
  if (this->module->notwith)
   free (this->module->notwith);
  free (this->module);
 }
 if (this->param) {
  if (((struct dexecinfo *)(this->param))->variables)
   free (((struct dexecinfo *)(this->param))->variables);
  if (((struct dexecinfo *)(this->param))->environment)
   free (((struct dexecinfo *)(this->param))->environment);
  free (this->param);
 }
#endif

 return 0;
}

int _einit_mod_daemon_scanmodules (struct lmodule *modchain) {
 struct cfgnode *node;

 node = NULL;
 while ((node = cfg_findnode ("services-virtual-module-daemon", 0, node))) {
  struct smodule *modinfo = ecalloc (1, sizeof (struct smodule));
  struct dexecinfo *dexec = ecalloc (1, sizeof (struct dexecinfo));
  struct lmodule *new;
  int i = 0;
  char doop = 1;
  if (!node->arbattrs) continue;
  for (; node->arbattrs[i]; i+=2 ) {
   if (strmatch (node->arbattrs[i], "id")) {
    modinfo->rid = node->arbattrs[i+1];
    dexec->id = node->arbattrs[i+1];
   } else if (strmatch (node->arbattrs[i], "name"))
    modinfo->name = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "command"))
    dexec->command = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "prepare"))
    dexec->prepare = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "cleanup"))
    dexec->cleanup = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "is-up"))
    dexec->is_up = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "is-down"))
    dexec->is_down = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "uid"))
    dexec->uid = atoi(node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "gid"))
    dexec->gid = atoi(node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "user"))
    dexec->user = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "group"))
    dexec->group = node->arbattrs[i+1];
   else if (strmatch("restart", node->arbattrs[i]))
    dexec->restart = parse_boolean(node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "pid")) {
    dexec->environment = straddtoenviron (dexec->environment, "pidfile", node->arbattrs[i+1]);
    dexec->pidfile = node->arbattrs[i+1];
   }

   else if (strmatch (node->arbattrs[i], "requires"))
    modinfo->si.requires = str2set (':', node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "provides")) {
    modinfo->si.provides = str2set (':', node->arbattrs[i+1]);

    dexec->environment = straddtoenviron (dexec->environment, "services", node->arbattrs[i+1]);
   } else if (strmatch (node->arbattrs[i], "after"))
    modinfo->si.after = str2set (':', node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "before"))
    modinfo->si.before = str2set (':', node->arbattrs[i+1]);

   else if (strmatch (node->arbattrs[i], "variables"))
    dexec->variables = str2set (':', node->arbattrs[i+1]);
   else
    dexec->environment = straddtoenviron (dexec->environment, node->arbattrs[i], node->arbattrs[i+1]);
  }

  if (_einit_mod_daemon_dxdata) {
   uint32_t u = 0;
   char add = 1;
   for (u = 0; _einit_mod_daemon_dxdata[u]; u++) {
    if (strmatch (_einit_mod_daemon_dxdata[u]->id, dexec->id)) {
     add = 0;
     _einit_mod_daemon_dxdata[u] = dexec;
     break;
    }
   }
   if (add)
    _einit_mod_daemon_dxdata = (struct dexecinfo **)setadd ((void **)_einit_mod_daemon_dxdata, (void *)dexec, SET_NOALLOC);
  } else
   _einit_mod_daemon_dxdata = (struct dexecinfo **)setadd ((void **)_einit_mod_daemon_dxdata, (void *)dexec, SET_NOALLOC);

  if (!modinfo->rid) continue;

  struct lmodule *lm = modchain;
  while (lm) {
   if (lm->source && strmatch(lm->source, modinfo->rid)) {

    lm->param = (void *)dexec;
    lm->enable = (int (*)(void *, struct einit_event *))_einit_mod_daemon_enable;
    lm->disable = (int (*)(void *, struct einit_event *))_einit_mod_daemon_disable;
    lm->cleanup = _einit_mod_daemon_cleanup_after_module;
    lm->module = modinfo;

    lm = mod_update (lm);
    doop = 0;
    break;
   }
   lm = lm->next;
  }
  if (doop) {
   new = mod_add (NULL, modinfo);
   if (new) {
    new->source = estrdup (modinfo->rid);
    new->param = (void *)dexec;
    new->enable = (int (*)(void *, struct einit_event *))_einit_mod_daemon_enable;
    new->disable = (int (*)(void *, struct einit_event *))_einit_mod_daemon_disable;
/*    new->reset = (int (*)(void *, struct einit_event *))NULL;
    new->reload = (int (*)(void *, struct einit_event *))NULL;*/
    new->cleanup = _einit_mod_daemon_cleanup_after_module;
   }
  }
 }

 return 0;
}

int _einit_mod_daemon_enable (struct dexecinfo *dexec, struct einit_event *status) {
 return startdaemon (dexec, status);
}

int _einit_mod_daemon_disable (struct dexecinfo *dexec, struct einit_event *status) {
 return stopdaemon (dexec, status);
}

int _einit_mod_daemon_configure (struct lmodule *irr) {
 module_init (irr);

 irr->scanmodules = _einit_mod_daemon_scanmodules;
 irr->cleanup = _einit_mod_daemon_cleanup;

 exec_configure (irr);
 event_listen (EVENT_SUBSYSTEM_IPC, _einit_mod_daemon_ipc_event_handler);

 return 0;
}
