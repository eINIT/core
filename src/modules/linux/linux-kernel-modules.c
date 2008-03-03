/*
 *  linux-kernel-modules.c
 *  einit
 *
 *  Moved from linux-module-kernel.c on 22/09/2007
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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
#include <ctype.h>

int linux_kernel_modules_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_linux_kernel_modules_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
 .name      = "Linux Kernel Module Support",
 .rid       = "linux-kernel-modules",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_kernel_modules_configure
};

module_register(einit_linux_kernel_modules_self);

#endif

int linux_kernel_modules_module_configure (struct lmodule *);
int linux_kernel_modules_cleanup (struct lmodule *);

char **linux_kernel_modules_get_modprobe_c () {
 char **rv = NULL;
 FILE *f = popen ("modprobe -c", "r");

 if (f) {
  char buffer[BUFFERSIZE];
  while (fgets (buffer, BUFFERSIZE, f)) {
   if (isalnum(buffer[0]) || (strtrim(buffer), isalnum(buffer[0])))
    rv = (char **)set_noa_add ((void **)rv, (void *)estrdup(buffer));
  }

  pclose (f);
 }

 return rv;
}

char **linux_kernel_modules_get_modprobe_l () {
 char **rv = NULL;
 FILE *f = popen ("modprobe -l", "r");

 if (f) {
  char buffer[BUFFERSIZE];
  while (fgets (buffer, BUFFERSIZE, f)) {
   rv = (char **)set_noa_add ((void **)rv, (void *)estrdup(buffer));
  }

  pclose (f);
 }

 return rv;
}

void linux_kernel_modules_free_modprobe (char **r) {
 if (!r) return;
 int i = 0;
 for (; r[i]; i++) {
  efree (r[i]);
 }
 efree (r);
}

void *linux_kernel_modules_load_exec (void *c) {
 pexec (c, NULL, 0, 0, NULL, NULL, NULL, NULL);

 return NULL;
}

int linux_kernel_modules_load (char **modules) {
 if (!modules) return status_failed;
 pthread_t **threads = NULL;

 char *modprobe_command = cfg_getstring ("configuration-command-modprobe/with-env", 0);
 uint32_t i = 0;

 struct cfgnode *pn = cfg_getnode ("configuration-kernel-modules-load-in-parallel", NULL);

 if (pn && pn->flag) { /* load in parallel */
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
      threads = (pthread_t **)set_noa_add ((void **)threads, threadid);
     }
    } else {
     linux_kernel_modules_load_exec (applied);
    }
   }
  }
 } else { /* load in sequence */
  for (; modules[i]; i++) if (strcmp ("", modules[i])) {
   const char *tpldata[] = { "module", modules[i], NULL };
   char *applied = apply_variables (modprobe_command, tpldata);

   if (applied) {
    notice (4, "loading kernel module: %s", modules[i]);
    linux_kernel_modules_load_exec (applied);
   }
  }
 }

 efree (modules);

 if (threads) {
  int i = 0;

  for (; threads[i]; i++) {
   pthread_join (*(threads[i]), NULL);

   efree (threads[i]);
  }

  efree (threads);
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
     rv = set_str_add (rv, module);
    }
   }

   efree (t);
   efree (d);
  }
 }

 return rv;
}

enum lkm_run_code {
 lkm_pre_dev,
 lkm_post_dev,
 lkm_initramfs,
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

char **linux_kernel_modules_storage(struct einit_event *status) {
 return NULL;
}

char **linux_kernel_modules_sound(struct einit_event *status) {
 char **modprobe_c = linux_kernel_modules_get_modprobe_c();
 char **rv = NULL;
 struct stat st;

 struct cfgnode *n = cfg_getnode ("configuration-services-alsa-oss-emulation", NULL);

 char oss_emulation = n && n->svalue && strcmp (n->svalue, "no");

 if (modprobe_c) {
  int i;

  for (i = 0; modprobe_c[i]; i++) {
   char **x = str2set_by_whitespace(modprobe_c[i]);

   if (x) {
    if (x[0] && x[1] && x[2] && !x[3] && strmatch (x[0], "alias") && strprefix(x[1], "snd-card-") && !inset((const void **)rv, x[2], SET_TYPE_STRING)) {
     rv = set_str_add (rv, x[2]);
    }
    efree (x);
   }
  }

  char *b = NULL;
  if (!rv && (stat ("/proc/asound/cards", &st) || !(b = readfile ("/proc/asound/cards")) || (strtrim (b), strmatch (b, "--- no soundcards ---")))) {
   if (status) {
    fbprintf (status, "Could not detect custom ALSA settings, loading all detected ALSA drivers.");
   } else {
    notice (4, "linux_kernel_modules_sound(): Could not detect custom ALSA settings, loading all detected ALSA drivers.");
   }

   for (i = 0; modprobe_c[i]; i++) {
    char **x = str2set_by_whitespace(modprobe_c[i]);

    if (x) {
     if (x[0] && x[1] && x[2] && !x[3] && strmatch (x[0], "alias") && strprefix(x[1], "pci:") && strprefix(x[2], "snd") && !inset((const void **)rv, x[2], SET_TYPE_STRING)) {
      rv = set_str_add (rv, x[2]);
     }
     efree (x);
    }
   }
  }

  if (b) efree (b);

  if (!rv) {
   if (status) {
    fbprintf (status, "No ALSA drivers around, looking for OSS emulation modules anyway");
   } else {
    notice (4, "linux_kernel_modules_sound(): No ALSA drivers around, looking for OSS emulation modules anyway");
   }
  }

  char ** modprobe_l = linux_kernel_modules_get_modprobe_l();

  if (modprobe_l) {
   if (oss_emulation) {
    for (i = 0; modprobe_l[i]; i++) {
     b = strrchr (modprobe_l[i], '/');
     if (!b)
      b = modprobe_l[i];
     else
      b++;

     b = estrdup(b);

     char *lx = strrchr (b, '.');
     if (lx) {
      *lx = 0;
     }

     size_t len = strlen (b);
     if ((len > 7) && (b[0] == 's') && (b[1] == 'n') && (b[2] == 'd') && (b[len-3] == 'o') && (b[len-2] == 's') && (b[len-1] == 's') && !inset((const void **)rv, b, SET_TYPE_STRING)) {
      rv = set_str_add (rv, b);
     }

     efree (b);
    }
   } else {
    notice (4, "linux_kernel_modules_sound(): Not searching for OSS drivers");
   }

   for (i = 0; modprobe_l[i]; i++) {
    b = strrchr (modprobe_l[i], '/');
    if (!b)
     b = modprobe_l[i];
    else
     b++;

    b = estrdup(b);

    char *lx = strrchr (b, '.');
    if (lx) {
     *lx = 0;
    }

    if ((b[0] == 's') && (b[1] == 'n') && (b[2] == 'd') && b[3] && b[4] && b[5]) {
     if ((strmatch (b+4, "seq") || strmatch (b+4, "ioctl32")) && !inset((const void **)rv, b, SET_TYPE_STRING))
      rv = set_str_add (rv, b);
    }

    efree (b);
   }

   linux_kernel_modules_free_modprobe(modprobe_l);
  } else {
   if (status) {
    fbprintf (status, "only found partial modprobe data");
   } else {
    notice (1, "linux_kernel_modules_sound(): only found partial modprobe data");
   }
  }

  linux_kernel_modules_free_modprobe(modprobe_c);
 } else {
  if (status) {
   fbprintf (status, "no modprobe data");
  } else {
   notice (1, "linux_kernel_modules_sound(): no modprobe data");
  }
 }

 return rv;
}

char **linux_kernel_modules_get_by_subsystem (char *subsystem, char *dwait, struct einit_event *status) {
 char **rv = NULL;

 if ((rv = linux_kernel_modules_get_from_node (subsystem, dwait))) {
  return rv;
 } else if (strmatch (subsystem, "generic") || strmatch (subsystem, "arbitrary")) {
  *dwait = 1;
  return linux_kernel_modules_autoload_d();
 } else if (strmatch (subsystem, "storage")) {
  *dwait = 1;
  return linux_kernel_modules_storage(status);
 } else if (strmatch (subsystem, "alsa") || strmatch (subsystem, "audio") || strmatch (subsystem, "sound")) {
  *dwait = 1;
  return linux_kernel_modules_sound(status);
 }

 return NULL;
}

int linux_kernel_modules_run (enum lkm_run_code code) {
 pthread_t **threads = NULL;

 if (code == lkm_pre_dev) {
  char dwait;
  char **modules = linux_kernel_modules_get_by_subsystem ("storage", &dwait, NULL);

  if (modules) {
   pthread_t *threadid = emalloc (sizeof (pthread_t));

   if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
    linux_kernel_modules_load (modules);
   } else {
    if (dwait)
     threads = (pthread_t **)set_noa_add ((void **)threads, threadid);
   }
  }
 } else if (code == lkm_initramfs) {
  char dwait;
  char **modules = linux_kernel_modules_get_by_subsystem ("storage", &dwait, NULL);
  if (modules) {
  pthread_t *threadid = emalloc (sizeof (pthread_t));
  if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
   linux_kernel_modules_load (modules);
  } else {
   if (dwait)
    threads = (pthread_t **)set_noa_add ((void **)threads, threadid);
   }
  }
  modules = linux_kernel_modules_get_by_subsystem ("generic", &dwait, NULL);
  if (modules) {
   pthread_t *threadid = emalloc (sizeof (pthread_t));
   if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
    linux_kernel_modules_load (modules);
   } else {
    if (dwait)
     threads = (pthread_t **)set_noa_add ((void **)threads, threadid);
   }
  }
 } else if (code == lkm_post_dev) {
  struct stree *linux_kernel_modules_nodes = cfg_prefix(MPREFIX);
  char have_generic = 0;

  if (linux_kernel_modules_nodes) {
   struct stree *cur = streelinear_prepare(linux_kernel_modules_nodes);

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
      goto nextgroup;
     }

     if (node && node->svalue) {
      char **modules = str2set (':', node->svalue);

      if (modules) {
       pthread_t *threadid = emalloc (sizeof (pthread_t));

       if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
        linux_kernel_modules_load (modules);
       } else {
        if (!node->flag)
         threads = (pthread_t **)set_noa_add ((void **)threads, threadid);
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
   char **modules = linux_kernel_modules_get_by_subsystem ("generic", &dwait, NULL);

   if (modules) {
    pthread_t *threadid = emalloc (sizeof (pthread_t));

    if (ethread_create (threadid, NULL, (void *(*)(void *))linux_kernel_modules_load, modules)) {
     linux_kernel_modules_load (modules);
    } else {
     if (dwait)
      threads = (pthread_t **)set_noa_add ((void **)threads, threadid);
    }
   }
  }
 }

 if (threads) {
  int i = 0;

  for (; threads[i]; i++) {
   pthread_join (*(threads[i]), NULL);

   efree (threads[i]);
  }

  efree (threads);
 }

 return status_ok;
}

int linux_kernel_modules_module_enable (char *subsys, struct einit_event *status) {
 char dwait = 0;
 char **modules = linux_kernel_modules_get_by_subsystem (subsys, &dwait, status);

 if (modules) {
  char *n = set2str (' ', (const char **)modules);
  fbprintf (status, "loading kernel modules: %s", n);
  efree (n);

  if (dwait) {
   linux_kernel_modules_load (modules);
  } else {
   fbprintf (status, "not waiting for the result...");

   ethread_spawn_detached_run ((void *(*)(void *))linux_kernel_modules_load, (void *)modules);
  }
 } else {
  fbprintf (status, "no kernel modules to load");
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

void linux_kernel_modules_scanmodules_add_subsystem (char *subsystem) {
 char tmp[BUFFERSIZE];
 struct lmodule *m = NULL;
 struct smodule *sm;

 esprintf (tmp, BUFFERSIZE, "linux-kernel-modules-%s", subsystem);

 if ((m = mod_lookup_rid (tmp))) {
  mod_update(m);
  return;
 }

 sm = emalloc (sizeof(struct smodule));
 memset (sm, 0, sizeof (struct smodule));

 sm->rid = (char *)str_stabilise (tmp);

 esprintf (tmp, BUFFERSIZE, "Linux Kernel Modules (%s)", subsystem);
 sm->name = (char *)str_stabilise (tmp);

 sm->eiversion = EINIT_VERSION;
 sm->eibuild = BUILDNUMBER;
 sm->mode = einit_module | einit_feedback_job;

 esprintf (tmp, BUFFERSIZE, "kern-%s", subsystem);
 sm->si.provides = set_str_add (sm->si.provides, tmp);

 sm->configure = linux_kernel_modules_module_configure;

 mod_add (NULL, sm);
}

void linux_kernel_modules_scanmodules (struct einit_event *ev) {
 struct stree *linux_kernel_modules_nodes = cfg_prefix(MPREFIX);

 char have_audio = 0;

 if (linux_kernel_modules_nodes) {
  struct stree *cur = streelinear_prepare(linux_kernel_modules_nodes);

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
    linux_kernel_modules_scanmodules_add_subsystem (subsystem);

    if (strmatch (subsystem, "alsa") || strmatch (subsystem, "audio") || strmatch (subsystem, "sound")) {
     have_audio = 1;
    }
   }

   cur = streenext(cur);
  }
 }

 if (!have_audio) {
  linux_kernel_modules_scanmodules_add_subsystem ("alsa");
  linux_kernel_modules_scanmodules_add_subsystem ("audio");
  linux_kernel_modules_scanmodules_add_subsystem ("sound");
 }
}

void linux_kernel_modules_boot_event_handler_initramfs (struct einit_event *ev) {
 linux_kernel_modules_run(lkm_initramfs);
}

void linux_kernel_modules_boot_event_handler_early (struct einit_event *ev) {
 linux_kernel_modules_run(lkm_pre_dev);
}

void linux_kernel_modules_boot_event_handler_load_kernel_extensions (struct einit_event *ev) {
 linux_kernel_modules_run(lkm_post_dev);
}

int linux_kernel_modules_cleanup (struct lmodule *this) {
 exec_cleanup (this);

 event_ignore (einit_core_update_modules, linux_kernel_modules_scanmodules);

 event_ignore (einit_boot_initramfs, linux_kernel_modules_boot_event_handler_initramfs);
 event_ignore (einit_boot_early, linux_kernel_modules_boot_event_handler_early);
 event_ignore (einit_boot_load_kernel_extensions, linux_kernel_modules_boot_event_handler_load_kernel_extensions);

 return 0;
}

int linux_kernel_modules_configure (struct lmodule *this) {
 module_init(this);

 struct stree *linux_kernel_modules_nodes = cfg_prefix(MPREFIX);
 streefree (linux_kernel_modules_nodes);

 if (!linux_kernel_modules_nodes) {
  return status_configure_failed | status_not_in_use;
 }

 exec_configure (this);

 thismodule->cleanup = linux_kernel_modules_cleanup;

 event_listen (einit_core_update_modules, linux_kernel_modules_scanmodules);

 event_listen (einit_boot_initramfs, linux_kernel_modules_boot_event_handler_initramfs);
 event_listen (einit_boot_early, linux_kernel_modules_boot_event_handler_early);
 event_listen (einit_boot_load_kernel_extensions, linux_kernel_modules_boot_event_handler_load_kernel_extensions);

 linux_kernel_modules_scanmodules(NULL);

 return 0;
}
