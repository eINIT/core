/*
 *  compatibility-sysv-utmp.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/05/2006.
 *  renamed and moved from einit-utmp-forger.c on 2006/12/28
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <einit-modules/utmp.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int compatibility_sysv_utmp_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
char * compatibility_sysv_utmp_provides[] = {"utmp", NULL};
char * compatibility_sysv_utmp_requires[] = {"mount-critical", NULL};
const struct smodule module_compatibility_sysv_utmp_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "System-V Compatibility: {U|W}TMP",
 .rid       = "compatibility-sysv-utmp",
 .si        = {
  .provides = compatibility_sysv_utmp_provides,
  .requires = compatibility_sysv_utmp_requires,
  .after    = NULL,
  .before   = NULL
 },
 .configure = compatibility_sysv_utmp_configure
};

module_register(module_compatibility_sysv_utmp_self);

#endif

int  compatibility_sysv_utmp_enable  (void *, struct einit_event *);
int  compatibility_sysv_utmp_disable (void *, struct einit_event *);
char updateutmp_f (unsigned char, struct utmp *);

int compatibility_sysv_utmp_cleanup (struct lmodule *irr) {
// event_ignore (EVENT_SUBSYSTEM_IPC, compatibility_sysv_utmp_ipc_event_handler);
 function_unregister ("einit-utmp-update", 1, updateutmp_f);
 utmp_cleanup (irr);

 return 0;
}

char updateutmp_f (unsigned char options, struct utmp *new_entry) {
 int ufile;
 struct stat st;

// strip the UTMP_ADD action if we don't get a new entry to add along with it
 if ((options & UTMP_ADD) && !new_entry) options ^= UTMP_ADD;
// if we don't have anything to do, bail out
 if (!options) return -1;

 if (coremode == einit_mode_sandbox)
  ufile = eopen ("var/run/utmp", O_RDWR);
 else
  ufile = eopen ("/var/run/utmp", O_RDWR);
 if (ufile) {
  if (!fstat (ufile, &st) && st.st_size) {
   struct utmp *utmpentries = mmap (NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, ufile, 0);

   if (utmpentries != MAP_FAILED) {
    uint32_t entries = st.st_size / sizeof(struct utmp),
    i = 0;
    eclose (ufile);
    ufile = 0;

    for (i = 0; i < entries; i++) {
#ifdef LINUX
     switch (utmpentries[i].ut_type) {
      case DEAD_PROCESS:
       if (options & UTMP_ADD) {
        memcpy (&(utmpentries[i]), new_entry, sizeof (struct utmp));
        options ^= UTMP_ADD;
       }

       break;
      case RUN_LVL:
       if (options & UTMP_CLEAN) {
/* the higher 8 bits contain the old runlevel, the lower 8 bits the current one */
        char *new_previous_runlevel = cfg_getstring ("configuration-compatibility-sysv-simulate-runlevel/before", NULL),
            *new_runlevel = cfg_getstring ("configuration-compatibility-sysv-simulate-runlevel/now", NULL);

        if (new_runlevel && new_runlevel[0]) {
         if (new_previous_runlevel)
          utmpentries[i].ut_pid = (new_previous_runlevel[0] << 8) | new_runlevel[0];
         else
          utmpentries[i].ut_pid = (utmpentries[i].ut_pid << 8) | new_runlevel[0];
        }
       }
       break;

      case UT_UNKNOWN:
      case BOOT_TIME:
      case NEW_TIME:
      case OLD_TIME:
      case INIT_PROCESS:
      case LOGIN_PROCESS:
      case USER_PROCESS:
      case ACCOUNTING:
       if (options & UTMP_CLEAN) {
#ifdef LINUX
        struct stat xst;
        char path[BUFFERSIZE];
        esprintf (path, BUFFERSIZE, "/proc/%i/", utmpentries[i].ut_pid);
        if (stat (path, &xst)) { // stat path under proc to see if process exists
// if not...
#endif
// clean utmp record
         if (options & UTMP_ADD) {
          memcpy (&(utmpentries[i]), new_entry, sizeof (struct utmp));
          options ^= UTMP_ADD;
         } else {
          utmpentries[i].ut_type = DEAD_PROCESS;
          memset (&(utmpentries[i].ut_user), 0, sizeof (utmpentries[i].ut_user));
          memset (&(utmpentries[i].ut_host), 0, sizeof (utmpentries[i].ut_host));
          memset (&(utmpentries[i].ut_time), 0, sizeof (utmpentries[i].ut_time));
         }
#ifdef LINUX
        }
#endif
       }
       break;
#ifdef DEBUG
      default:
       notice (6, "bad UTMP entry: [%c%c%c%c] %i (%s), %s@%s: %i.%i\n", utmpentries[i].ut_id[0], utmpentries[i].ut_id[1], utmpentries[i].ut_id[2], utmpentries[i].ut_id[3], utmpentries[i].ut_type, utmpentries[i].ut_line, utmpentries[i].ut_user, utmpentries[i].ut_host, utmpentries[i].ut_tv.tv_sec, utmpentries[i].ut_tv.tv_usec);
       break;
#endif
     }

     if ((options & UTMP_MODIFY) && (utmpentries[i].ut_pid == new_entry->ut_pid)) {
      memcpy (&(utmpentries[i]), new_entry, sizeof (struct utmp));
      options ^= UTMP_MODIFY;
     }
#endif
     if (!options) break;
    }

    munmap (utmpentries, st.st_size);
   } else {
    bitch(bitch_stdio, 0, "mmap() failed");
   }
  }

  if (ufile)
   eclose (ufile);
 } else {
  bitch(bitch_stdio, 0, "open() failed");
 }

 if (options & UTMP_ADD) { // still didn't get to add this.. try to append it to the file
  if (coremode == einit_mode_sandbox)
   ufile = open ("var/run/utmp", O_WRONLY | O_APPEND);
  else
   ufile = open ("/var/run/utmp", O_WRONLY | O_APPEND);

  if (ufile) {
   if (write(ufile, new_entry, sizeof (struct utmp)) != sizeof (struct utmp)) {
    bitch(bitch_stdio, 0, "short write to utmp file");
   }
   eclose (ufile);

  } else {
   bitch(bitch_stdio, 0, "mmap() failed");
  }

  options ^= UTMP_ADD;
 }

 return 0;
}

int compatibility_sysv_utmp_enable (void *pa, struct einit_event *status) {
 char utmp_cfg = parse_boolean (cfg_getstring ("configuration-compatibility-sysv/utmp", NULL));

 if (utmp_cfg) {
  status->string = "cleaning utmp";
  status_update (status);
  updateutmp_f (UTMP_CLEAN, NULL);
 }

/* always return OK, as utmp is pretty much useless to eINIT, so no reason
   to bitch about it... */
 return STATUS_OK;
}

int compatibility_sysv_utmp_disable (void *pa, struct einit_event *status) {
 return STATUS_OK;
}

int compatibility_sysv_utmp_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->enable = compatibility_sysv_utmp_enable;
 thismodule->disable = compatibility_sysv_utmp_disable;
 thismodule->cleanup = compatibility_sysv_utmp_cleanup;

 utmp_configure (irr);
 function_register ("einit-utmp-update", 1, updateutmp_f);
// event_listen (EVENT_SUBSYSTEM_IPC, compatibility_sysv_utmp_ipc_event_handler);

 return 0;
}
