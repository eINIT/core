/*
 *  einit-hostname.c
 *  einit
 *
 *  Created by Magnus Deininger on 05/09/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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

char * provides[] = {"hostname", "domainname", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= 0,
	.options	= 0,
	.name		= "Set Host- and Domainname",
	.rid		= "einit-hostname",
	.provides	= provides,
	.requires	= NULL,
	.notwith	= NULL
};

int examine_configuration (struct lmodule *irr) {
 int pr = 0;

 if (!cfg_getstring("hostname", NULL)) {
  fputs (" * configuration variable \"hostname\" not found.\n", stderr);
  pr++;
 }

 if (!cfg_getstring("domainname", NULL)) {
  fputs (" * configuration variable \"domainname\" not found.\n", stderr);
  pr++;
 }

 return pr;
}

int enable (void *pa, struct einit_event *status) {
 char *name;
 if (name = cfg_getstring ("hostname", NULL)) {
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

 if (name = cfg_getstring ("domainname", NULL)) {
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

 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 return STATUS_OK;
}
