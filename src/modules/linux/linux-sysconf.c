/*
 *  linux-sysconf.c
 *  einit
 *
 *  Created by Magnus Deininger on 27/03/2006.
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
#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <linux/vt.h>

#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_sysconf_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * linux_sysconf_provides[] = {"sysv-cleanups", NULL};
char * linux_sysconf_before[] = {"^displaymanager$", NULL};
char * linux_sysconf_after[] = {"^fs-(root|var|var-run|var-log)$", NULL};

const struct smodule module_linux_sysconf_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module | einit_feedback_job,
 .name      = "Linux System Configuration and SysV-specific cleanups",
 .rid       = "linux-sysconf",
 .si        = {
  .provides = linux_sysconf_provides,
  .requires = NULL,
  .after    = linux_sysconf_after,
  .before   = linux_sysconf_before
 },
 .configure = linux_sysconf_configure
};

module_register(module_linux_sysconf_self);

#endif

char linux_sysconf_block_chvt = 0;

void linux_reboot () {
 _exit (einit_exit_status_last_rites_reboot);
}

void linux_power_off () {
 _exit (einit_exit_status_last_rites_halt);
}

void linux_sysconf_ctrl_alt_del () {
 if (reboot (LINUX_REBOOT_CMD_CAD_OFF) == -1)
  notice (1, "Couldn't change the CTRL+ALT+DEL handler: %s", strerror (errno));
}

void linux_sysconf_hwclock() {
 char *options = cfg_getstring ("configuration-services-hwclock/options", NULL);
 if (!options) options = "--utc";
 char tmp [BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "/sbin/hwclock --hctosys %s", options);

 qexec (tmp);
}

void linux_sysconf_shutdown_hwclock() {
 char *options = cfg_getstring ("configuration-services-hwclock/options", NULL);
 if (!options) options = "--utc";
 char tmp [BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "/sbin/hwclock --systohc %s", options);

 qexec (tmp);
}

void linux_sysconf_shutdown(struct einit_event *ev) {
 linux_sysconf_shutdown_hwclock();
}

void linux_sysconf_make_timezone_symlink (void) {
 char *zoneinfo = cfg_getstring ("configuration-system-timezone", NULL);
 if (zoneinfo) {
  char tmp [BUFFERSIZE];
  esprintf (tmp, BUFFERSIZE, "/usr/share/zoneinfo/%s", zoneinfo);
  remove ("/etc/localtime");
  symlink (tmp, "/etc/localtime");
 }
}

void linux_sysconf_root_ok (struct einit_event *ev) {
 linux_sysconf_make_timezone_symlink();
}

void linux_sysconf_service_enabled(struct einit_event *ev) {
 if (ev->string && (strmatch (ev->string, "einit-psplash") || strmatch (ev->string, "einit-usplash") || strmatch (ev->string, "einit-exquisite") || strmatch (ev->string, "einit-fbsplash"))) {
  linux_sysconf_block_chvt = 1;
 }
}

void linux_sysconf_fix_ttys() {
 struct cfgnode *filenode = cfg_getnode ("configuration-feedback-visual-std-io", NULL);

 if (filenode && filenode->arbattrs) {
  uint32_t i = 0;
  FILE *tmp;
  struct stat st;

  for (; filenode->arbattrs[i]; i+=2) {
   errno = 0;

   if (filenode->arbattrs[i]) {
    if (strmatch (filenode->arbattrs[i], "stdio")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      tmp = freopen (filenode->arbattrs[i+1], "r", stdin);
      if (!tmp)
       freopen ("/dev/null", "r+", stdin);

      tmp = freopen (filenode->arbattrs[i+1], "w", stdout);
      if (!tmp)
       tmp = freopen ("einit-panic-stdout", "w", stdout);
     } else {
      perror ("einit-feedback-visual-textual: opening stdio");
     }
    } else if (strmatch (filenode->arbattrs[i], "stderr")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      tmp = freopen (filenode->arbattrs[i+1], "a", stderr);
      if (!tmp)
       tmp = freopen ("einit-panic-stdout", "a", stderr);
      if (tmp)
       eprintf (stderr, "\n%i: eINIT: visualiser einit-vis-text activated.\n", (int)time(NULL));
     } else {
      perror ("einit-feedback-visual-textual: opening stderr");
     }
    } else {
     if (!(coremode & einit_mode_sandbox)) {
      if (strmatch (filenode->arbattrs[i], "console")) {
       int tfd = 0;
       errno = 0;
       if ((tfd = open (filenode->arbattrs[i+1], O_WRONLY, 0)) > 0) {
        fcntl (tfd, F_SETFD, FD_CLOEXEC);
        ioctl (tfd, TIOCCONS, 0);
       }
       if (errno)
        perror (filenode->arbattrs[i+1]);
      } else if (strmatch (filenode->arbattrs[i], "kernel-vt")) {
       int arg = (strtol (filenode->arbattrs[i+1], (char **)NULL, 10) << 8) | 11;
       errno = 0;

       ioctl(0, TIOCLINUX, &arg);
       if (errno)
        perror ("einit-feedback-visual-textual: redirecting kernel messages");
      } else if (!linux_sysconf_block_chvt && strmatch (filenode->arbattrs[i], "activate-vt")) {
       uint32_t vtn = strtol (filenode->arbattrs[i+1], (char **)NULL, 10);
       int tfd = 0;
       errno = 0;
       if ((tfd = open ("/dev/tty1", O_RDWR, 0)) > 0) {
        fcntl (tfd, F_SETFD, FD_CLOEXEC);
        ioctl (tfd, VT_ACTIVATE, vtn);
       }
       if (errno)
        perror ("einit-feedback-visual-textual: activate terminal");
       if (tfd > 0) close (tfd);
      }
     }
    }
   }
  }
 }
}

void linux_sysconf_sysctl () {
 FILE *sfile;
 char *sfilename;

 linux_sysconf_fix_ttys();

 if ((sfilename = cfg_getstring ("configuration-services-sysctl/config", NULL))) {
  notice (4, "doing system configuration via %s.", sfilename);

  if ((sfile = efopen (sfilename, "r"))) {
   char buffer[BUFFERSIZE], *cptr;
   while (fgets (buffer, BUFFERSIZE, sfile)) {
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
        char tarbuffer[BUFFERSIZE];

        strcpy (tarbuffer, "/proc/sys/");

        *cptr = 0;
        cptr++;

        strtrim (buffer);
        strtrim (cptr);

        for (; buffer[ci]; ci++) {
         if (buffer[ci] == '.') buffer[ci] = '/';
        }

        strncat (tarbuffer, buffer, sizeof(tarbuffer) - strlen (tarbuffer) + 1);

        if ((ofile = efopen(tarbuffer, "w"))) {
         eputs (cptr, ofile);
         efclose (ofile);
        }
       }
      }

      break;
    }
   }

   efclose (sfile);
  }
 }
}

void linux_sysconf_boot_devices_available(struct einit_event *ev) {
 linux_sysconf_sysctl();
 linux_sysconf_hwclock();
}

void linux_sysconf_einit_core_mode_switch_done (struct einit_event *ev) {
 if (strmatch (ev->string, "power-down")) {
  linux_power_off ();
 } else if (strmatch (ev->string, "power-reset")) {
  linux_reboot ();
 }
}

int linux_sysconf_enable (void *na, struct einit_event *status) {
 fbprintf (status, "pruning wtmp and utmp files");

 FILE *f = fopen("/var/run/utmp", "w");
 if (f) { fclose (f); }
 f = fopen("/var/log/wtmp", "w");
 if (f) { fclose (f); }

 return status_ok;
}

int linux_sysconf_disable (void *na, struct einit_event *status) {
 return status_ok;
}

int linux_sysconf_configure (struct lmodule *irr) {
 module_init (irr);

 irr->enable = linux_sysconf_enable;
 irr->disable = linux_sysconf_disable;

 event_listen (einit_boot_early, linux_sysconf_ctrl_alt_del);
 event_listen (einit_boot_devices_available, linux_sysconf_boot_devices_available);

 event_listen (einit_boot_root_device_ok, linux_sysconf_root_ok);

 event_listen (einit_core_service_enabled, linux_sysconf_service_enabled);
 event_listen (einit_core_mode_switch_done, linux_sysconf_einit_core_mode_switch_done);

 event_listen (einit_power_down_scheduled, linux_sysconf_shutdown);
 event_listen (einit_power_reset_scheduled, linux_sysconf_shutdown);

 return 0;
}
