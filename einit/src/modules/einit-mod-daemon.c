/*
 *  einit-mod-daemon.c
 *  einit
 *
 *  Created by Magnus Deininger on 03/05/2006.
 *  Renamed from einit-mod-daemon.c on 11/10/2006.
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
#include <einit/scheduler.h>
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

// char *provides[] = {"daemon", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_LOADER,
	.options	= 0,
	.name		= "daemon-pseudo-module support",
	.rid		= "einit-mod-daemon",
	.provides	= NULL,
	.requires	= NULL,
	.notwith	= NULL
};


int scanmodules (struct lmodule *);
int configure (struct lmodule *);
int enable (struct dexecinfo *dexec, struct einit_event *status);
int disable (struct dexecinfo *dexec, struct einit_event *status);

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-system-shell", NULL)) {
   fdputs (" * configuration variable \"configuration-system-shell\" not found.\n", ev->integer);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int configure (struct lmodule *irr) {
 exec_configure (irr);
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *this) {
 exec_cleanup(this);
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup_after_module (struct lmodule *this) {
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
}

int scanmodules (struct lmodule *modchain) {
 struct cfgnode *node;

 node = NULL;
 while (node = cfg_findnode ("services-virtual-module-daemon", 0, node)) {
  struct smodule *modinfo = ecalloc (1, sizeof (struct smodule));
  struct dexecinfo *dexec = ecalloc (1, sizeof (struct dexecinfo));
  struct lmodule *new;
  int i = 0;
  char doop = 1;
  if (!node->arbattrs) continue;
  for (; node->arbattrs[i]; i+=2 ) {
   if (!strcmp (node->arbattrs[i], "id"))
    modinfo->rid = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "name"))
    modinfo->name = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "command"))
    dexec->command = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "prepare"))
    dexec->prepare = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "cleanup"))
    dexec->cleanup = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "uid"))
    dexec->uid = atoi(node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "gid"))
    dexec->gid = atoi(node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "user"))
    dexec->user = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "group"))
    dexec->group = node->arbattrs[i+1];
   else if (!strcmp("restart", node->arbattrs[i]))
    dexec->restart = parse_boolean(node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "requires"))
    modinfo->requires = str2set (':', node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "provides"))
    modinfo->provides = str2set (':', node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "uses"))
    modinfo->uses = str2set (':', node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "notwith"))
    modinfo->notwith = str2set (':', node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "variables"))
    dexec->variables = str2set (':', node->arbattrs[i+1]);
   else
    dexec->environment = straddtoenviron (dexec->environment, node->arbattrs[i], node->arbattrs[i+1]);
  }
/*  new = mod_add (NULL, (int (*)(void *, struct einit_event *))startdaemon,
           (int (*)(void *, struct einit_event *))stopdaemon, NULL, NULL,
           cleanup_after_module,
           (void *)dexec, modinfo);*/

  if (!modinfo->rid) continue;

  struct lmodule *lm = modchain;
  while (lm) {
   if (lm->source && !strcmp(lm->source, modinfo->rid)) {
/*    char tnotice[512];
    snprintf (tnotice, 512, "einit-mod-daemon: module already loaded: %s: updating\n", modinfo->rid);
    notice (5, tnotice);*/

//    free (modinfo->rid);
    lm->param = (void *)dexec;
    lm->enable = (int (*)(void *, struct einit_event *))enable;
    lm->disable = (int (*)(void *, struct einit_event *))disable;
    lm->cleanup = cleanup_after_module;
    lm->module = modinfo;

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
    new->enable = (int (*)(void *, struct einit_event *))enable;
    new->disable = (int (*)(void *, struct einit_event *))disable;
/*    new->reset = (int (*)(void *, struct einit_event *))NULL;
    new->reload = (int (*)(void *, struct einit_event *))NULL;*/
    new->cleanup = cleanup_after_module;
   }
  }
 }
}

int enable (struct dexecinfo *dexec, struct einit_event *status) {
 return startdaemon (dexec, status);
}

int disable (struct dexecinfo *dexec, struct einit_event *status) {
 return stopdaemon (dexec, status);
}
