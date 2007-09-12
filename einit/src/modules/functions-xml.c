/*
 *  functions-xml.c
 *  einit
 *
 *  Created by Magnus Deininger on 12/11/2007.
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

#include <stdarg.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_functions_xml_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_functions_xml_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Wrapper for Registered Functions from Configuration Data",
 .rid       = "functions-xml",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_functions_xml_configure
};

module_register(einit_functions_xml_self);

#endif

void * einit_functions_xml_generic_wrapper (char *name, ...) {
 notice (1, "function called: %s", name);

 return 0;
}

void einit_functions_xml_update_functions () {
 function_call_by_name_multi (int, "function-test-generic", 1, (const char **)str2set (':', "3:2:1"), 1, 2, 3);
 function_call_by_name (int, "function-test-generic-2", 1, 1, 2, 3);
}

void einit_functions_xml_core_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_core_configuration_update:
   einit_functions_xml_update_functions ();
   break;

  default:
   break;
 }
}

int einit_functions_xml_cleanup (struct lmodule *pa) {
 event_ignore (einit_event_subsystem_core, einit_functions_xml_core_event_handler);

 function_unregister ("function-test-generic-2", 1, einit_functions_xml_generic_wrapper);
 function_unregister ("function-test-generic-1", 1, einit_functions_xml_generic_wrapper);

 return 0;
}

int einit_functions_xml_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = einit_functions_xml_cleanup;

 event_listen (einit_event_subsystem_core, einit_functions_xml_core_event_handler);

 function_register ("function-test-generic-1", 1, einit_functions_xml_generic_wrapper);
 function_register ("function-test-generic-2", 1, einit_functions_xml_generic_wrapper);

 return 0;
}
