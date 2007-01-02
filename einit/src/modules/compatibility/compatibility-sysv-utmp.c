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

#define _MODULE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"utmp", NULL};
char * requires[] = {"mount/critical", NULL};
const struct smodule self = {
    .eiversion    = EINIT_VERSION,
    .version      = 1,
    .mode         = 0,
    .options      = 0,
    .name         = "System-V Compatibility: {U|W}TMP",
    .rid          = "compatibility-sysv-utmp",
    .si           = {
        .provides = provides,
        .requires = requires,
        .after    = NULL,
        .before   = NULL
    }
};

#define UTMP_CLEAN 0x01
#define UTMP_ADD   0x02

int  enable  (void *, struct einit_event *);
int  disable (void *, struct einit_event *);
char update_utmp (unsigned char, struct utmp *);

char update_utmp (unsigned char options, struct utmp *new_entry) {
 int ufile;
 struct stat st;
 if (gmode == EINIT_GMODE_SANDBOX)
  ufile = open ("var/run/utmp", O_RDWR);
 else
  ufile = open ("/var/run/utmp", O_RDWR);
 if (ufile) {
  if (!fstat (ufile, &st) && st.st_size) {
   struct utmp *utmpentries = mmap (NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, ufile, 0);

   if (utmpentries != MAP_FAILED) {
    uint32_t entries = st.st_size / sizeof(struct utmp),
    i = 0;
    close (ufile);
    ufile = 0;

#ifdef DEBUG
    fprintf (stderr, " >> checking %i utmp entries.\n", entries);
#endif
    for (; i < entries; i++) {
     switch (utmpentries[i].ut_type) {
#ifdef DEAD_PROCESS
      case DEAD_PROCESS:
       break;
#endif
#ifdef RUN_LVL
      case RUN_LVL:
       if (options & UTMP_CLEAN) {
/* the higher 8 bits contain the old runlevel, the lower 8 bits the current one */
#ifdef DEBUG
        char previous_runlevel = (utmpentries[i].ut_pid >> 8) ? (utmpentries[i].ut_pid >> 8) : 'N',
             current_runlevel = utmpentries[i].ut_pid & 0xff;
        printf(" >> setting runlevel; before: %c %c\n", previous_runlevel, current_runlevel);
#endif

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
#endif

#ifdef UT_UNKNOWN
      case UT_UNKNOWN:
#endif
#ifdef BOOT_TIME
      case BOOT_TIME:
#endif
#ifdef NEW_TIME
      case NEW_TIME:
#endif
#ifdef OLD_TIME
      case OLD_TIME:
#endif
#ifdef INIT_PROCESS
      case INIT_PROCESS:
#endif
#ifdef LOGIN_PROCESS
      case LOGIN_PROCESS:
#endif
#ifdef USER_PROCESS
      case USER_PROCESS:
#endif
#ifdef ACCOUNTING
      case ACCOUNTING:
       if (options & UTMP_CLEAN) {
#ifdef LINUX
        struct stat xst;
        char path[256];
        snprintf (path, 256, "/proc/%i/", utmpentries[i].ut_pid);
        if (stat (path, &xst)) { // stat path under proc to see if process exists
// if not...
#endif
// clean utmp record
         utmpentries[i].ut_type = DEAD_PROCESS;
         memset (&(utmpentries[i].ut_user), 0, sizeof (utmpentries[i].ut_user));
         memset (&(utmpentries[i].ut_host), 0, sizeof (utmpentries[i].ut_host));
         memset (&(utmpentries[i].ut_time), 0, sizeof (utmpentries[i].ut_time));
#ifdef LINUX
        }
#endif
       }
       break;
#endif
      default:
       fprintf (stderr, " >> bad UTMP entry: [%c%c%c%c] %i (%s), %s@%s: %i.%i\n", utmpentries[i].ut_id[0], utmpentries[i].ut_id[1], utmpentries[i].ut_id[2], utmpentries[i].ut_id[3], utmpentries[i].ut_type, utmpentries[i].ut_line, utmpentries[i].ut_user, utmpentries[i].ut_host, utmpentries[i].ut_tv.tv_sec, utmpentries[i].ut_tv.tv_usec);
       break;
     }

    }
    munmap (utmpentries, st.st_size);
   }
  }

  if (ufile)
   close (ufile);
 } else {
  perror (" >> utmp: mmap()");
 }

 return 0;
}

int enable (void *pa, struct einit_event *status) {
 char utmp_cfg = parse_boolean (cfg_getstring ("configuration-compatibility-sysv/utmp", NULL));

 if (utmp_cfg) {
  status->string = "cleaning utmp";
  status_update (status);
  update_utmp (UTMP_CLEAN, NULL);
 }

/* always return OK, as utmp is pretty much useless to eINIT, so no reason
   to bitch about it... */
 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 return STATUS_OK;
}
