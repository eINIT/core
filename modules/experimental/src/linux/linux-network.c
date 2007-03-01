/*
 *  linux-network.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/10/2006.
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

#define _MODULE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit-modules/network.h>
#include <dirent.h>
#include <sys/stat.h>

#if 0
#include <sys/ioctl.h>
#include <linux/sockios.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"linux-network", NULL};
const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Network Configuration (Linux-specific Parts)",
 .rid       = "linux-network-experimental",
 .si        = {
  .provides = provides,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

void find_network_interfaces_proc (struct network_control_block *cb) {
// cb->add_network_interface ("eth0", "Generic Network Interface", 0x00000001);
}

void find_network_interfaces_sys (struct network_control_block *cb) {
 DIR *dir;
 struct dirent *entry;

 dir = eopendir ("/sys/class/net");
 if (dir != NULL) {
  while (entry = ereaddir (dir)) {
   char tmp[BUFFERSIZE];
   if (entry->d_name[0] == '.') continue;

#if 0
   ioctl (0, SIOCGIFNAME, entry->d_name, &tmp);

   puts (tmp);

   cb->add_network_interface (entry->d_name, tmp, 0x00000001);
#else
   cb->add_network_interface (entry->d_name, "Generic Network Interface", 0x00000001);
#endif
  }
  eclosedir (dir);
 }
}

int configure (struct lmodule *irr) {
 function_register ("find-network-interfaces-proc", 1, (void *)find_network_interfaces_proc);
 function_register ("find-network-interfaces-sys", 1, (void *)find_network_interfaces_sys);

 return 0;
}

int cleanup (struct lmodule *irr) {
 function_unregister ("find-network-interfaces-proc", 1, (void *)find_network_interfaces_proc);
 function_unregister ("find-network-interfaces-sys", 1, (void *)find_network_interfaces_sys);

 return 0;
}
