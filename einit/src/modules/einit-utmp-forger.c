/*
 *  einit-utmp-forger.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/05/2006.
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

#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <utmp.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"utmp", NULL};
char * requires[] = {"mount/critical", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= 0,
	.options	= 0,
	.name		= "System-V compatibility: UTMP forger",
	.rid		= "einit-utmp-forger",
	.provides	= provides,
	.requires	= requires,
	.notwith	= NULL
};

int examine_configuration (struct lmodule *irr) {
 int pr = 0;

 return pr;
}

int enable (void *pa, struct einit_event *status) {
 FILE *ufile;
 struct cfgnode *utmp_cfg = cfg_findnode ("configuration-compatibility-sysv-forge-utmp", 0, NULL);

 if (utmp_cfg && utmp_cfg->flag) {
  ufile = fopen ("/var/run/utmp", "w");
  if (ufile) {
   struct utmp *utmpentries = ecalloc (2, sizeof (struct utmp));
//   int er = fread (utmpentries, sizeof (struct utmp), 30, ufile);
   int i;
//   for (i = 0; i < er; i++) {
//	puts (utmpentries[i].ut_line);
//   }
#ifdef LINUX
   utmpentries[0].ut_type = INIT_PROCESS;
   utmpentries[0].ut_pid = 1;
   utmpentries[1].ut_type = RUN_LVL;
#endif
//   utmpentries[1].ut_pid = ;
   fwrite (utmpentries, sizeof (struct utmp), 2, ufile);
   fclose (ufile);
   free (utmpentries);
  }
 }

/* always return OK, as utmp is pretty much useless to eINIT, so no reason
   to bitch about it... */
 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 return STATUS_OK;
}
