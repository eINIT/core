/*
 *  compatibility-mod-sysv-init-d.c
 *  einit
 *
 *  Created by Magnus Deininger on 28/12/2006.
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
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>

#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int _compatibility_mod_sysv_init_d_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)
const struct smodule _compatibility_mod_sysv_init_d_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "System-V Compatibility: init.d Pseudo-Module Support",
 .rid       = "compatibility-mod-sysv-init-d",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
.configure = _compatibility_mod_sysv_init_d_configure
};

module_register(_compatibility_mod_sysv_init_d_self);

#endif

int _compatibility_mod_sysv_init_d_scanmodules (struct lmodule *);
int _compatibility_mod_sysv_init_d_init_d_enable (char *, struct einit_event *);
int _compatibility_mod_sysv_init_d_init_d_disable (char *, struct einit_event *);
int _compatibility_mod_sysv_init_d_init_d_custom (char *, char *, struct einit_event *);
int _compatibility_mod_sysv_init_d_configure (struct lmodule *);
int _compatibility_mod_sysv_init_d_cleanup (struct lmodule *);

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getstring("configuration-compatibility-sysv-init.d/path", NULL)) {
   fputs ("NOTICE: CV \"configuration-compatibility-sysv-init.d/path\":\n  Not found: Regular Init Scripts will not be processed. (not a problem)\n", (FILE *)ev->para);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int _compatibility_mod_sysv_init_d_cleanup (struct lmodule *irr) {
 exec_cleanup(irr);
 event_ignore (einit_event_subsystem_ipc, ipc_event_handler);
}

int _compatibility_mod_sysv_init_d_cleanup_after_module (struct lmodule *this) {
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
  free (this->param);
 }
#endif
}

int _compatibility_mod_sysv_init_d_scanmodules (struct lmodule *modchain) {
 DIR *dir;
 struct dirent *de;
 char *nrid = NULL,
      *init_d_path = cfg_getpath ("configuration-compatibility-sysv-init.d/path"),
      *tmp = NULL;
 uint32_t plen;
 struct smodule *modinfo;

 if (!init_d_path) return -1;

 plen = strlen (init_d_path) +1;

 if (dir = eopendir (init_d_path)) {
  while (de = ereaddir (dir)) {
   if (de->d_name[0] == '.') continue;
   char doop = 1;
//   puts (de->d_name);

   tmp = (char *)emalloc (plen + strlen (de->d_name));
   struct stat sbuf;
   struct lmodule *lm;
   *tmp = 0;
   strcat (tmp, init_d_path);
   strcat (tmp, de->d_name);
   if (!stat (tmp, &sbuf) && S_ISREG (sbuf.st_mode)) {
    char tmpx[BUFFERSIZE];

    modinfo = emalloc (sizeof (struct smodule));
    memset (modinfo, 0, sizeof(struct smodule));
    modinfo->mode |= einit_module_deprecated;

    nrid = emalloc (8 + strlen(de->d_name));
    *nrid = 0;
    strcat (nrid, "init-d-");
    strcat (nrid, de->d_name);

    snprintf (tmpx, BUFFERSIZE, "System-V-Style init.d Script (%s)", de->d_name);
    modinfo->name = estrdup (tmpx);
    modinfo->rid = estrdup(nrid);

    char *xt[2] = { (nrid + 7), NULL };

    modinfo->si.provides = (char **)setdup ((const void **)xt, SET_TYPE_STRING);

    struct lmodule *lm = modchain;
    while (lm) {
     if (lm->source && !strcmp(lm->source, tmp)) {
      lm->param = (void *)estrdup (tmp);
      lm->enable = (int (*)(void *, struct einit_event *))_compatibility_mod_sysv_init_d_init_d_enable;
      lm->disable = (int (*)(void *, struct einit_event *))_compatibility_mod_sysv_init_d_init_d_disable;
      lm->custom = (int (*)(void *, char *, struct einit_event *))_compatibility_mod_sysv_init_d_init_d_custom;
      lm->cleanup = _compatibility_mod_sysv_init_d_cleanup_after_module;
      lm->module = modinfo;

      lm = mod_update (lm);
      doop = 0;
      break;
     }
     lm = lm->next;
    }

    if (doop) {
     struct lmodule *new = mod_add (NULL, modinfo);
     if (new) {
      new->source = estrdup (tmp);
      new->param = (void *)estrdup (tmp);
      new->enable = (int (*)(void *, struct einit_event *))_compatibility_mod_sysv_init_d_init_d_enable;
      new->disable = (int (*)(void *, struct einit_event *))_compatibility_mod_sysv_init_d_init_d_disable;
      new->custom = (int (*)(void *, char *, struct einit_event *))_compatibility_mod_sysv_init_d_init_d_custom;
      new->cleanup = _compatibility_mod_sysv_init_d_cleanup_after_module;
     }
    }

   }
   if (nrid) free (nrid);
   if (tmp) free (tmp);
  }

  eclosedir (dir);
 }
}

int _compatibility_mod_sysv_init_d_init_d_enable (char *init_script, struct einit_event *status) {
 char *cmd;

 cmd = emalloc (7 + strlen(init_script));
 *cmd = 0;
 strcat (cmd, init_script);
 strcat (cmd, " start");

 return pexec (cmd, NULL, 0, 0, NULL, NULL, NULL, status);
}

int _compatibility_mod_sysv_init_d_init_d_disable (char *init_script, struct einit_event *status) {
 char *cmd;

 cmd = emalloc (6 + strlen(init_script));
 *cmd = 0;
 strcat (cmd, init_script);
 strcat (cmd, " stop");

 return pexec (cmd, NULL, 0, 0, NULL, NULL, NULL, status);
}

int _compatibility_mod_sysv_init_d_init_d_custom (char *init_script, char *custom_command, struct einit_event *status) {
 char *cmd;

 cmd = emalloc (strlen(init_script) + 2 + strlen (custom_command));
 *cmd = 0;
 strcat (cmd, init_script);
 strcat (cmd, " ");
 strcat (cmd, custom_command);

 return pexec (cmd, NULL, 0, 0, NULL, NULL, NULL, status);
}

int _compatibility_mod_sysv_init_d_configure (struct lmodule *irr) {
 module_init (irr);

 irr->cleanup = _compatibility_mod_sysv_init_d_cleanup;
 irr->scanmodules = _compatibility_mod_sysv_init_d_scanmodules;

 exec_configure (irr);
 event_listen (einit_event_subsystem_ipc, ipc_event_handler);
}
