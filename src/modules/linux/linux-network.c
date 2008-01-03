/*
 *  linux-network.c
 *  einit
 *
 *  Created on 03/01/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
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
#include <unistd.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <pthread.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_network_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_network_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Network Helpers (Linux)",
 .rid       = "linux-network",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_network_configure
};

module_register(linux_network_self);

#endif

char **linux_network_interfaces = NULL;

pthread_mutex_t linux_network_interfaces_mutex = PTHREAD_MUTEX_INITIALIZER;

char **linux_network_list_interfaces_proc (int spawn_events) {
 char **interfaces = NULL;
 char **new_interfaces = NULL;
 char *buffer = readfile ("/proc/net/dev");

 if (buffer) {
  char **buffer_lines = str2set ('\n', buffer);
  efree (buffer);

  int i = 0;
  for (; buffer_lines[i]; i++) {
   strtrim (buffer_lines[i]);
   char **line_split = str2set(':', buffer_lines[i]);
   if (line_split[1]) { /* have at least two elements: it's one of the interface stat lines */
    interfaces = (char **)setadd((void **)interfaces, line_split[0], SET_TYPE_STRING);
   }
   efree (line_split);
  }
  efree (buffer_lines);
 }

 if (spawn_events) {
  if (interfaces) {
   emutex_lock (&linux_network_interfaces_mutex);
   int i = 0;
   for (; interfaces[i]; i++) {
    if (!linux_network_interfaces || !inset ((const void **)linux_network_interfaces, interfaces[i], SET_TYPE_STRING))
     new_interfaces = (char **)setadd ((void **)new_interfaces, interfaces[i], SET_TYPE_STRING);
   }
   emutex_unlock (&linux_network_interfaces_mutex);
  }

  if (new_interfaces) {
// spawn some events here about the new interfaces
   efree (new_interfaces);
  }
 }

 return interfaces;
}

int linux_network_cleanup (struct lmodule *pa) {
 function_unregister ("network-list-interfaces-linux", 1, (void *)linux_network_list_interfaces_proc);
 function_unregister ("network-list-interfaces-generic", 1, (void *)linux_network_list_interfaces_proc);

 return 0;
}

int linux_network_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_network_cleanup;

 function_register ("network-list-interfaces-linux", 1, (void *)linux_network_list_interfaces_proc);
 function_register ("network-list-interfaces-generic", 1, (void *)linux_network_list_interfaces_proc);

 return 0;
}
