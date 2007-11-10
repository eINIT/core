/*
 *  module-scheme-guile.c
 *  einit
 *
 *  Created on 10/11/2007.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>

#include <libguile.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_scheme_guile_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_scheme_guile_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (.scm, Guile)",
 .rid       = "einit-module-scheme-guile",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_scheme_guile_configure
};

module_register(module_scheme_guile_self);

#endif

int module_scheme_guile_cleanup (struct lmodule *pa) {
 return 0;
}

void module_scheme_guile_scanmodules_work_scheme (char **modules) {
 size_t i = 0;

 for (; modules[i]; i++) {
  scm_c_primitive_load (modules[i]);
 }

 return;
}

int module_scheme_guile_scanmodules ( struct lmodule *modchain ) {
 char **modules = NULL;
 struct cfgnode *node = NULL;

 while ((node = cfg_findnode ("subsystem-scheme-modules", 0, node))) {
  char **nmodules = readdirfilter(node, "/lib/einit/modules-scheme/", ".*\\.scm$", NULL, 0);

  if (nmodules) {
   modules = (char **)setcombine_nc ((void **)modules, (const void **)nmodules, SET_TYPE_STRING);

   free (nmodules);
  }
 }

 if (modules) {
//  struct lmodule *lm = modchain;

  scm_with_guile ((void *(*)(void *))module_scheme_guile_scanmodules_work_scheme, (void *)modules);

  free (modules);
 }

 return 1;
}

void module_scheme_guile_notice_wo (char *n) {
 notice (5, n);
}

SCM module_scheme_guile_notice (SCM message) {
 char *msg;

 scm_dynwind_begin (0);

 msg = scm_to_locale_string (message);
 scm_dynwind_unwind_handler (free, msg, SCM_F_WIND_EXPLICITLY);

 scm_without_guile ((void *(*)(void *))module_scheme_guile_notice_wo, msg);

 scm_dynwind_end ();

 return SCM_BOOL_T;
}

void module_scheme_guile_critical_wo (char *n) {
 notice (2, n);
}

SCM module_scheme_guile_critical (SCM message) {
 char *msg;

 scm_dynwind_begin (0);

 msg = scm_to_locale_string (message);
 scm_dynwind_unwind_handler (free, msg, SCM_F_WIND_EXPLICITLY);

 scm_without_guile ((void *(*)(void *))module_scheme_guile_critical_wo, msg);

 scm_dynwind_end ();

 return SCM_BOOL_T;
}

void module_scheme_guile_configure_scheme (void *n) {
 scm_c_define_gsubr ("notice", 1, 0, 0, module_scheme_guile_notice);
 scm_c_define_gsubr ("critical", 1, 0, 0, module_scheme_guile_critical);
}

int module_scheme_guile_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = module_scheme_guile_scanmodules;
 pa->cleanup = module_scheme_guile_cleanup;

 scm_with_guile ((void *(*)(void *))module_scheme_guile_configure_scheme, NULL);

 return 0;
}
