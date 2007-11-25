/*
 *  linux-cpufreq.c
 *  einit
 *
 *  Created by Magnus Deininger on 22/10/2007.
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

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_cpufreq_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_cpufreq_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "CPU Frequency Manager (Linux)",
 .rid       = "linux-cpufreq",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_cpufreq_configure
};

module_register(linux_cpufreq_self);

#endif

int linux_cpufreq_in_switch = 0;



void linux_cpufreq_set_governor_data (char *gd, int cpus) {
 if (gd) {
  char tmp[BUFFERSIZE];
  int i = 0;

  for (; i < cpus; i++) {
   FILE *f;
   notice (4, "setting cpufreq data: %s (cpu %i)", gd, i);

   esprintf (tmp, BUFFERSIZE, "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_governor", i);

   if ((f = fopen (tmp, "w"))) {
    fputs (gd, f);
    fputs ("\n", f);

    fclose (f);
   }
  }
 }
}

void linux_cpufreq_switch () {
 struct cfgnode *node = cfg_getnode ("configuration-linux-cpufreq", NULL);

 if (node && node->arbattrs) {
  char *governor_data = NULL;
  int cpus = 32;
  int i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "cpus")) {
    cpus = parse_integer (node->arbattrs[i+1]);
   } else if (strmatch (node->arbattrs[i], "in-switch")) {
    governor_data = node->arbattrs[i+1];
   }
  }

  if (governor_data) {
   linux_cpufreq_set_governor_data (governor_data, cpus);
  }
 }

 linux_cpufreq_in_switch++;
}

void linux_cpufreq_switch_done () {
 linux_cpufreq_in_switch--;

 if (!linux_cpufreq_in_switch) {
  struct cfgnode *node = cfg_getnode ("configuration-linux-cpufreq", NULL);

  if (node && node->arbattrs) {
   char *governor_data = NULL;
   int cpus = 32;
   int i = 0;

   for (; node->arbattrs[i]; i+=2) {
    if (strmatch (node->arbattrs[i], "cpus")) {
     cpus = parse_integer (node->arbattrs[i+1]);
    } else if (strmatch (node->arbattrs[i], "post-switch")) {
     governor_data = node->arbattrs[i+1];
    }
   }

   if (governor_data) {
    linux_cpufreq_set_governor_data (governor_data, cpus);
   }
  }
 }
}

int linux_cpufreq_cleanup (struct lmodule *pa) {
 event_ignore (einit_core_mode_switching, linux_cpufreq_switch);
 event_ignore (einit_core_mode_switch_done, linux_cpufreq_switch_done);

 return 0;
}

int linux_cpufreq_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_cpufreq_cleanup;

 event_listen (einit_core_mode_switching, linux_cpufreq_switch);
 event_listen (einit_core_mode_switch_done, linux_cpufreq_switch_done);

 return 0;
}
