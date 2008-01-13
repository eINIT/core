/*
 *  compile-c.c
 *  einit
 *
 *  Created on 29/08/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_c_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_c_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT Automatic C-Module Compiler",
 .rid       = "einit-compile-c",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_c_configure
};

module_register(module_c_self);

#endif

char module_c_firstrun = 1;
int module_c_usage = 0;

char module_c_compile_file (char *oname, char *base, char *nname) {
 char ret = 0;
 char *compiler_template = cfg_getstring ("subsystem-c-compile/c", NULL);
 if (compiler_template) {
  char **data = NULL;
  char *compiler_command;

  data = (char **)setadd ((void **)data, "configuration-file", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, EINIT_LIB_BASE "/scripts/configuration", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, "compile-options", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, "", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, "link-options", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, "", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, "module-source", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, oname, SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, "module-target", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, nname, SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, "module-basename", SET_TYPE_STRING);
  data = (char **)setadd ((void **)data, base, SET_TYPE_STRING);

  if ((compiler_command = apply_variables (compiler_template, (const char **)data))) {
   int srv;

   notice (1, "compiling: %s", oname);

   srv = system(compiler_command);

   if ((srv != -1) && (srv != 127)) {
    if (WIFEXITED(srv) && !(WEXITSTATUS(srv))) {
     ret = 1;
	}
   }

   efree (compiler_command);
  }

  efree (data);
 }

 return ret;
}

char module_c_update_modules () {
 char ret = 0;
 char *outputpath = cfg_getstring ("subsystem-c-compile-to", NULL);

 if (outputpath) {
  char **modules = NULL;
  ssize_t outputpath_len = strlen (outputpath);
  struct cfgnode *node = NULL;

  if (module_c_firstrun) {
   if (einit_argv) {
    uint32_t y = 0;

    for (; einit_argv[y]; y++) {
     if (strmatch (einit_argv[y], "--recompile")) {
      char **to_unlink = readdirfilter(NULL, outputpath, ".*\\.so$", NULL, 0);
      notice (3, "pruning cache and recompiling");

      if (to_unlink) {
       uint32_t x = 0;

       for (; to_unlink[x]; x++)
        unlink (to_unlink[x]);
      }
	 }
    }
   }

   module_c_firstrun = 0;
  }

  while ((node = cfg_findnode ("subsystem-c-sources", 0, node))) {
   char **nmodules = readdirfilter(node, "/lib/einit/modules-c/", ".*\\.(c|i|ii)$", NULL, 0);

   if (nmodules) {
    modules = (char **)setcombine_nc ((void **)modules, (const void **)nmodules, SET_TYPE_STRING);
   }
  }

  if (modules) {
   uint32_t i = 0;

   for (; modules[i]; i++) {
    char *oname = estrdup(modules[i]);
    char *base = strrchr (modules[i], '/');
	char *nbase = strrchr (base ? base : modules[i], '.');
	char *nname = NULL;
	ssize_t nname_len;
	char shift = 0;

    struct stat st1, st2;

    if (stat (oname, &st1)) {
     efree (oname); continue;
    }

    if (!base) base = modules[i];
	else {
	 base++;
	}
    if (!nbase) nbase = modules[i];
	else {
	 *nbase = 0;
	 nbase = base;
	}

    nname_len = outputpath_len + strlen (nbase) + 5;
    nname = emalloc (nname_len);

    esprintf (nname, nname_len, "%s/%s.so", outputpath, nbase);

    if ((nname[0] == '/') && (coremode & einit_mode_sandbox)) {
     nname++;
	 shift = 1;
	}

/* recompile if the the target file doesn't exist, or the source file is newer */
    if (stat (nname, &st2) ||
        (st1.st_mtime > st2.st_mtime)) {
     if (module_c_compile_file (oname, base, nname)) {
      ret = 1;
	 }
    }

    if (shift) nname--;

    efree (oname);
	efree (nname);
   }

   efree (modules);
  }
 }

 return ret;
}

void module_c_einit_event_handler_update_configuration (struct einit_event *ev) {
 module_c_usage++;

 if (module_c_update_modules()) {
  ev->chain_type = einit_core_configuration_update;
 }

 module_c_usage--;
}

int module_c_cleanup (struct lmodule *this) {
 event_ignore (einit_core_update_configuration, module_c_einit_event_handler_update_configuration);

 return 0;
}

int module_c_suspend (struct lmodule *this) {
 if (!module_c_usage) {
  event_wakeup (einit_core_update_configuration, this);
  event_ignore (einit_core_update_configuration, module_c_einit_event_handler_update_configuration);

  return status_ok;
 } else
  return status_failed;
}

int module_c_resume (struct lmodule *this) {
 event_wakeup_cancel (einit_core_update_configuration, this);

 return status_ok;
}

int module_c_configure (struct lmodule *irr) {
 module_init (irr);

 struct cfgnode *node = cfg_getnode ("subsystem-c-active", NULL);
 if (!node || !node->flag) { /* node needs to exist and explicitly say 'no' to disable this module */
  return status_configure_failed | status_not_in_use;
 }

 thismodule->cleanup = module_c_cleanup;

 thismodule->suspend = module_c_suspend;
 thismodule->resume = module_c_resume;

 event_listen (einit_core_update_configuration, module_c_einit_event_handler_update_configuration);

 return 0;
}
