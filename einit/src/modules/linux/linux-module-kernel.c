/*
 *  linux-module-kernel.c
 *  einit
 *
 *  Created by Magnus Deininger on 30/05/2006.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <expat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/event.h>
#include <dirent.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>
#include <einit-modules/exec.h>

int linux_module_kernel_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_linux_module_kernel_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Linux Kernel Module Support",
 .rid       = "linux-module-kernel",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_module_kernel_configure,
 .configuration = NULL
};

module_register(einit_linux_module_kernel_self);

#endif

int linux_module_kernel_scanmodules (struct lmodule *);
int linux_module_kernel_enable (char **, struct einit_event *);
int linux_module_kernel_disable (char **, struct einit_event *);
int linux_module_kernel_module_cleanup (struct lmodule *);
int linux_module_kernel_module_configure (struct lmodule *);
int linux_module_kernel_cleanup (struct lmodule *);

void linux_module_kernel_add_update_group (char *groupname, char **elements, char *seq) {
 struct cfgnode newnode;
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "services-alias-%s", groupname);

 struct cfgnode *onode = cfg_getnode (tmp, NULL);
 if (onode) {
  char *jele = NULL;
  if (!onode->source || !strmatch (onode->source, self->rid)) {
   uint32_t i = 0;

   for (; onode->arbattrs[i]; i+=2) {
    if (strmatch (onode->arbattrs[i], "group")) {
     char **nele = str2set (':', onode->arbattrs[i+1]);

     nele = (char **)setcombine_nc ((void **)nele, (const void **)elements, SET_TYPE_STRING);

     jele = set2str (':', (const char **)nele);

     break;
    }
   }
  }

  if (!jele) {
   jele = set2str (':', (const char **)elements);
  }

  char **oarb = onode->arbattrs;
  char **narb = NULL;

  narb = (char **)setadd ((void **)narb, (void *)"group", SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)jele, SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)"seq", SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)seq, SET_TYPE_STRING);

  onode->arbattrs = narb;

  if (oarb) {
   free (oarb);
  }
  free (jele);

 } else {
  char *jele = set2str (':', (const char **)elements);
  memset (&newnode, 0, sizeof(struct cfgnode));

  newnode.id = estrdup (tmp);
  newnode.source = self->rid;
  newnode.type = einit_node_regular;

  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"group", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)jele, SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"seq", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)seq, SET_TYPE_STRING);

  cfg_addnode (&newnode);
  free (jele);
 }
}

int linux_module_kernel_scanmodules (struct lmodule *mainlist) {
 struct stree *linux_module_kernel_nodes = cfg_prefix("configuration-kernel-modules-");

 if (linux_module_kernel_nodes) {
  struct stree *cur = linux_module_kernel_nodes;
  char **modules_group = NULL;

  while (cur) {
   struct cfgnode *node = cur->value;
   if (node && node->svalue) {
    char tmp[BUFFERSIZE];
    struct smodule *sm = ecalloc (1, sizeof (struct smodule));
    char doop = 1;
    struct lmodule *lm;

    esprintf (tmp, BUFFERSIZE, "linux-kernel-%s", cur->key + 29);

    lm = mainlist;
    while (lm) {
     if (lm->source && strmatch(lm->source, tmp)) {
      if (lm->si && lm->si->provides)
       modules_group  = (char **)setadd ((void **)modules_group, lm->si->provides[0], SET_TYPE_STRING);

      if (lm->param) {
       void *t = lm->param;
       lm->param = str2set (':', node->svalue);
       free (t);
      } else
       lm->param = str2set (':', node->svalue);
      lm = mod_update (lm);

      doop = 0;
     }

     lm = lm->next;
    }

    if (doop) {
     sm->rid = estrdup (tmp);
     esprintf (tmp, BUFFERSIZE, "Linux Kernel Modules (%s)", cur->key + 29);
     sm->name = estrdup (tmp);

     esprintf (tmp, BUFFERSIZE, "kern-%s", cur->key + 29);
     sm->si.provides = (char **)setadd ((void **)sm->si.provides, tmp, SET_TYPE_STRING);

     sm->si.requires = (char **)setadd ((void **)sm->si.requires, "mount-system", SET_TYPE_STRING);

     modules_group  = (char **)setadd ((void **)modules_group, tmp, SET_TYPE_STRING);

     sm->configure = linux_module_kernel_module_configure;

     lm = mod_add (NULL, sm);
    }
   }

   cur = streenext (cur);
  }

  if (modules_group) {
   linux_module_kernel_add_update_group ("modules", modules_group, "most");
   free (modules_group);
  }

  streefree (linux_module_kernel_nodes);
 }

 return 0;
}


int linux_module_kernel_enable (char **modules, struct einit_event *status) {
 if (!modules) return status_failed;
 char *modprobe_command = cfg_getstring ("configuration-command-modprobe/with-env", 0);
 uint32_t i = 0;

 for (; modules[i]; i++) {
  const char *tpldata[] = { "module", modules[i], NULL };
  char *applied = apply_variables (modprobe_command, tpldata);

  if (applied) {
   fbprintf (status, "loading kernel module: %s", modules[i]);

   if (pexec (applied, NULL, 0, 0, NULL, NULL, NULL, status) & status_failed) {
    status->flag++;
    fbprintf (status, "loading kernel module \"%s\" failed", modules[i]);
   }
  }
 }

 return status_ok;
}

int linux_module_kernel_disable (char **modules, struct einit_event *status) {
 if (!modules) return status_failed;
 char *modprobe_command = cfg_getstring ("configuration-command-rmmod/with-env", 0);
 uint32_t i = 0;

 for (; modules[i]; i++) {
  const char *tpldata[] = { "module", modules[i], NULL };
  char *applied = apply_variables (modprobe_command, tpldata);

  if (applied) {
   fbprintf (status, "unloading kernel module: %s", modules[i]);

   if (pexec (applied, NULL, 0, 0, NULL, NULL, NULL, status) & status_failed) {
    status->flag++;
    fbprintf (status, "unloading kernel module \"%s\" failed", modules[i]);
   }
  }
 }

 return status_ok;
}

int linux_module_kernel_cleanup (struct lmodule *this) {
 exec_cleanup (this);

 return 0;
}

int linux_module_kernel_module_cleanup (struct lmodule *this) {
 if (this->param) free (this->param);

 return 0;
}

int linux_module_kernel_module_configure (struct lmodule *tm) {
 char tmp[BUFFERSIZE];
 struct cfgnode *node;

 if (!tm->module || !tm->module->rid) return 1;

 esprintf (tmp, BUFFERSIZE, "configuration-kernel-modules-%s", tm->module->rid +13);

 node = cfg_getnode (tmp, NULL);
 if (node && node->svalue) {
  if (tm->param) free (tm->param);
  tm->param = str2set (':', node->svalue);
 }

 tm->cleanup = linux_module_kernel_module_cleanup;
 tm->enable = (int (*)(void *, struct einit_event *))linux_module_kernel_enable;
 tm->disable = (int (*)(void *, struct einit_event *))linux_module_kernel_disable;

 tm->source = estrdup(tm->module->rid);

 return 0;
}

int linux_module_kernel_configure (struct lmodule *this) {
 module_init(this);
 exec_configure (this);

 thismodule->cleanup = linux_module_kernel_cleanup;
 thismodule->scanmodules = linux_module_kernel_scanmodules;

 return 0;
}
