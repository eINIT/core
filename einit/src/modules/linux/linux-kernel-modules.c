/*
 *  linux-kernel-modules.c
 *  einit
 *
 *  Moved from linux-module-kernel.c on 22/09/2007
 *  Copyright 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

int linux_kernel_modules_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_linux_kernel_modules_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Linux Kernel Module Support",
 .rid       = "linux-kernel-modules",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_kernel_modules_configure,
 .configuration = NULL
};

module_register(einit_linux_kernel_modules_self);

#endif

int linux_kernel_modules_module_configure (struct lmodule *);
int linux_kernel_modules_cleanup (struct lmodule *);

void *linux_kernel_modules_load_exec (void *c) {
 pexec (c, NULL, 0, 0, NULL, NULL, NULL, NULL);

 return NULL;
}

int linux_kernel_modules_load (char **modules) {
 if (!modules) return status_failed;
 pthread_t **threads = NULL;

 char *modprobe_command = cfg_getstring ("configuration-command-modprobe/with-env", 0);
 uint32_t i = 0;

 for (; modules[i]; i++) if (strcmp ("", modules[i])) {
  const char *tpldata[] = { "module", modules[i], NULL };
  char *applied = apply_variables (modprobe_command, tpldata);

  if (applied) {
   notice (4, "loading kernel module: %s", modules[i]);

   if (modules[i+1]) {
    pthread_t *threadid = emalloc (sizeof (pthread_t));

    if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load_exec, applied)) {
     linux_kernel_modules_load_exec (applied);
    } else {
     threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
    }
   } else {
    linux_kernel_modules_load_exec (applied);
   }
  }
 }

 free (modules);

 if (threads) {
  int i = 0;

  for (; threads[i]; i++) {
   pthread_join (*(threads[i]), NULL);

   free (threads[i]);
  }

  free (threads);
 }

 return status_ok;
}

char **linux_kernel_modules_autoload_d() {
 char **rv = NULL;
 char *file = cfg_getstring ("configuration-kernel-modules-autoload.d/file", NULL);
 if (file) {
  char *d = readfile (file);

  notice (4, "grabbing kernel modules from file \"%s\"", file);

  if (d) {
   char **t = str2set ('\n', d);
   int i = 0;

   for (; t[i]; i++) {
    char *module = t[i];
    strtrim (module);
    if ((module[0] != '#') && (module[0] != '\n') && (module[0] != '\r') && (module[0] != 0)) {
     rv = (char **)setadd ((void **)rv, module, SET_TYPE_STRING);
    }
   }

   free (t);
   free (d);
  }
 }

 return rv;
}

enum lkm_run_code {
 lkm_pre_dev,
 lkm_post_dev,
 lkm_shutdown
};

#define MPREFIX "configuration-kernel-modules-"

char **linux_kernel_modules_get_from_node (char *node, char *dwait) {
 int len = strlen (node) + sizeof(MPREFIX) + 1;
 char *buffer = emalloc (len);
 struct cfgnode *n;

 esprintf (buffer, len, MPREFIX "%s", node);

 n = cfg_getnode (buffer, NULL);
 if (n) {
  *dwait = !n->flag;
  return str2set (':', n->svalue);
 }

 return NULL;
}

#define KERNEL_MODULES_PATH_PREFIX "/lib/modules/"

char **linux_kernel_modules_storage() {
 return NULL;
}

char **linux_kernel_modules_sound() {
 return NULL;
}

char **linux_kernel_modules_get_by_subsystem (char *subsystem, char *dwait) {
 char **rv = NULL;

 if ((rv = linux_kernel_modules_get_from_node (subsystem, dwait))) {
  return rv;
 } else if (strmatch (subsystem, "generic") || strmatch (subsystem, "arbitrary")) {
  *dwait = 1;
  return linux_kernel_modules_autoload_d();
 } else if (strmatch (subsystem, "storage")) {
  *dwait = 1;
  return linux_kernel_modules_storage();
 } else if (strmatch (subsystem, "alsa") || strmatch (subsystem, "audio") || strmatch (subsystem, "sound")) {
  *dwait = 1;
  return linux_kernel_modules_sound();
 }

 return NULL;
}

int linux_kernel_modules_run (enum lkm_run_code code) {
 pthread_t **threads = NULL;

 if (code == lkm_pre_dev) {
  char dwait;
  char **modules = linux_kernel_modules_get_by_subsystem ("storage", &dwait);

  if (modules) {
   pthread_t *threadid = emalloc (sizeof (pthread_t));

   if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
    linux_kernel_modules_load (modules);
   } else {
    if (dwait)
     threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
   }
  }
 } else if (code == lkm_post_dev) {
  struct stree *linux_kernel_modules_nodes = cfg_prefix(MPREFIX);
  char have_generic = 0;
  char have_audio = 0;

  if (linux_kernel_modules_nodes) {
   struct stree *cur = linux_kernel_modules_nodes;

   while (cur) {
    char *subsystem = cur->key + sizeof (MPREFIX) -1;
    struct cfgnode *nod = cur->value;

    if (nod && nod->arbattrs) {
     size_t i;
     for (i = 0; nod->arbattrs[i]; i+=2) {
      if (strmatch (nod->arbattrs[i], "provide-service") && parse_boolean (nod->arbattrs[i+1])) {
       goto nextgroup;
      }
     }
    }

    if (strmatch (subsystem, "storage")) {
    } else {
     struct cfgnode *node = cur->value;

     if (strmatch (subsystem, "generic") || strmatch (subsystem, "arbitrary")) {
      have_generic = 1;
     } else if (strmatch (subsystem, "alsa") || strmatch (subsystem, "audio") || strmatch (subsystem, "sound")) {
      have_audio = 1;
     }

     if (node && node->svalue) {
      char **modules = str2set (':', node->svalue);

      if (modules) {
       pthread_t *threadid = emalloc (sizeof (pthread_t));

       if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
        linux_kernel_modules_load (modules);
       } else {
        if (!node->flag)
         threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
       }
      }
     }
    }

    nextgroup:

    cur = streenext (cur);
   }

   streefree (linux_kernel_modules_nodes);
  }

  if (!have_generic) {
   char dwait;
   char **modules = linux_kernel_modules_get_by_subsystem ("generic", &dwait);

   if (modules) {
    pthread_t *threadid = emalloc (sizeof (pthread_t));

    if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
     linux_kernel_modules_load (modules);
    } else {
     if (dwait)
      threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
    }
   }
  }

  if (!have_audio) {
   char dwait;
   char **modules = linux_kernel_modules_get_by_subsystem ("audio", &dwait);

   if (modules) {
    pthread_t *threadid = emalloc (sizeof (pthread_t));

    if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
     linux_kernel_modules_load (modules);
    } else {
     if (dwait)
      threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
    }
   }
  }
 }

 if (threads) {
  int i = 0;

  for (; threads[i]; i++) {
   pthread_join (*(threads[i]), NULL);

   free (threads[i]);
  }

  free (threads);
 }

 return status_ok;
}

void linux_kernel_modules_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   linux_kernel_modules_run(lkm_pre_dev);
   break;

  case einit_boot_load_kernel_extensions:
   linux_kernel_modules_run(lkm_post_dev);
   break;

   default: break;
 }
}

int linux_kernel_modules_module_enable (char *subsys, struct einit_event *status) {
 char dwait = 0;
 char **modules = linux_kernel_modules_get_by_subsystem (subsys, &dwait);

 if (modules) {
  pthread_t threadid;

  if (dwait) {
   linux_kernel_modules_load (modules);
  } else if (ethread_create (&threadid, &thread_attribute_detached, (void *(*)(void *))linux_kernel_modules_load, modules)) {
   linux_kernel_modules_load (modules);
  }
 }

 return status_ok;
}

int linux_kernel_modules_module_disable (char *p, struct einit_event *status) {
 return status_ok;
}

int linux_kernel_modules_module_configure (struct lmodule *this) {
 this->enable = (int (*)(void *, struct einit_event *))linux_kernel_modules_module_enable;
 this->disable = (int (*)(void *, struct einit_event *))linux_kernel_modules_module_disable;

 this->param = this->module->rid + 21;

 return 0;
}

int linux_kernel_modules_scanmodules (struct lmodule *lm) {
 struct stree *linux_kernel_modules_nodes = cfg_prefix(MPREFIX);

 if (linux_kernel_modules_nodes) {
  struct stree *cur = linux_kernel_modules_nodes;

  while (cur) {
   char *subsystem = cur->key + sizeof (MPREFIX) -1;
   char usegroup = 0;

   if (!strmatch (subsystem, "storage")) {
    struct cfgnode *nod = cur->value;

    if (nod && nod->arbattrs) {
     size_t i;
     for (i = 0; nod->arbattrs[i]; i+=2) {
      if (strmatch (nod->arbattrs[i], "provide-service") && parse_boolean (nod->arbattrs[i+1])) {
       usegroup = 1;
      }
     }
    }
   }

   if (usegroup) {
    char tmp[BUFFERSIZE];
    struct lmodule *m = lm;
    struct smodule *sm;

    esprintf (tmp, BUFFERSIZE, "linux-kernel-modules-%s", subsystem);

    while (m) {
     if (m->module && strmatch (m->module->rid, tmp)) {
      mod_update(m);
      goto nextgroup;
     }

     m = m->next;
    }

    sm = emalloc (sizeof(struct smodule));
    memset (sm, 0, sizeof (struct smodule));

    sm->rid = estrdup (tmp);

    esprintf (tmp, BUFFERSIZE, "Linux Kernel Modules (%s)", subsystem);
    sm->name = estrdup (tmp);

    sm->eiversion = EINIT_VERSION;
    sm->eibuild = BUILDNUMBER;
    sm->mode = einit_module_generic | einit_feedback_job;

    esprintf (tmp, BUFFERSIZE, "kern-%s", subsystem);
    sm->si.provides = (char **)setadd ((void **)sm->si.provides, tmp, SET_TYPE_STRING);

    sm->configure = linux_kernel_modules_module_configure;

    mod_add (NULL, sm);
   }

   nextgroup:

   cur = streenext(cur);
  }
 }

 return 0;
}

int linux_kernel_modules_cleanup (struct lmodule *this) {
 exec_cleanup (this);

 event_ignore (einit_event_subsystem_boot, linux_kernel_modules_boot_event_handler);

#if 0
 event_ignore (einit_event_subsystem_power, linux_kernel_modules_power_event_handler);
#endif

 return 0;
}

int linux_kernel_modules_configure (struct lmodule *this) {
 module_init(this);
 exec_configure (this);

 thismodule->cleanup = linux_kernel_modules_cleanup;
 thismodule->scanmodules = linux_kernel_modules_scanmodules;

 event_listen (einit_event_subsystem_boot, linux_kernel_modules_boot_event_handler);

#if 0
 event_listen (einit_event_subsystem_power, linux_kernel_modules_power_event_handler);
#endif

 return 0;
}
