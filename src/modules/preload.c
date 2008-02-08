/*
 *  fqdn.c
 *  einit
 *
 *  Created by Magnus Deininger on 08/10/2007.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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

#include <einit-modules/scheduler.h>

#include <dlfcn.h>
#include <sys/wait.h>

#if defined(LINUX)
#include <sys/prctl.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_preload_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_preload_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT preload",
 .rid       = "einit-preload",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_preload_configure,
};

module_register(einit_preload_self);

#endif

void einit_preload_boot_event_handler_early (struct einit_event *);

#if 0
int einit_preload_cleanup (struct lmodule *this) {
 sched_cleanup(irr);

 if ((coremode & einit_mode_sandbox)) return 0;

 event_ignore (einit_boot_early, einit_preload_boot_event_handler_early);

 return 0;
}
#endif

void einit_preload_run () {
 char *binaries = NULL;
 if ((binaries = cfg_getstring ("configuration-system-preload/binaries", NULL))) {
  char **files = str2set (':', binaries);

  if (files) {
   int i = 0;
   void *dh;

   notice (4, "pre-loading binaries: %s.", binaries);

   for (; files[i]; i++) {
    if (files[i][0] == '/') {
     if ((dh = dlopen (files[i], RTLD_NOW))) {
      dlclose (dh);
     }
    } else {
     char **w = which (files[i]);
     if (w) {
      int n = 0;

      for (; w[n]; n++) {
       if ((dh = dlopen (w[n], RTLD_NOW))) {
        dlclose (dh);
       }
      }

      efree (w);
     }
    }
   }

   efree (files);
  }
 }
}

void einit_preload_boot_event_handler_early (struct einit_event *ev) {
 struct cfgnode *node = cfg_getnode ("configuration-system-preload", NULL);

 if (node && node->flag) {
  pid_t p = fork();
  pid_t q = 0;

  switch (p) {
   case 0:
    q = fork();
    if (q == 0) {
#if defined(LINUX) && defined(PR_SET_NAME)
     prctl (PR_SET_NAME, "einit [preload-static]", 0, 0, 0);
#endif
     disable_core_dumps();

     einit_preload_run();
     _exit (EXIT_SUCCESS);
    }

    disable_core_dumps();
    _exit (EXIT_SUCCESS);

   case -1:
    notice (3, "fork failed, cannot preload");
    break;

   default:
    waitpid (p, NULL, 0);
//    sched_watch_pid(p);
    break;
  }
 }
}

int einit_preload_configure (struct lmodule *irr) {
 module_init (irr);
 sched_configure(irr);

 if (!(coremode & (einit_mode_sandbox | einit_mode_ipconly))) {
  einit_preload_boot_event_handler_early(NULL);
 }
 return status_configure_done | status_block;

 return 0;
}
