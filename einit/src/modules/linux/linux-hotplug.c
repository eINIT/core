/*
 *  linux-hotplug.c
 *  einit
 *
 *  Created on 01/10/2007.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/types.h>
#include <linux/netlink.h>

#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_hotplug_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_hotplug_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "eINIT Hotplug Agent",
 .rid       = "linux-hotplug",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_hotplug_configure
};

module_register(linux_hotplug_self);

#endif

char linux_hotplug_enabled = 0;

int linux_hotplug_run() {
 if (!linux_hotplug_enabled) {
  linux_hotplug_enabled = 1;

  struct sockaddr_nl nls;
  struct pollfd pfd;
  char buf[512];

  // Open hotplug event netlink socket

  memset(&nls,0,sizeof(struct sockaddr_nl));
  nls.nl_family = AF_NETLINK;
  nls.nl_pid = getpid();
  nls.nl_groups = -1;

  pfd.events = POLLIN;
  pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (pfd.fd==-1)
   return 1;

  // Listen to netlink socket

  if (bind(pfd.fd, (void *)&nls, sizeof(struct sockaddr_nl)))
   return 1;
  while (-1!=poll(&pfd, 1, -1)) {
   int i, len = recv(pfd.fd, buf, sizeof(buf), MSG_DONTWAIT);
   if (len == -1) return 1;

   // Print the data to stdout.
   i = 0;
   while (i<len) {
    printf("%s\n", buf+i);
    i += strlen(buf+i)+1;
   }
  }
  return 0;
 }
 return status_ok;
}

int linux_hotplug_shutdown() {
 if (linux_hotplug_enabled) {
  linux_hotplug_enabled = 0;
 }
 return status_ok;
}

void linux_hotplug_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_scheduled) || (ev->type == einit_power_reset_scheduled)) {
  linux_hotplug_shutdown();
 }
}

void linux_hotplug_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   {
    pid_t p = fork();

    switch (p) {
     case 0:
      linux_hotplug_run();
      _exit (EXIT_SUCCESS);

     case -1:
      notice (3, "fork failed, cannot hotplug");
      break;

     default:
      sched_watch_pid(p);
      break;
    }
   }
   break;

  default: break;
 }
}

int linux_hotplug_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_event_subsystem_power, linux_hotplug_power_event_handler);
 event_ignore (einit_event_subsystem_boot, linux_hotplug_boot_event_handler);

 return 0;
}

int linux_hotplug_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 pa->cleanup = linux_hotplug_cleanup;

 event_listen (einit_event_subsystem_boot, linux_hotplug_boot_event_handler);
 event_listen (einit_event_subsystem_power, linux_hotplug_power_event_handler);

 return 0;
}
