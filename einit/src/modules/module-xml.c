/*
 *  module-xml.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/04/2006.
 *  Renamed from mod-exec.c on 11/10/2006.
 *  Renamed from module-exec.c/Joined with module-daemon.c on 30/05/2006.
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
#include <einit-modules/process.h>
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

int einit_module_xml_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_module_xml_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (Configuration, Shell-Script)",
 .rid       = "einit-module-exec",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_module_xml_configure
};

module_register(einit_module_xml_self);

#endif

int einit_module_xml_scanmodules (struct lmodule *);
int einit_module_xml_pexec_wrapper (struct mexecinfo *, struct einit_event *);
int einit_module_xml_pexec_wrapper_custom (struct mexecinfo *, char *, struct einit_event *);

int einit_module_xml_daemon_enable (struct dexecinfo *dexec, struct einit_event *status);
int einit_module_xml_daemon_disable (struct dexecinfo *dexec, struct einit_event *status);
int einit_module_xml_daemon_custom (struct dexecinfo *dexec, char *command, struct einit_event *status);

struct mexecinfo **einit_module_xml_mxdata = NULL;
struct dexecinfo **einit_mod_daemon_dxdata = NULL;
struct lmodule **einit_module_xml_shutdown = NULL;

/*void einit_module_xml_einit_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_core_recover:
  default: break;
 }
}*/

void einit_module_xml_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "examine") && strmatch(ev->argv[1], "configuration")) {
  if (!cfg_getnode("configuration-system-shell", NULL)) {
   eputs (" * configuration variable \"configuration-system-shell\" not found.\n", ev->output);
   ev->ipc_return++;
  }

  if (einit_module_xml_mxdata) {
   uint32_t i = 0;
   for (i = 0; einit_module_xml_mxdata[i]; i++) {
    if (einit_module_xml_mxdata[i]->variables) {
     check_variables (einit_module_xml_mxdata[i]->id, (const char **)einit_module_xml_mxdata[i]->variables, ev->output);
    }
   }
  }

  if (einit_mod_daemon_dxdata) {
   uint32_t i = 0;
   for (i = 0; einit_mod_daemon_dxdata[i]; i++) {
    if (einit_mod_daemon_dxdata[i]->variables) {
     check_variables (einit_mod_daemon_dxdata[i]->id, (const char **)einit_mod_daemon_dxdata[i]->variables, ev->output);
    }
   }
  }

  ev->implemented = 1;
 }
}

void einit_module_xml_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_scheduled) || (ev->type == einit_power_reset_scheduled)) {
  if (einit_module_xml_shutdown) {
   uint32_t i = 0;

   notice (1, "shutdown initiated, calling pre-shutdown scripts");

   for (; einit_module_xml_shutdown[i]; i++) {
    if (einit_module_xml_shutdown[i]->status & status_enabled)
     mod(einit_module_custom, einit_module_xml_shutdown[i], "on-shutdown");
   }
  }
 }
}

int einit_module_xml_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);
 event_ignore (einit_event_subsystem_ipc, einit_module_xml_ipc_event_handler);
// event_ignore (einit_event_subsystem_core, einit_module_xml_einit_event_handler);
 event_ignore (einit_event_subsystem_power, einit_module_xml_power_event_handler);

 return 0;
}

int einit_module_xml_cleanup_after_module (struct lmodule *pa) {
 if (pa->param) {
  if (((struct mexecinfo *)(pa->param))->variables)
   free (((struct mexecinfo *)(pa->param))->variables);
  if (((struct mexecinfo *)(pa->param))->environment)
   free (((struct mexecinfo *)(pa->param))->environment);
  free (pa->param);
 }

 return 0;
}

int einit_module_xml_daemon_cleanup_after_module (struct lmodule *this) {
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

int einit_module_xml_recover_shell (struct lmodule *module) {
 struct mexecinfo *data = module->param;
 char *pidfile;

 if (!data) return status_failed;

 if (data->pidfile && (pidfile = readfile (data->pidfile))) {
  pid_t pid = parse_integer (pidfile);

  if (pidexists (pid)) {
   mod (einit_module_enable | einit_module_ignore_dependencies, module, NULL);
  }

  free (pidfile);
 }

 return status_ok;
}

int einit_module_xml_recover_daemon (struct lmodule *module) {
 struct dexecinfo *data = module->param;
 char *pidfile;

 if (!data) return status_failed;

 if (data->pidfile && (pidfile = readfile (data->pidfile))) {
  pid_t pid = parse_integer (pidfile);

  if (pidexists (pid)) {
   mod (einit_module_enable | einit_module_ignore_dependencies, module, NULL);
  }

  free (pidfile);
 }

 return status_ok;
}

int einit_module_xml_scanmodules (struct lmodule *modchain) {
 struct stree *virtual_module_nodes = cfg_prefix("services-virtual-module-");
 if (virtual_module_nodes) {
  struct stree *curnode = virtual_module_nodes;

  while (curnode) {
   struct cfgnode *node = curnode->value;
   char type_daemon = strmatch (curnode->key + 24, "daemon"), type_shell = !type_daemon && strmatch (curnode->key + 24, "shell");

   if ((type_daemon || type_shell) && node->arbattrs) {
    struct smodule *modinfo = emalloc (sizeof (struct smodule));
    struct mexecinfo *mexec = type_shell ? ecalloc (1, sizeof (struct mexecinfo)) : NULL;
    struct dexecinfo *dexec = type_daemon ? ecalloc (1, sizeof (struct dexecinfo)) : NULL;
    uint32_t i = 0;
    char doop = 1, shutdownaction = 0;
    char **custom_hooks = NULL;
    struct lmodule *new;

    memset (modinfo, 0, sizeof (struct smodule));

    if (type_shell)
     mexec->oattrs = node->arbattrs;

    if (type_daemon)
     dexec->oattrs = node->arbattrs;

    for (; node->arbattrs[i]; i+=2 ) {
     if (strstr (node->arbattrs[i], "execute:") == node->arbattrs[i]) {
      if (strmatch (node->arbattrs[i], "execute:on-shutdown"))
       shutdownaction = 1;

      custom_hooks = (char **)setadd ((void **)custom_hooks, (node->arbattrs[i]) + 8, SET_TYPE_STRING);

      continue;
     } else if (strmatch (node->arbattrs[i], "id")) {
      modinfo->rid = node->arbattrs[i+1];
      if (type_shell) mexec->id = node->arbattrs[i+1];
      else dexec->id = node->arbattrs[i+1];
     } else if (strmatch (node->arbattrs[i], "name"))
      modinfo->name = node->arbattrs[i+1];
     else if (type_shell && strmatch (node->arbattrs[i], "enable"))
      mexec->enable = node->arbattrs[i+1];
     else if (type_shell && strmatch (node->arbattrs[i], "disable"))
      mexec->disable = node->arbattrs[i+1];
     else if (type_daemon && strmatch (node->arbattrs[i], "command"))
      dexec->command = node->arbattrs[i+1];
     else if (type_daemon && strmatch (node->arbattrs[i], "is-up"))
      dexec->is_up = node->arbattrs[i+1];
     else if (type_daemon && strmatch (node->arbattrs[i], "is-down"))
      dexec->is_down = node->arbattrs[i+1];
     else if (type_daemon && strmatch("restart", node->arbattrs[i]))
      dexec->restart = parse_boolean(node->arbattrs[i+1]);
     else if (strmatch (node->arbattrs[i], "prepare")) {
      if (type_shell) mexec->prepare = node->arbattrs[i+1];
      else dexec->prepare = node->arbattrs[i+1];
     } else if (strmatch (node->arbattrs[i], "cleanup")) {
      if (type_shell) mexec->cleanup = node->arbattrs[i+1];
      else dexec->cleanup = node->arbattrs[i+1];
     } else if (strmatch (node->arbattrs[i], "uid")) {
      if (type_shell) mexec->uid = atoi(node->arbattrs[i+1]);
      else dexec->uid = atoi(node->arbattrs[i+1]);
     } else if (strmatch (node->arbattrs[i], "gid")) {
      if (type_shell) mexec->gid = atoi(node->arbattrs[i+1]);
      else dexec->gid = atoi(node->arbattrs[i+1]);
     } else if (strmatch (node->arbattrs[i], "user")) {
      if (type_shell) mexec->user = node->arbattrs[i+1];
      else dexec->user = node->arbattrs[i+1];
     } else if (strmatch (node->arbattrs[i], "group")) {
      if (type_shell) mexec->group = node->arbattrs[i+1];
      else dexec->group = node->arbattrs[i+1];
     } else if (strmatch (node->arbattrs[i], "pid")) {
      if (type_shell) {
       mexec->environment = straddtoenviron (mexec->environment, "pidfile", node->arbattrs[i+1]);
       mexec->pidfile = node->arbattrs[i+1];
      } else {
       dexec->environment = straddtoenviron (dexec->environment, "pidfile", node->arbattrs[i+1]);
       dexec->pidfile = node->arbattrs[i+1];
      }
     }

     else if (strmatch (node->arbattrs[i], "options")) {
      char **opt = str2set (':', node->arbattrs[i+1]);
      uint32_t ri = 0;

      for (; opt[ri]; ri++) {
       if (strmatch (opt[ri], "feedback"))
        modinfo->mode |= einit_module_feedback;
      }

      if (type_daemon) {
       for (ri = 0; opt[ri]; ri++) {
        if (strmatch (opt[ri], "forking"))
         dexec->options |= daemon_model_forking;
       }
      }
     }

     else if (strmatch (node->arbattrs[i], "requires"))
      modinfo->si.requires = str2set (':', node->arbattrs[i+1]);
     else if (strmatch (node->arbattrs[i], "provides")) {
      modinfo->si.provides = str2set (':', node->arbattrs[i+1]);

      if (type_shell)
       mexec->environment = straddtoenviron (mexec->environment, "services", node->arbattrs[i+1]);
      else
       dexec->environment = straddtoenviron (dexec->environment, "services", node->arbattrs[i+1]);
     } else if (strmatch (node->arbattrs[i], "after"))
      modinfo->si.after = str2set (':', node->arbattrs[i+1]);
     else if (strmatch (node->arbattrs[i], "shutdown-after"))
      modinfo->si.shutdown_after = str2set (':', node->arbattrs[i+1]);
     else if (strmatch (node->arbattrs[i], "before"))
      modinfo->si.before = str2set (':', node->arbattrs[i+1]);
     else if (strmatch (node->arbattrs[i], "shutdown-before"))
      modinfo->si.shutdown_before = str2set (':', node->arbattrs[i+1]);
     else if (strmatch (node->arbattrs[i], "variables")) {
      if (type_shell)
       mexec->variables = str2set (':', node->arbattrs[i+1]);
      else
       dexec->variables = str2set (':', node->arbattrs[i+1]);
     } else if (type_daemon && strmatch (node->arbattrs[i], "need-files"))
       dexec->need_files = str2set (':', node->arbattrs[i+1]);
     else if (type_shell)
      mexec->environment = straddtoenviron (mexec->environment, node->arbattrs[i], node->arbattrs[i+1]);
     else
      dexec->environment = straddtoenviron (dexec->environment, node->arbattrs[i], node->arbattrs[i+1]);
    }

    if (type_shell) {
     if (einit_module_xml_mxdata) {
      uint32_t u = 0;
      char add = 1;
      for (u = 0; einit_module_xml_mxdata[u]; u++) {
       if (strmatch (einit_module_xml_mxdata[u]->id, mexec->id)) {
        add = 0;
        einit_module_xml_mxdata[u] = mexec;
        break;
       }
      }
      if (add) {
       einit_module_xml_mxdata = (struct mexecinfo **)setadd ((void **)einit_module_xml_mxdata, (void *)mexec, SET_NOALLOC);
      }
     } else
      einit_module_xml_mxdata = (struct mexecinfo **)setadd ((void **)einit_module_xml_mxdata, (void *)mexec, SET_NOALLOC);
    } else {
     if (einit_mod_daemon_dxdata) {
      uint32_t u = 0;
      char add = 1;
      for (u = 0; einit_mod_daemon_dxdata[u]; u++) {
       if (strmatch (einit_mod_daemon_dxdata[u]->id, dexec->id)) {
        add = 0;
        einit_mod_daemon_dxdata[u] = dexec;
        break;
       }
      }
      if (add)
       einit_mod_daemon_dxdata = (struct dexecinfo **)setadd ((void **)einit_mod_daemon_dxdata, (void *)dexec, SET_NOALLOC);
     } else
      einit_mod_daemon_dxdata = (struct dexecinfo **)setadd ((void **)einit_mod_daemon_dxdata, (void *)dexec, SET_NOALLOC);
    }

    if (!modinfo->rid) continue;

    struct lmodule *lm = modchain;
    while (lm) {
     if (lm->source && strmatch(lm->source, modinfo->rid)) {
      if (type_shell) {
       lm->param = (void *)mexec;
       lm->enable = (int (*)(void *, struct einit_event *))einit_module_xml_pexec_wrapper;
       lm->disable = (int (*)(void *, struct einit_event *))einit_module_xml_pexec_wrapper;
       lm->custom = (int (*)(void *, char *, struct einit_event *))einit_module_xml_pexec_wrapper_custom;
       lm->cleanup = einit_module_xml_cleanup_after_module;
      } else {
       lm->param = (void *)dexec;
       lm->enable = (int (*)(void *, struct einit_event *))einit_module_xml_daemon_enable;
       lm->disable = (int (*)(void *, struct einit_event *))einit_module_xml_daemon_disable;
       lm->custom = (int (*)(void *, char *, struct einit_event *))einit_module_xml_daemon_custom;
       lm->cleanup = einit_module_xml_daemon_cleanup_after_module;
      }
      lm->module = modinfo;

      lm->functions = custom_hooks ? (char **)setdup((const void **)custom_hooks, SET_TYPE_STRING) : NULL;

      lm->recover = type_shell ? einit_module_xml_recover_shell : einit_module_xml_recover_daemon;

      lm = mod_update (lm);
      doop = 0;

      if (shutdownaction && !inset ((const void **)einit_module_xml_shutdown, (void *)lm, SET_NOALLOC)) {
       einit_module_xml_shutdown = (struct lmodule **)setadd ((void **)einit_module_xml_shutdown, (void *)lm, SET_NOALLOC);
      }

      break;
     }
     lm = lm->next;
    }

    if (doop) {
     new = mod_add (NULL, modinfo);
     if (new) {
      new->source = estrdup (modinfo->rid);
      if (type_shell) {
       new->param = (void *)mexec;
       new->enable = (int (*)(void *, struct einit_event *))einit_module_xml_pexec_wrapper;
       new->disable = (int (*)(void *, struct einit_event *))einit_module_xml_pexec_wrapper;
       new->custom = (int (*)(void *, char *, struct einit_event *))einit_module_xml_pexec_wrapper_custom;
       new->cleanup = einit_module_xml_cleanup_after_module;
      } else {
       new->param = (void *)dexec;
       new->enable = (int (*)(void *, struct einit_event *))einit_module_xml_daemon_enable;
       new->disable = (int (*)(void *, struct einit_event *))einit_module_xml_daemon_disable;
       new->custom = (int (*)(void *, char *, struct einit_event *))einit_module_xml_daemon_custom;
       new->cleanup = einit_module_xml_daemon_cleanup_after_module;
      }

      new->functions = custom_hooks ? (char **)setdup((const void **)custom_hooks, SET_TYPE_STRING) : NULL;

      if (shutdownaction && !inset ((const void **)einit_module_xml_shutdown, (void *)new, SET_NOALLOC)) {
       einit_module_xml_shutdown = (struct lmodule **)setadd ((void **)einit_module_xml_shutdown, (void *)new, SET_NOALLOC);
      }

      new->recover = type_shell ? einit_module_xml_recover_shell : einit_module_xml_recover_daemon;
     }
    }
   }

   curnode = curnode->next;
  }

  streefree (virtual_module_nodes);
 }

 return 0;
}

int einit_module_xml_daemon_enable (struct dexecinfo *dexec, struct einit_event *status) {
 return startdaemon (dexec, status);
}

int einit_module_xml_daemon_disable (struct dexecinfo *dexec, struct einit_event *status) {
 return stopdaemon (dexec, status);
}

int einit_module_xml_custom (char **arbattrs, char *command, struct einit_event *status, char **variables, uid_t uid, gid_t gid, char *user, char *group, char **environment) {
 if (arbattrs) {
  char tmp[BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "execute:%s", command);

  uint32_t i = 0;

  for (; arbattrs[i]; i+=2 ) {
   if (strmatch (arbattrs[i], tmp)) {
    return pexec (arbattrs[i+1], (const char **)variables, uid, gid, user, group, environment, status);
   }
  }
 }

 return status_failed | status_command_not_implemented;
}

int einit_module_xml_daemon_custom (struct dexecinfo *dexec, char *command, struct einit_event *status) {
 return ((status->module->status & status_enabled) ? status_enabled : status_disabled) | einit_module_xml_custom (dexec->oattrs, command, status, dexec->variables, dexec->uid, dexec->gid, dexec->user, dexec->group, dexec->environment);
}

int einit_module_xml_pexec_wrapper_custom (struct mexecinfo *shellcmd, char *command, struct einit_event *status) {
 return ((status->module->status & status_enabled) ? status_enabled : status_disabled) | einit_module_xml_custom (shellcmd->oattrs, command, status, shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment);
}


int einit_module_xml_pexec_wrapper (struct mexecinfo *shellcmd, struct einit_event *status) {
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
   char *pidfile = NULL;
   if (shellcmd->pidfile && (pidfile = readfile (shellcmd->pidfile))) {
    pid_t pid = parse_integer (pidfile);

    free (pidfile);
    pidfile = NULL;

    if (pidexists (pid)) {
     fbprintf (status, "Module's PID-file already exists and is valid.");

     return status_ok;
    }
   }

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

int einit_module_xml_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = einit_module_xml_scanmodules;
 pa->cleanup = einit_module_xml_cleanup;

 exec_configure (pa);
 event_listen (einit_event_subsystem_ipc, einit_module_xml_ipc_event_handler);
// event_listen (einit_event_subsystem_core, einit_module_xml_einit_event_handler);
 event_listen (einit_event_subsystem_power, einit_module_xml_power_event_handler);

 return 0;
}
