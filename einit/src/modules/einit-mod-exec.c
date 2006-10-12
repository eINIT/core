/*
 *  einit-mod-exec.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/04/2006.
 *  Renamed from mod-exec.c on 11/10/2006.
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
#include <signal.h>
#include <pthread.h>
#include <einit/pexec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

struct mexecinfo {
 char *enable;
 char *disable;
 char *reset;
 char *reload;
 char **variables;
 char **environment;
 uid_t uid;
 gid_t gid;
 char *user, *group;
};

char *provides[] = {"exec", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_LOADER,
	.options	= 0,
	.name		= "shell-command-pseudo-module support",
	.rid		= "einit-mod-exec",
	.provides	= provides,
	.requires	= NULL,
	.notwith	= NULL
};

int scanmodules (struct lmodule *);
int pexec_wrapper (struct mexecinfo *, struct einit_event *);
int configure (struct lmodule *);

int examine_configuration (struct lmodule *irr) {
 int pr = 0;

 if (!cfg_getnode("shell", NULL)) {
  fputs (" * configuration variable \"shell\" not found.\n", stderr);
  pr++;
 }

 return pr;
}

int configure (struct lmodule *irr) {
 pexec_configure (irr);
}

int cleanup (struct lmodule *this) {
 pexec_cleanup(this);
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
  if (((struct mexecinfo *)(this->param))->variables)
   free (((struct mexecinfo *)(this->param))->variables);
  if (((struct mexecinfo *)(this->param))->environment)
   free (((struct mexecinfo *)(this->param))->environment);
  free (this->param);
 }
}

int scanmodules (struct lmodule *modchain) {
 struct cfgnode *node;

 node = NULL;
 while (node = cfg_findnode ("mod-shell", 0, node)) {
  struct smodule *modinfo = ecalloc (1, sizeof (struct smodule));
  struct mexecinfo *mexec = ecalloc (1, sizeof (struct mexecinfo));
  int i = 0;
  if (!node->arbattrs) continue;
  for (; node->arbattrs[i]; i+=2 ) {
   if (!strcmp (node->arbattrs[i], "id"))
    modinfo->rid = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "name"))
    modinfo->name = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "enable"))
    mexec->enable = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "disable"))
    mexec->disable = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "reset"))
    mexec->reset = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "reload"))
    mexec->reload = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "uid"))
    mexec->uid = atoi(node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "gid"))
    mexec->gid = atoi(node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "user"))
    mexec->user = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "group"))
    mexec->group = node->arbattrs[i+1];
   else if (!strcmp (node->arbattrs[i], "requires"))
    modinfo->requires = str2set (':', node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "provides"))
    modinfo->provides = str2set (':', node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "notwith"))
    modinfo->notwith = str2set (':', node->arbattrs[i+1]);
   else if (!strcmp (node->arbattrs[i], "variables"))
    mexec->variables = str2set (':', node->arbattrs[i+1]);
   else
    mexec->environment = straddtoenviron (mexec->environment, node->arbattrs[i], node->arbattrs[i+1]);
  }
  mod_add (NULL,
           (int (*)(void *, struct einit_event *))pexec_wrapper, /* enable */
           (int (*)(void *, struct einit_event *))pexec_wrapper, /* disable */
           (int (*)(void *, struct einit_event *))pexec_wrapper, /* reset */
           (int (*)(void *, struct einit_event *))pexec_wrapper, /* reload */
           cleanup_after_module,  /* cleanup */
           (void *)mexec, modinfo);
 }
}

int pexec_wrapper (struct mexecinfo *shellcmd, struct einit_event *status) {
 char *command;

 if (!shellcmd) return STATUS_FAIL;

 if (status->task & MOD_ENABLE) {
  command = shellcmd->enable;
 } else if (status->task & MOD_DISABLE) {
  command = shellcmd->disable;
 } else if (status->task & MOD_RESET) {
  command = shellcmd->disable;
 } else if (status->task & MOD_RELOAD) {
  command = shellcmd->disable;
 } else return STATUS_FAIL;

 if (!command) return STATUS_FAIL;

 return pexec (command, shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);
}
