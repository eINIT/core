/*
 *  bsd-network.c
 *  einit
 *
 *  Created on 30/12/2007.
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
#include <einit/module.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <pthread.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int bsd_network_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule bsd_network_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Network Helpers (BSD)",
 .rid       = "bsd-network",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = bsd_network_configure
};

module_register(bsd_network_self);

#endif

char **bsd_network_interfaces = NULL;

pthread_mutex_t bsd_network_interfaces_mutex = PTHREAD_MUTEX_INITIALIZER;

char **bsd_network_list_interfaces_ifconfig (int spawn_events) {
 char **interfaces = NULL;
 char **new_interfaces = NULL;
 char buffer[BUFFERSIZE * 4];
 FILE *f = popen ("ifconfig -l", "r");

 if (f) {
  if (fgets (buffer, (BUFFERSIZE * 4), f)) {
   strtrim (buffer);
   if (buffer[0]) {
    interfaces = str2set (' ', buffer);
   }
  }
  pclose (f);
 }

 if (spawn_events) {
  if (interfaces) {
   emutex_lock (&bsd_network_interfaces_mutex);
   int i = 0;
   for (; interfaces[i]; i++) {
    if (!bsd_network_interfaces || !inset ((const void **)bsd_network_interfaces, interfaces[i], SET_TYPE_STRING))
     new_interfaces = (char **)setadd ((void **)new_interfaces, interfaces[i], SET_TYPE_STRING);
   }
   emutex_unlock (&bsd_network_interfaces_mutex);
  }

  if (new_interfaces) {
// spawn some events here about the new interfaces
   efree (new_interfaces);
  }
 }

 return interfaces;
}

int bsd_network_cleanup (struct lmodule *pa) {
 function_unregister ("network-list-interfaces-bsd", 1, (void *)bsd_network_list_interfaces_ifconfig);
 function_unregister ("network-list-interfaces-generic", 1, (void *)bsd_network_list_interfaces_ifconfig);

 return 0;
}

int bsd_network_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = bsd_network_cleanup;

 function_register ("network-list-interfaces-bsd", 1, (void *)bsd_network_list_interfaces_ifconfig);
 function_register ("network-list-interfaces-generic", 1, (void *)bsd_network_list_interfaces_ifconfig);

 return 0;
}
