/*
 *  module-network-v2.c
 *  einit
 *
 *  Created on 12/09/2007.
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
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <sys/stat.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_module_network_v2_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_module_network_v2_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (Network, v2)",
 .rid       = "einit-module-network-v2",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_module_network_v2_configure
};

module_register(einit_module_network_v2_self);

#endif

#if defined(BSD)
char *bsd_network_suffixes[] = { "bsd", "generic", NULL };
#elif defined(LINUX)
char *bsd_network_suffixes[] = { "linux", "generic", NULL };
#else
char *bsd_network_suffixes[] = { "generic", NULL };
#endif

int einit_module_network_v2_scanmodules (struct lmodule *);

int einit_module_network_v2_cleanup (struct lmodule *pa) {
 return 0;
}

int einit_module_network_v2_scanmodules (struct lmodule *modchain) {
 char **interfaces = function_call_by_name_multi (char **, "network-list-interfaces", 1, (const char **)bsd_network_suffixes, 0);

 if (interfaces) {
  int i = 0;
  for (; interfaces[i]; i++) {
#if 0
   fprintf (stderr, "interface: %s\n", interfaces[i]);
   fflush (stderr);
#endif
  }
 }

 return 1;
}

int einit_module_network_v2_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = einit_module_network_v2_scanmodules;
 pa->cleanup = einit_module_network_v2_cleanup;

 return 0;
}
