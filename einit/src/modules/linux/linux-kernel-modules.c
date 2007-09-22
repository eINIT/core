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
 .mode      = einit_module_generic,
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

int linux_kernel_modules_load (char **modules) {
 if (!modules) return status_failed;
 char *modprobe_command = cfg_getstring ("configuration-command-modprobe/with-env", 0);
 uint32_t i = 0;

 for (; modules[i]; i++) {
  const char *tpldata[] = { "module", modules[i], NULL };
  char *applied = apply_variables (modprobe_command, tpldata);

  if (applied) {
   notice (4, "loading kernel module: %s", modules[i]);

   if (pexec (applied, NULL, 0, 0, NULL, NULL, NULL, NULL) & status_failed) {
    notice (2, "loading kernel module \"%s\" failed", modules[i]);
   }
  }
 }

 free (modules);
 return status_ok;
}

int linux_kernel_modules_unload (char **modules) {
 if (!modules) return status_failed;
 char *modprobe_command = cfg_getstring ("configuration-command-rmmod/with-env", 0);
 uint32_t i = 0;

 for (; modules[i]; i++) {
  const char *tpldata[] = { "module", modules[i], NULL };
  char *applied = apply_variables (modprobe_command, tpldata);

  if (applied) {
   notice (4, "unloading kernel module: %s", modules[i]);

   if (pexec (applied, NULL, 0, 0, NULL, NULL, NULL, NULL) & status_failed) {
    notice (2, "unloading kernel module \"%s\" failed", modules[i]);
   }
  }
 }

 free (modules);
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
  }
 }

 return rv;
}

int linux_kernel_modules_run (char shutdown) {
 pthread_t **threads = NULL;
 struct stree *linux_kernel_modules_nodes = cfg_prefix("configuration-kernel-modules-");

 if (linux_kernel_modules_nodes) {
  struct stree *cur = linux_kernel_modules_nodes;

  while (cur) {
   struct cfgnode *node = cur->value;
   if (node && node->svalue) {
    char **modules = str2set (':', node->svalue);

    if (modules) {
     if (shutdown) {
      pthread_t *threadid = emalloc (sizeof (pthread_t));

      if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_unload, modules)) {
       linux_kernel_modules_unload (modules);
      } else
       threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
     } else {
      pthread_t *threadid = emalloc (sizeof (pthread_t));

      if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
       linux_kernel_modules_load (modules);
      } else
       threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
     }
    }
   }

   cur = streenext (cur);
  }

  streefree (linux_kernel_modules_nodes);
 }

 char **kamodules = linux_kernel_modules_autoload_d();
 if (kamodules) {
  if (shutdown) {
   pthread_t *threadid = emalloc (sizeof (pthread_t));

   if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_unload, kamodules)) {
    linux_kernel_modules_unload (kamodules);
   } else
    threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
  } else {
   pthread_t *threadid = emalloc (sizeof (pthread_t));

   if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, kamodules)) {
    linux_kernel_modules_load (kamodules);
   } else
    threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
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

void linux_kernel_modules_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_imminent) || (ev->type == einit_power_reset_imminent)) {
  linux_kernel_modules_run(1);
 }
}

void linux_kernel_modules_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_load_kernel_extensions:
   linux_kernel_modules_run(0);

   /* some code */
   break;

   default: break;
 }
}

int linux_kernel_modules_cleanup (struct lmodule *this) {
 exec_cleanup (this);

 event_ignore (einit_event_subsystem_boot, linux_kernel_modules_boot_event_handler);
 event_ignore (einit_event_subsystem_power, linux_kernel_modules_power_event_handler);

 return 0;
}

int linux_kernel_modules_configure (struct lmodule *this) {
 module_init(this);
 exec_configure (this);

 thismodule->cleanup = linux_kernel_modules_cleanup;

 event_listen (einit_event_subsystem_boot, linux_kernel_modules_boot_event_handler);
 event_listen (einit_event_subsystem_power, linux_kernel_modules_power_event_handler);

 return 0;
}
