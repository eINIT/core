/*
 *  einit-bootchartd.c
 *  einit
 *
 *  Created by Magnus Deininger on 30/03/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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

#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#include <einit/einit.h>
#include <syslog.h>

#define PIDFILE "/dev/einit-bootchartd.pid"

char **bootchartd_argv = NULL;
int bootchartd_argc = 0;

char bootchartd_active = 1;
stack_t signalstack;

void signal_sigterm (int signal, siginfo_t *siginfo, void *context) {
 /* nothing to do here... really */

 bootchartd_active = 0;

 return;
}

void connect_or_terminate () {
 if (!einit_connect (&bootchartd_argc, bootchartd_argv)) {
  fprintf (stderr, "could not connect to einit.\n");
  _exit (EXIT_FAILURE);
 }
}

char *bootchartd_get_uptime () {
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
      snprintf (buffer, 30, "%s%s", r[0], r[1]);

      uptime = (char *)str_stabilise (buffer);
     }

     efree (r);
    }
   }

   efree (t);
  }

  efree (tmp);
 }

 return uptime;
}

char *bootchartd_update_ds (char *ds, char *uptime) {
 char *t = readfile ("/proc/diskstats");
 if (t) {
  size_t len = strlen (uptime) + strlen (t) + 4 + (ds ? strlen (ds) : 0);
  char *tx = emalloc (len);

  if (ds) {
   snprintf (tx, len, "%s\n%s\n%s\n", ds, uptime, t);
   efree (ds);
  } else {
   snprintf (tx, len, "%s\n%s\n", uptime, t);
  }

  efree (t);

  ds = tx;
 }

 return ds;
}

char *bootchartd_update_ps (char *ps, char *uptime) {
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

     efree (u);
    }

    efree (t);
   }

   if (da) {
    data = set_str_add (data, da);
    efree (da);
    da = NULL;
   }
  }

  closedir(d);
 }

 if (data) {
  char *t = set2str ('\n', (const char **)data);

  if (t) {
   size_t len = strlen (uptime) + strlen (t) + 4 + (ps ? strlen (ps) : 0);
   char *tx = emalloc (len);

   if (ps) {
    snprintf (tx, len, "%s\n%s\n%s\n", ps, uptime, t);
    efree (ps);
   } else {
    snprintf (tx, len, "%s\n%s\n", uptime, t);
   }

   efree (t);

   ps = tx;
  }

  efree (data);
 }

 return ps;
}

char *bootchartd_update_st (char *st, char *uptime) {
 char *t = readfile ("/proc/stat");
 if (t) {
  size_t len = strlen (uptime) + strlen (t) + 4 + (st ? strlen (st) : 0);
  char *tx = emalloc (len);

  if (st) {
   snprintf (tx, len, "%s\n%s\n%s\n", st, uptime, t);
   efree (st);
  } else {
   snprintf (tx, len, "%s\n%s\n", uptime, t);
  }

  efree (t);

  st = tx;
 }

 return st;
}

int einit_bootchart() {
 unsigned long sleep_time = einit_get_configuration_integer ("configuration-bootchart-polling-interval", NULL);
 char *save_to = einit_get_configuration_string ("configuration-bootchart-save-to", NULL);
 size_t max_log_size = einit_get_configuration_integer ("configuration-bootchart-max-log-size", NULL);
 signed int extra_wait = einit_get_configuration_integer ("configuration-bootchart-extra-waiting-time", NULL);
 FILE *f;

 if (!sleep_time) sleep_time = 200000;
 if (!save_to) save_to = "/var/log/bootchart.tgz";
 if (!max_log_size) max_log_size = 1024*1024;

 char *buffer_ds = NULL;
 char *buffer_ps = NULL;
 char *buffer_st = NULL;

 while (bootchartd_active || (extra_wait > 0)) {
  size_t log_size = 0;
  char *uptime = bootchartd_get_uptime();

  if (uptime) {
   buffer_ds = bootchartd_update_ds (buffer_ds, uptime);
   buffer_ps = bootchartd_update_ps (buffer_ps, uptime);
   buffer_st = bootchartd_update_st (buffer_st, uptime);

   uptime = NULL;
  }

  usleep (sleep_time);
  if (!bootchartd_active)
   extra_wait -= sleep_time;

  if (buffer_ds) log_size += strlen (buffer_ds);
  if (buffer_ps) log_size += strlen (buffer_ps);
  if (buffer_st) log_size += strlen (buffer_st);

  if (log_size > max_log_size) {
   fprintf (stderr, "boot log exceeded maximum log size, stopping log.");
   break;
  }
 }

 fprintf (stderr, "generating bootchart data.");

 mkdir ("/tmp/bootchart.einit", 0755);

 if (buffer_ds) {
  if ((f = fopen ("/tmp/bootchart.einit/proc_diskstats.log", "w"))) {
   fputs (buffer_ds, f);

   fclose (f);
  }

  efree (buffer_ds);
  buffer_ds = NULL;
 }

 if (buffer_ps) {
  if ((f = fopen ("/tmp/bootchart.einit/proc_ps.log", "w"))) {
   fputs (buffer_ps, f);

   fclose (f);
  }

  efree (buffer_ps);
  buffer_ps = NULL;
 }

 if (buffer_st) {
  if ((f = fopen ("/tmp/bootchart.einit/proc_stat.log", "w"))) {
   fputs (buffer_st, f);

   fclose (f);
  }

  efree (buffer_st);
  buffer_st = NULL;
 }

 if ((f = fopen ("/tmp/bootchart.einit/header", "w"))) {
  char *t, buffer[BUFFERSIZE];
  time_t ti = time(NULL);
  /* we're emulating bootchartd-0.8/0.9's format... */
  fputs ("version = 0.8\n", f);

  if (gethostname (buffer, BUFFERSIZE) == 0) {
   fprintf (f, "title = eINIT Boot Chart for %s, %s", buffer, ctime(&ti));
  } else {
   fprintf (f, "title = eINIT Boot Chart, %s", ctime(&ti));
  }

  fprintf (f, "system.uname = %s %s %s %s\n", osinfo.sysname, osinfo.release, osinfo.version, osinfo.machine);

  if ((t = readfile ("/etc/gentoo-release"))) {
   strtrim (t);
   fprintf (f, "system.release = %s\n", t);
   efree (t);
  } else {
   fputs ("system.release = unknown\n", f);
  }

  if ((t = readfile ("/proc/cpuinfo"))) {
   char **r = str2set ('\n', t);
   char *n = NULL;
   int i;

   if (r) {
    for (i = 0; r[i]; i++) {
     if (strprefix (r[i], "model name")) {
      n = r[i];
      break;
     }
    }

    if (n)
     fprintf (f, "system.cpu = %s\n", n);
    else
     fputs ("system.cpu = unknown\n", f);
   }

   efree (t);
  } else {
   fputs ("system.cpu = unknown\n", f);
  }

  if ((t = readfile ("/proc/cmdline"))) {
   fprintf (f, "system.kernel.options = %s\n", t);
   efree (t);
  }

  fclose (f);
 }

 char buffer[BUFFERSIZE];
 if (coremode & einit_mode_sandbox) {
  snprintf (buffer, BUFFERSIZE, "export pwx=`pwd`; cd /tmp/bootchart.einit; tar czf \"${pwx}/%s\" *", save_to);
 } else {
  snprintf (buffer, BUFFERSIZE, "cd /tmp/bootchart.einit; tar czf %s *", save_to);
 }
 system (buffer);

 unlink_recursive ("/tmp/bootchart.einit/", 1);

 char *di = einit_get_configuration_string ("configuration-bootchart-chart-directory", NULL);
 char *fo = einit_get_configuration_string ("configuration-bootchart-chart-format", NULL);

 if (!di) di = "/var/log";
 if (!fo) fo = "png";
 snprintf (buffer, BUFFERSIZE, "bootchart -o %s -f %s %s", di, fo, save_to);
 fputs (buffer, stderr);

 system(buffer);

 return EXIT_SUCCESS;
}

int main (int argc, char **argv) {
 FILE *pidfile;

 bootchartd_argv = argv;
 bootchartd_argc = argc;

 struct sigaction action;

 signalstack.ss_sp = emalloc (SIGSTKSZ);
 signalstack.ss_size = SIGSTKSZ;
 signalstack.ss_flags = 0;
 sigaltstack (&signalstack, NULL);

 sigemptyset(&(action.sa_mask));

 action.sa_sigaction = signal_sigterm;
 action.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
 if ( sigaction (SIGTERM, &action, NULL) ) {
  perror ("could not set SIGTERM handler");
 }

 if (daemon(0, 0)) {
  perror ("could not daemonise");
  return EXIT_FAILURE;
 }

 pidfile = fopen (PIDFILE, "w");
 if (pidfile) {
  fprintf (pidfile, "%d\n", getpid());
  fclose (pidfile);
 }

 connect_or_terminate();

 return einit_bootchart();
}
