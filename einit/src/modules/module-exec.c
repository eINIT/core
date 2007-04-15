/*
 *  module-exec.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/04/2006.
 *  Renamed from mod-exec.c on 11/10/2006.
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
#include <einit-modules/scheduler.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

struct mexecinfo {
 char *id;
 char *enable;
 char *disable;
 char *prepare;
 char *cleanup;
 char **variables;
 char **environment;
 uid_t uid;
 gid_t gid;
 char *user, *group;
 char *pidfile;
 char **oattrs;
};

int einit_mod_exec_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_mod_exec_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (Configuration, Shell-Script)",
 .rid       = "module-exec",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_mod_exec_configure
};

module_register(einit_mod_exec_self);

#endif

int einit_mod_exec_scanmodules (struct lmodule *);
int einit_mod_exec_pexec_wrapper (struct mexecinfo *, struct einit_event *);
int einit_mod_exec_pexec_wrapper_custom (struct mexecinfo *, char *, struct einit_event *);

struct mexecinfo **einit_mod_exec_mxdata = NULL;

void einit_mod_exec_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "examine") && strmatch(ev->argv[1], "configuration")) {
  if (!cfg_getnode("configuration-system-shell", NULL)) {
   eputs (" * configuration variable \"configuration-system-shell\" not found.\n", ev->output);
   ev->ipc_return++;
  }

  if (einit_mod_exec_mxdata) {
   uint32_t i = 0;
   for (i = 0; einit_mod_exec_mxdata[i]; i++) {
    if (einit_mod_exec_mxdata[i]->variables) {
     check_variables (einit_mod_exec_mxdata[i]->id, (const char **)einit_mod_exec_mxdata[i]->variables, ev->output);
    }
   }
  }

  ev->implemented = 1;
 }
}

int einit_mod_exec_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);
 event_ignore (einit_event_subsystem_ipc, einit_mod_exec_ipc_event_handler);

 return 0;
}

int einit_mod_exec_cleanup_after_module (struct lmodule *pa) {
 if (pa->param) {
  if (((struct mexecinfo *)(pa->param))->variables)
   free (((struct mexecinfo *)(pa->param))->variables);
  if (((struct mexecinfo *)(pa->param))->environment)
   free (((struct mexecinfo *)(pa->param))->environment);
  free (pa->param);
 }

 return 0;
}

int einit_mod_exec_scanmodules (struct lmodule *modchain) {
 struct cfgnode *node;

 node = NULL;
 while ((node = cfg_findnode ("services-virtual-module-shell", 0, node))) {
  struct smodule *modinfo = ecalloc (1, sizeof (struct smodule));
  struct mexecinfo *mexec = ecalloc (1, sizeof (struct mexecinfo));
  struct lmodule *new;
  int i = 0;
  char doop = 1;

  if (!node->arbattrs) continue;

  mexec->oattrs = (char **)node->arbattrs;

  for (; node->arbattrs[i]; i+=2 ) {
   if (strstr (node->arbattrs[i], "execute:") == node->arbattrs[i]) {
    continue;
   } else if (strmatch (node->arbattrs[i], "id")) {
    modinfo->rid = node->arbattrs[i+1];
    mexec->id = node->arbattrs[i+1];
   } else if (strmatch (node->arbattrs[i], "name"))
    modinfo->name = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "enable"))
    mexec->enable = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "disable"))
    mexec->disable = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "prepare"))
    mexec->prepare = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "cleanup"))
    mexec->cleanup = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "uid"))
    mexec->uid = atoi(node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "gid"))
    mexec->gid = atoi(node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "user"))
    mexec->user = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "group"))
    mexec->group = node->arbattrs[i+1];
   else if (strmatch (node->arbattrs[i], "pid")) {
    mexec->environment = straddtoenviron (mexec->environment, "pidfile", node->arbattrs[i+1]);
    mexec->pidfile = node->arbattrs[i+1];
   }

   else if (strmatch (node->arbattrs[i], "options")) {
    char **opt = str2set (':', node->arbattrs[i+1]);
    uint32_t ri = 0;

    for (; opt[ri]; ri++) {
     if (strmatch (opt[ri], "feedback"))
      modinfo->mode |= einit_module_feedback;
    }
   }

   else if (strmatch (node->arbattrs[i], "requires"))
    modinfo->si.requires = str2set (':', node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "provides")) {
    modinfo->si.provides = str2set (':', node->arbattrs[i+1]);

    mexec->environment = straddtoenviron (mexec->environment, "services", node->arbattrs[i+1]);
   } else if (strmatch (node->arbattrs[i], "after"))
    modinfo->si.after = str2set (':', node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "before"))
    modinfo->si.before = str2set (':', node->arbattrs[i+1]);
   else if (strmatch (node->arbattrs[i], "variables"))
    mexec->variables = str2set (':', node->arbattrs[i+1]);
   else
    mexec->environment = straddtoenviron (mexec->environment, node->arbattrs[i], node->arbattrs[i+1]);
  }

  if (einit_mod_exec_mxdata) {
   uint32_t u = 0;
   char add = 1;
   for (u = 0; einit_mod_exec_mxdata[u]; u++) {
    if (strmatch (einit_mod_exec_mxdata[u]->id, mexec->id)) {
     add = 0;
     einit_mod_exec_mxdata[u] = mexec;
     break;
    }
   }
   if (add) {
    einit_mod_exec_mxdata = (struct mexecinfo **)setadd ((void **)einit_mod_exec_mxdata, (void *)mexec, SET_NOALLOC);
   }
  } else
   einit_mod_exec_mxdata = (struct mexecinfo **)setadd ((void **)einit_mod_exec_mxdata, (void *)mexec, SET_NOALLOC);

  if (!modinfo->rid) continue;

  struct lmodule *lm = modchain;
  while (lm) {
   if (lm->source && strmatch(lm->source, modinfo->rid)) {
    lm->param = (void *)mexec;
    lm->enable = (int (*)(void *, struct einit_event *))einit_mod_exec_pexec_wrapper;
    lm->disable = (int (*)(void *, struct einit_event *))einit_mod_exec_pexec_wrapper;
    lm->custom = (int (*)(void *, char *, struct einit_event *))einit_mod_exec_pexec_wrapper_custom;
    lm->cleanup = einit_mod_exec_cleanup_after_module;
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
    new->param = (void *)mexec;
    new->enable = (int (*)(void *, struct einit_event *))einit_mod_exec_pexec_wrapper;
    new->disable = (int (*)(void *, struct einit_event *))einit_mod_exec_pexec_wrapper;
    new->custom = (int (*)(void *, char *, struct einit_event *))einit_mod_exec_pexec_wrapper_custom;
    new->cleanup = einit_mod_exec_cleanup_after_module;
   }
  }
 }

 return 0;
}

int einit_mod_exec_pexec_wrapper_custom (struct mexecinfo *shellcmd, char *command, struct einit_event *status) {
 if (strmatch (command, "zap"))
  return status_ok | status_disabled;
 else if (shellcmd->oattrs) {
  char tmp[BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "execute:%s", command);

  uint32_t i = 0;

  for (; shellcmd->oattrs[i]; i+=2 ) {
   if (strmatch (shellcmd->oattrs[i], tmp)) {
    return status_enabled | pexec (shellcmd->oattrs[i+1], (const char **)shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);
   }
  }
 }

/* if (task & MOD_RESET) {
  if (shellcmd->reset) {
   retval = pexec (shellcmd->reset, (const char **)shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);
  } else {
   task = einit_module_enable | einit_module_disable;
  }
 } else if (task & MOD_RELOAD) {
  if (shellcmd->reload) {
   retval = pexec (shellcmd->reload, (const char **)shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);
  } else {
   task = einit_module_enable | einit_module_disable;
  }
 }

// + ZAP */

 return status_failed | status_command_not_implemented;
}

int einit_mod_exec_pexec_wrapper (struct mexecinfo *shellcmd, struct einit_event *status) {
 int retval = status_failed;

 if (shellcmd) {
  int32_t task = status->task;

  if (task & einit_module_disable) {
   if (shellcmd->disable) {
    retval = pexec (shellcmd->disable, (const char **)shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);

    if (retval & status_ok) {
     if (shellcmd->cleanup) {
      pexec (shellcmd->cleanup, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);
     }

     if (shellcmd->pidfile) {
      unlink (shellcmd->pidfile);
      errno = 0;
     }
    }
   }
  }
  if (task & einit_module_enable) {
   if (shellcmd->enable) {
    if (shellcmd->pidfile) {
     unlink (shellcmd->pidfile);
     errno = 0;
    }

    if (shellcmd->prepare) {
     pexec (shellcmd->prepare, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);
    }

    retval = pexec (shellcmd->enable, (const char **)shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);

    if ((retval == status_failed) && shellcmd->cleanup)
     pexec (shellcmd->cleanup, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);
   }
  }
 }

 return retval;
}

int einit_mod_exec_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = einit_mod_exec_scanmodules;
 pa->cleanup = einit_mod_exec_cleanup;

 exec_configure (pa);
 event_listen (einit_event_subsystem_ipc, einit_mod_exec_ipc_event_handler);

 return 0;
}
