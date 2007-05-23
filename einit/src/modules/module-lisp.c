/*
 *  module-lisp.c
 *  einit
 *
 *  created on 15/05/2007.
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
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>
#include <einit/configuration.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_lisp_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_lisp_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (.lisp)",
 .rid       = "module-lisp",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_lisp_configure
};

module_register(module_lisp_self);

#endif

enum lisp_node_type {
 lnt_cons,
 lnt_symbol,
 lnt_constant
};

struct lisp_node {
 lisp_node_type type;

 union {
  struct { /* cons */
   lisp_node *primus;
   lisp_node *secundus;
  };

  char *symbol;
  
  char *constant_string;
  double constant_float;
  int constant_int;
 };
};

pthread_mutex_t modules_update_mutex = PTHREAD_MUTEX_INITIALIZER;

int module_lisp_scanmodules (struct lmodule *);

int module_lisp_cleanup (struct lmodule *pa) {
 return 0;
}

int module_lisp_scanmodules ( struct lmodule *modchain ) {
 void *sohandle;
 char **modules = NULL;

 modules = readdirfilter(cfg_getnode ("subsystem-lisp-import", NULL), 
                         "/lib/einit/modules-lisp/", "(\.e?lisp)$", NULL, 0);

 if (modules) {
 }

 return 1;
}

int module_lisp_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = module_lisp_scanmodules;
 pa->cleanup = module_lisp_cleanup;

 return 0;
}
