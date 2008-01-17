/*
 *  compile-xml-sh.c
 *  einit
 *
 *  Created on 13/01/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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

int compile_xml_sh_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule compile_xml_sh_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT XML/sh-to-.so compiler",
 .rid       = "einit-compile-xml-sh",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = compile_xml_sh_configure
};

module_register(compile_xml_sh_self);

#endif

#define MODULES_PREFIX "services-virtual-module-"
#define MODULES_PREFIX_LENGTH (sizeof(MODULES_PREFIX) -1)

#define MODULES_EXECUTE_NODE_TEMPLATE MODULES_PREFIX "%s-execute"
#define MODULES_ARBITRARY_NODE_TEMPLATE MODULES_PREFIX "%s-%s"

char compile_xml_sh_firstrun = 1;

char compile_xml_sh_compile_file (char *oname, char *base, char *nname) {
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

char compile_xml_sh_update_modules () {
 char ret = 0;
 char *outputpath = cfg_getstring ("subsystem-xml-sh-compiler-compile-to", NULL);

 if (outputpath) {
  char do_compile = 0;

  if (compile_xml_sh_firstrun) {
   if (einit_argv) {
    uint32_t y = 0;

    for (; einit_argv[y]; y++) {
     if (strmatch (einit_argv[y], "--compile-xml-sh")) {
      unlink (outputpath);
      do_compile = 1;
     }
    }
   }

   compile_xml_sh_firstrun = 0;
  }

  if (do_compile) {
   struct stree *modules = cfg_prefix (MODULES_PREFIX);

   if (modules) {
    streefree (modules);
   }
  }
 }

 return ret;
}

void compile_xml_sh_einit_event_handler_update_configuration (struct einit_event *ev) {
 if (compile_xml_sh_update_modules()) {
  ev->chain_type = einit_core_configuration_update;
 }
}

int compile_xml_sh_cleanup (struct lmodule *this) {
 event_ignore (einit_core_update_configuration, compile_xml_sh_einit_event_handler_update_configuration);

 return 0;
}

int compile_xml_sh_configure (struct lmodule *irr) {
 module_init (irr);

 struct cfgnode *node = cfg_getnode ("subsystem-xml-sh-compiler-active", NULL);
 if (!node || !node->flag) { /* node needs to exist and explicitly say 'no' to disable this module */
  return status_configure_failed | status_not_in_use;
 }

 thismodule->cleanup = compile_xml_sh_cleanup;

 event_listen (einit_core_update_configuration, compile_xml_sh_einit_event_handler_update_configuration);

 return 0;
}
