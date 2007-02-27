/*
 *  linux-sysconf.c
 *  einit
 *
 *  Created by Magnus Deininger on 27/03/2006.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <errno.h>
#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"sysconf", NULL};
char * requires[] = {"mount/system", NULL};
const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Linux-specific System-Configuration",
 .rid       = "linux-sysconf",
 .si        = {
  .provides = provides,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

void linux_reboot () {
 reboot (LINUX_REBOOT_CMD_RESTART);
 notice (1, "\naight, who hasn't eaten his cereals this morning?");
 exit (EXIT_FAILURE);
}

void linux_power_off () {
 reboot (LINUX_REBOOT_CMD_POWER_OFF);
 notice (1, "\naight, who hasn't eaten his cereals this morning?");
 exit (EXIT_FAILURE);
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-system-ctrl-alt-del", NULL)) {
   eputs (" * configuration variable \"configuration-system-ctrl-alt-del\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getstring ("configuration-system-sysctl-file", NULL)) {
   eputs (" * configuration variable \"configuration-system-sysctl-file\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int configure (struct lmodule *irr) {
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 function_register ("core-power-off-linux", 1, linux_power_off);
 function_register ("core-power-reset-linux", 1, linux_reboot);

 return 0;
}

int cleanup (struct lmodule *this) {
 function_unregister ("core-power-reset-linux", 1, linux_reboot);
 function_unregister ("core-power-off-linux", 1, linux_power_off);
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);

 return 0;
}

int enable (void *pa, struct einit_event *status) {
 struct cfgnode *cfg = cfg_getnode ("configuration-system-ctrl-alt-del", NULL);
 FILE *sfile;
 char *sfilename;

 if (cfg && !cfg->flag) {
  if (gmode != EINIT_GMODE_SANDBOX) {
   if (reboot (LINUX_REBOOT_CMD_CAD_OFF) == -1) {
    status->string = strerror(errno);
    errno = 0;
    status->flag++;
    status_update (status);
   }
  } else {
   if (1) {
    status->string = strerror(errno);
    errno = 0;
    status->flag++;
    status_update (status);
   }
  }
 }

 if ((sfilename = cfg_getstring ("configuration-system-sysctl-file", NULL))) {
  if ((sfile = fopen (sfilename, "r"))) {
   char buffer[2048], *cptr;
   while (fgets (buffer, 2048, sfile)) {
    switch (buffer[0]) {
     case ';':
     case '#':
     case 0:
      break;
     default:
      strtrim (buffer);

      if (buffer[0]) {
       if ((cptr = strchr(buffer, '='))) {
        ssize_t ci = 0;
        FILE *ofile;
        char tarbuffer[2048];

        strcpy (tarbuffer, "/proc/sys/");

        *cptr = 0;
        cptr++;

        strtrim (buffer);
        strtrim (cptr);

        for (; buffer[ci]; ci++) {
         if (buffer[ci] == '.') buffer[ci] = '/';
        }

        strncat (tarbuffer, buffer, 2047);

        if ((ofile = fopen(tarbuffer, "w"))) {
         eputs (cptr, ofile);
         fclose (ofile);
        } else {
         bitch2(BITCH_STDIO, "linux-sysconf:fopen(, w)", 0, tarbuffer);
        }
       }
      }

      break;
    }
   }

   fclose (sfile);
  } else {
   bitch2(BITCH_STDIO, "linux-sysconf:fopen(, r)", 0, sfilename);
  }
 }

 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 return STATUS_OK;
}
