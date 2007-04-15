/*
 *  hostname.c
 *  einit
 *
 *  Created by Magnus Deininger on 05/09/2006.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_hostname_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * einit_hostname_provides[] = {"hostname", "domainname", NULL};
char * einit_hostname_before[] = {"displaymanager", NULL};
const struct smodule einit_hostname_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "Set Host- and Domainname",
 .rid       = "hostname",
 .si        = {
  .provides = einit_hostname_provides,
  .requires = NULL,
  .after    = NULL,
  .before   = einit_hostname_before
 },
 .configure = einit_hostname_configure
};

module_register(einit_hostname_self);

#endif

void einit_hostname_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "examine") && strmatch(ev->argv[1], "configuration")) {
  char *s;

  if (!(s = cfg_getstring("configuration-network-hostname", NULL))) {
   eputs (" * configuration variable \"configuration-network-hostname\" not found.\n", ev->output);
   ev->ipc_return++;
  } else if (strmatch ("localhost", s)) {
   eputs (" * you should take your time to specify a hostname, go edit local.xml, look for the hostname-element.\n", ev->output);
   ev->ipc_return++;
  }
  if (!(s = cfg_getstring("configuration-network-domainname", NULL))) {
   eputs (" * configuration variable \"configuration-network-domainname\" not found.\n", ev->output);
   ev->ipc_return++;
  } else if (strmatch ("local", s)) {
   eputs (" * you should take your time to specify a domainname if you use NIS/YP services, go edit local.xml, look for the domainname-element.\n", ev->output);
  }

  ev->implemented = 1;
 }
}

int einit_hostname_cleanup (struct lmodule *this) {
 event_ignore (einit_event_subsystem_ipc, einit_hostname_ipc_event_handler);

 return 0;
}


int einit_hostname_enable (void *pa, struct einit_event *status) {
 char *name;
 if ((name = cfg_getstring ("configuration-network-hostname", NULL))) {
  status->string = "setting hostname";
  status_update (status);
  if (sethostname (name, strlen (name))) {
   status->string = strerror(errno);
   errno = 0;
   status->flag++;
   status_update (status);
  }
 } else {
  status->string = "no hostname configured";
  status->flag++;
  status_update (status);
 }

 if ((name = cfg_getstring ("configuration-network-domainname", NULL))) {
  status->string = "setting domainname";
  status_update (status);
  if (setdomainname (name, strlen (name))) {
   status->string = strerror(errno);
   errno = 0;
   status->flag++;
   status_update (status);
  }
 } else {
  status->string = "no domainname configured";
  status->flag++;
  status_update (status);
 }

 return status_ok;
}

int einit_hostname_disable (void *pa, struct einit_event *status) {
 return status_ok;
}

int einit_hostname_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->cleanup = einit_hostname_cleanup;
 thismodule->enable = einit_hostname_enable;
 thismodule->disable = einit_hostname_disable;

 event_listen (einit_event_subsystem_ipc, einit_hostname_ipc_event_handler);

 return 0;
}
