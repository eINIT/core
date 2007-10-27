/*
 *  linux-bootchart.c
 *  einit
 *
 *  Created by Magnus Deininger on 26/10/2007.
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/acct.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_bootchart_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_bootchart_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Bootchart-style Data Collector (Linux)",
 .rid       = "linux-bootchart",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_bootchart_configure
};

module_register(linux_bootchart_self);

#endif

int linux_bootchart_in_switch = 0;
char linux_bootchart_have_thread = 0;
unsigned long linux_bootchart_sleep_time = 0;
char linux_bootchart_process_accounting = 0;

char *linux_bootchart_get_uptime () {
 char *tmp = readfile ("/proc/uptime");
 char *uptime = NULL;

 if (tmp) {
  char **t = str2set (' ', tmp);
  if (t) {
   if (t[0] && t[1]) {
    char **r = str2set ('.', t[0]);

    if (r) {
     if (r[0] && r[1]) {
      char buffer[30];
      esprintf (buffer, 30, "%s%s", r[0], r[1]);

      uptime = estrdup (buffer);
     }

     free (r);
    }
   }

   free (t);
  }

  free (tmp);
 }

 return uptime;
}

char *linux_bootchart_update_ds (char *ds, char *uptime) {
 char *t = readfile ("/proc/diskstats");
 if (t) {
  size_t len = strlen (uptime) + strlen (t) + 4 + (ds ? strlen (ds) : 0);
  char *tx = emalloc (len);

  if (ds) {
   esprintf (tx, len, "%s\n%s\n%s\n", ds, uptime, t);
   free (ds);
  } else {
   esprintf (tx, len, "%s\n%s\n", uptime, t);
  }

  free (t);

  ds = tx;
 }

 return ds;
}

char *linux_bootchart_update_ps (char *ps, char *uptime) {
 DIR *d;
 struct dirent *e;
 char **data = NULL;

 d = opendir ("/proc");
 if (d != NULL) {
  while ((e = readdir (d))) {
   char *t, *u, *da = NULL;
   if (strmatch (e->d_name, ".") || strmatch (e->d_name, "..")) {
    continue;
   }

   if ((t = joinpath ("/proc/", e->d_name))) {
    if ((u = joinpath (t, "stat"))) {
     struct stat st;
     if (!stat (u, &st)) {
      da = readfile (u);
     }

     free (u);
    }

/*    if ((u = joinpath (t, "cmdline"))) {
     struct stat st;
     if (!stat (u, &st)) {
      char *ru = readfile (u);

      if (strstr (ru, )) {
       linux_bootchart_have_thread = 0;
      }
     }

     free (u);
    }*/

    free (t);
   }

   if (da) {
    data = (char **)setadd ((void **)data, da, SET_NOALLOC);
   }
  }

  closedir(d);
 }

 if (data) {
  char *t = set2str ('\n', (const char **)data);
  int y = 0;

  if (t) {
   size_t len = strlen (uptime) + strlen (t) + 4 + (ps ? strlen (ps) : 0);
   char *tx = emalloc (len);

   if (ps) {
    esprintf (tx, len, "%s\n%s\n%s\n", ps, uptime, t);
    free (ps);
   } else {
    esprintf (tx, len, "%s\n%s\n", uptime, t);
   }

   free (t);

   ps = tx;
  }

  for (; data[y]; y++) {
   free (data[y]);
  }

  free (data);
 }

 return ps;
}

char *linux_bootchart_update_st (char *st, char *uptime) {
 char *t = readfile ("/proc/stat");
 if (t) {
  size_t len = strlen (uptime) + strlen (t) + 4 + (st ? strlen (st) : 0);
  char *tx = emalloc (len);

  if (st) {
   esprintf (tx, len, "%s\n%s\n%s\n", st, uptime, t);
   free (st);
  } else {
   esprintf (tx, len, "%s\n%s\n", uptime, t);
  }

  free (t);

  st = tx;
 }

 return st;
}

void *linux_bootchart_thread (void *ignored) {
 struct cfgnode *node;
 char *save_to = "/var/log/bootchart.tgz";
 FILE *f;
 char try_acct = 1;
 signed int extra_wait = 0;

 if ((node = cfg_getnode ("configuration-bootchart-extra-waiting-time", NULL)) && node->value) {
  extra_wait = node->value;
 }

 char *buffer_ds = NULL;
 char *buffer_ps = NULL;
 char *buffer_st = NULL;

 while (linux_bootchart_have_thread || extra_wait) {
  char *uptime = linux_bootchart_get_uptime();

  if (linux_bootchart_process_accounting && try_acct) {
   if (acct ("/dev/kernel_pacct") == -1)
    try_acct = 1;
  }

  if (uptime) {
   buffer_ds = linux_bootchart_update_ds (buffer_ds, uptime);
   buffer_ps = linux_bootchart_update_ps (buffer_ps, uptime);
   buffer_st = linux_bootchart_update_st (buffer_st, uptime);

   free (uptime);
  }

  usleep (linux_bootchart_sleep_time);
  if (!linux_bootchart_have_thread)
   extra_wait -= linux_bootchart_sleep_time;
 }

 if ((node = cfg_getnode ("configuration-bootchart-save-to", NULL)) && node->svalue) {
  save_to = node->svalue;
 }

 if (coremode & einit_mode_sandbox) {
  save_to = "bootchart.tgz";
 }

 mkdir ("/tmp/bootchart.einit", 0755);

 if (buffer_ds) {
  if ((f = fopen ("/tmp/bootchart.einit/proc_diskstats.log", "w"))) {
   fputs (buffer_ds, f);

   fclose (f);
  }

  free (buffer_ds);
 }

 if (buffer_ps) {
  if ((f = fopen ("/tmp/bootchart.einit/proc_ps.log", "w"))) {
   fputs (buffer_ps, f);

   fclose (f);
  }

  free (buffer_ps);
 }

 if (buffer_st) {
  if ((f = fopen ("/tmp/bootchart.einit/proc_stat.log", "w"))) {
   fputs (buffer_st, f);

   fclose (f);
  }

  free (buffer_st);
 }

 if (linux_bootchart_process_accounting) {
  char *r = readfile ("/dev/kernel_pacct");
  if (r) {
   if ((f = fopen ("/tmp/bootchart.einit/kernel_pacct", "w"))) {
    fputs (r, f);

    fclose (f);
   }

   unlink ("/dev/kernel_pacct");
  }

  acct(NULL);
 }

 if ((f = fopen ("/tmp/bootchart.einit/header", "w"))) {
  char *t, buffer[BUFFERSIZE];
  time_t ti = time(NULL);
/* we're emulating bootchartd-0.8/0.9's format... */
  eputs ("version = 0.8\n", f);

  if (gethostname (buffer, BUFFERSIZE) == 0) {
   eprintf (f, "title = eINIT Boot Chart for %s, %s", buffer, ctime(&ti));
  } else {
   eprintf (f, "title = eINIT Boot Chart, %s", ctime(&ti));
  }

  fprintf (f, "system.uname = %s %s %s %s\n", osinfo.sysname, osinfo.release, osinfo.version, osinfo.machine);

  if ((t = readfile ("/etc/gentoo-release"))) {
   strtrim (t);
   eprintf (f, "system.release = %s\n", t);
   free (t);
  } else {
   eputs ("system.release = unknown\n", f);
  }

  if ((t = readfile ("/proc/cpuinfo"))) {
   char **r = str2set ('\n', t);
   char *n = NULL;
   int i;

   if (r) {
    for (i = 0; r[i]; i++) {
     if (strstr (r[i], "model name") == r[i]) {
      n = r[i];
      break;
     }
    }

    if (n)
     eprintf (f, "system.cpu = %s\n", n);
    else
     eputs ("system.cpu = unknown\n", f);
   }

   free (t);
  } else {
   eputs ("system.cpu = unknown\n", f);
  }

  if ((t = readfile ("/proc/cmdline"))) {
   eprintf (f, "system.kernel.options = %s\n", t);
   free (t);
  }

  fclose (f);
 }

 char buffer[BUFFERSIZE];
 if (coremode & einit_mode_sandbox) {
  esprintf (buffer, BUFFERSIZE, "export pwx=`pwd`; cd /tmp/bootchart.einit; tar czf \"${pwx}/%s\" *", save_to);
 } else {
  esprintf (buffer, BUFFERSIZE, "cd /tmp/bootchart.einit; tar czf %s *", save_to);
 }
 system (buffer);

 unlink_recursive ("/tmp/bootchart.einit/", 1);

 char *di = cfg_getstring ("configuration-bootchart-chart-directory", NULL);
 char *fo = cfg_getstring ("configuration-bootchart-chart-format", NULL);
 esprintf (buffer, BUFFERSIZE, "bootchart -o %s -f %s %s", di, fo, save_to);

 return NULL;
}

void linux_bootchart_switch () {
 struct cfgnode *node = cfg_getnode ("configuration-bootchart-active", NULL);

 if (node && node->flag) {
  if ((node = cfg_getnode ("configuration-bootchart-polling-interval", NULL)) && node->value) {
   linux_bootchart_sleep_time = node->value;
  } else {
   linux_bootchart_sleep_time = 200;
  }

  if ((node = cfg_getnode ("configuration-bootchart-process-accounting", NULL)) && node->flag) {
   linux_bootchart_process_accounting = 1;
  }

  if (!linux_bootchart_have_thread) {
   pthread_t th;
   linux_bootchart_have_thread = 1;

   ethread_create (&th, &thread_attribute_detached, linux_bootchart_thread, NULL);
  }
 }

 linux_bootchart_in_switch++;
}

void linux_bootchart_switch_done () {
 linux_bootchart_in_switch--;

 if (!linux_bootchart_in_switch && linux_bootchart_have_thread) {
  linux_bootchart_have_thread = 0;
 }
}

void linux_bootchart_core_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_core_mode_switching:
   linux_bootchart_switch ();
   break;

  case einit_core_mode_switch_done:
   linux_bootchart_switch_done ();
   break;

  default: break;
 }
}

void linux_bootchart_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   linux_bootchart_in_switch--;
   linux_bootchart_switch ();
   break;

  default: break;
 }
}

int linux_bootchart_cleanup (struct lmodule *pa) {
 event_ignore (einit_event_subsystem_core, linux_bootchart_core_event_handler);
 event_ignore (einit_event_subsystem_boot, linux_bootchart_boot_event_handler);

 return 0;
}

int linux_bootchart_configure (struct lmodule *tm) {
 module_init (tm);

 event_listen (einit_event_subsystem_boot, linux_bootchart_boot_event_handler);
 event_listen (einit_event_subsystem_core, linux_bootchart_core_event_handler);

 return 0;
}
