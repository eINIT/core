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

#include <einit-modules/exec.h>

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

struct einit_function_xml_data {
 int version;
 char *code;
 char *prototype;
};

char **einit_functions_xml_registered = NULL;

struct einit_function_xml_data einit_functions_xml_data_from_attrs (char **attrs) {
 struct einit_function_xml_data rv;
 memset (&rv, 0, sizeof (struct einit_function_xml_data));

 if (attrs) {
  int i = 0;

  for (; attrs[i]; i+=2) {
   if (strmatch (attrs[i], "version"))
    rv.version = parse_integer (attrs[i+1]);
   else if (strmatch (attrs[i], "prototype"))
    rv.prototype = attrs[i+1];
   else if (strmatch (attrs[i], "code"))
    rv.code = attrs[i+1];
  }
 }

 return rv;
}

struct einit_function_xml_data einit_functions_xml_data_get (char *id) {
 struct cfgnode *node = NULL;

 while ((node = cfg_findnode("special-function", 0, node))) {
  if (node->idattr && strmatch (node->idattr, id)) {
   return einit_functions_xml_data_from_attrs(node->arbattrs);
  }
 }

 return einit_functions_xml_data_from_attrs(NULL);
}

void * einit_functions_xml_generic_wrapper (char *name, ...) {
 struct einit_function_xml_data d = einit_functions_xml_data_get(name);

 if (d.version && d.prototype && d.code) {
  char **sprototype = str2set (':', d.prototype);
  int ai = 0;
  char *returntype = NULL;
  va_list rarg;
  char **call_environment = NULL;

  notice (1, "function called: %s; prototype = %s, code = %s", name, d.prototype, d.code);

  for (; sprototype[ai]; ai++) {
   if (!ai) {
    returntype = sprototype[ai];
   } else {
    if (ai == 1) {
     va_start (rarg, name);
    }

    char **argdefpair = str2set (' ', sprototype[ai]);
    char *argname = NULL;
    char argname_tmp[BUFFERSIZE];
    char *argvalue = NULL;
    char argvalue_tmp[BUFFERSIZE];

    if (argdefpair[1]) {
     argname = argdefpair[1];
     argdefpair[1] = NULL;
    } else {
     esprintf (argname_tmp, BUFFERSIZE, "arg%i", ai);
     argname = argname_tmp;
    }

    if (strmatch (argdefpair[0], "integer")) {
     esprintf (argvalue_tmp, BUFFERSIZE, "%i", va_arg (rarg, int));
     argvalue = argvalue_tmp;
    } else if (strmatch (argdefpair[0], "string")) {
     esprintf (argvalue_tmp, BUFFERSIZE, "%s", va_arg (rarg, char*));
     argvalue = argvalue_tmp;
    } else {
     argvalue = function_call_by_name_multi (char*, "einit-function-convert-argument", 1, (const char **)argdefpair, argname, va_arg (rarg, void*));

     if (!argvalue) argvalue = "(null)";
    }

    call_environment = straddtoenviron (call_environment, argname, argvalue);

    free (argdefpair);
   }
  }

  if (ai >= 1) {
   va_end (rarg);
  }

  pexec(d.code, NULL, 0, 0, NULL, NULL, call_environment, NULL);

  if (call_environment) {
   notice (1, "calling function with env=(%s)", set2str (' ', call_environment));
  }

  free (sprototype);
 } else {
  notice (1, "invalid function called: %s", name);
 }

 return 0;
}

void einit_functions_xml_update_functions () {
 struct cfgnode *node = NULL;

 while ((node = cfg_findnode("special-function", 0, node))) {
  if (node->idattr) {
   if (!inset ((const void **)einit_functions_xml_registered, node->idattr, SET_TYPE_STRING)) {
    struct einit_function_xml_data d = einit_functions_xml_data_from_attrs (node->arbattrs);
    notice (1, "registering function: %s", node->idattr);
    einit_functions_xml_registered = (char **)setadd ((void **)einit_functions_xml_registered, node->idattr, SET_TYPE_STRING);

    function_register_type (node->idattr, d.version, einit_functions_xml_generic_wrapper, function_type_generic);
   }
  }
 }

 function_call_by_name_multi (int, "function-test-generic", 1, (const char **)str2set (':', "3:2:1"), 11, "hello", 42);
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

 if (einit_functions_xml_registered) {
  int i = 0;

  for (; einit_functions_xml_registered[i]; i++) {
   struct einit_function_xml_data d = einit_functions_xml_data_get (einit_functions_xml_registered[i]);

   function_unregister_type (einit_functions_xml_registered[i], d.version, einit_functions_xml_generic_wrapper, function_type_generic);
  }
 }

 exec_cleanup (pa);

 return 0;
}

int einit_functions_xml_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 pa->cleanup = einit_functions_xml_cleanup;

 event_listen (einit_event_subsystem_core, einit_functions_xml_core_event_handler);

 return 0;
}
