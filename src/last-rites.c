/*
 *  last-rites.c
 *  einit
 *
 *  make REALLY sure that EVERYTHING is unmounted.
 *
 *  Created by Magnus Deininger on 21/08/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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

#ifdef __linux__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#include <einit/config.h>
#include <einit/utility.h>

#include <sys/reboot.h>
#include <linux/reboot.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

#include <linux/loop.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

// let's be serious, /sbin is gonna exist
#define LRTMPPATH "/sbin"

#define MAX_RETRIES 5

#if 0
#define kill(a,b) 1
#endif

#ifdef __NR_pivot_root
#define pivot_root(new_root,put_old) syscall(__NR_pivot_root, new_root, put_old)
#endif

int unmount_everything() {
 int errors = 0;
 int positives = 0;
 FILE *fp;

 if ((fp = fopen ("/proc/mounts", "r"))) {
  char buffer[BUFFERSIZE];
  errno = 0;
  while (!errno) {
   if (!fgets (buffer, BUFFERSIZE, fp)) {
    switch (errno) {
     case EINTR:
     case EAGAIN:
      errno = 0;
      break;
     case 0:
      goto done_parsing_file;
     default:
      perror("fgets() failed.");
      goto done_parsing_file;
    }
   } else if (buffer[0] != '#') {
    strtrim (buffer);

    if (buffer[0]) {
     char *cur = estrdup (buffer);
     char *scur = cur;
     uint32_t icur = 0;

     char *fs_spec = NULL;
     char *fs_file = NULL;
     char *fs_vfstype = NULL;
     char *fs_mntops = NULL;
     int fs_freq = 0;
     int fs_passno = 0;

     strtrim (cur);
     for (; *cur; cur++) {
      if (isspace (*cur)) {
       *cur = 0;
       icur++;
       switch (icur) {
        case 1: fs_spec = scur; break;
        case 2: fs_file = scur; break;
        case 3: fs_vfstype = scur; break;
        case 4: fs_mntops = scur; break;
        case 5: fs_freq = (int) strtol(scur, (char **)NULL, 10); break;
        case 6: fs_passno = (int) strtol(scur, (char **)NULL, 10); break;
       }
       scur = cur+1;
       strtrim (scur);
      }
     }
     if (cur != scur) {
      icur++;
      switch (icur) {
       case 1: fs_spec = scur; break;
       case 2: fs_file = scur; break;
       case 3: fs_vfstype = scur; break;
       case 4: fs_mntops = scur; break;
       case 5: fs_freq = (int) strtol(scur, (char **)NULL, 10); break;
       case 6: fs_passno = (int) strtol(scur, (char **)NULL, 10); break;
      }
     }

     if (fs_spec && strstr (fs_file, "/old") == fs_file) {
      fprintf (stderr, "still mounted: %s\n", fs_file);

      if (umount (fs_file) && umount2(fs_file, MNT_FORCE)) {
       perror (fs_file);
       errors++;

       if (fs_spec && fs_file && fs_vfstype) {
        if (mount(fs_spec, fs_file, fs_vfstype, MS_REMOUNT | MS_RDONLY, "")) {
         fprintf (stderr, "couldn't remount %s either\n", fs_file);
         perror (fs_file);
        } else
         fprintf (stderr, "remounted %s read-only\n", fs_file);
       } else {
        fprintf (stderr, "can't remount: bad data\n");
       }

#ifdef MNT_EXPIRE
       /* can't hurt to try this one */
       umount2(fs_file, MNT_EXPIRE);
       umount2(fs_file, MNT_EXPIRE);
#endif
      } else {
       positives = 1;
       fprintf (stderr, "unmounted %s\n", fs_file);
      }
     }

     errno = 0;
    }
   }
  }
  done_parsing_file:
  fclose (fp);
 }

 if (positives) {
  return unmount_everything();
 }

 return errors;
}

#if 0
void kill_everything() {
 DIR *d = opendir("/proc");

 if (d) {
  struct dirent *e;

  while ((e = readdir(d))) {
   if (e->d_name && e->d_name[0]) {
    pid_t pidtokill = atoi (e->d_name);

    if ((pidtokill > 0) && (pidtokill != 1) && (pidtokill != getpid())) {
     if (kill (pidtokill, SIGKILL)) fprintf (stderr, "couldn't send SIGKILL to %i\n", pidtokill);
    }
   }
  }

  closedir (d);
 }
}
#else
#define MAX_PID (4096*8 + 1)

void kill_everything() {
 pid_t pid = 2; /* don't kill 1 (us) */

 for (; pid < MAX_PID; pid++) {
  kill (pid, SIGKILL);
 }
}
#endif

void close_all_loops() {
 DIR *d = opendir("/dev/loop");

 if (d) {
  struct dirent *e;

  while ((e = readdir(d))) {
   char filename [1024];
   int loopfd;

   snprintf (filename, 1024, "/dev/loop/%s", e->d_name);

   if ((loopfd = open(filename, O_RDONLY)) >= 0) {
    ioctl (loopfd, LOOP_CLR_FD, 0);

    close (loopfd);
   }
  }

  closedir (d);
 }
}

void prune_file_descriptors () {
 fprintf (stderr, "pruning all file descriptors...\n");
 fclose (stdout);
 fclose (stderr);
 fclose (stdin);

 int i = 0;

 for (; i < 4096*8; i++) {
  close (i);
 }
}

void reopen_stdout_and_stderr () {
 stdout = fopen ("/dev/console", "w");
 stderr = fopen ("/dev/console", "w");

 fprintf (stderr, "stdout and stderr reopened\n");
}

void reopen_stdout_and_stderr_second_time () {
 fclose (stdout);
 fclose (stderr);

 stdout = fopen ("/dev/console", "w");
 stderr = fopen ("/dev/console", "w");

 fprintf (stderr, "stdout and stderr reopened (2)\n");
}

int lastrites () {
 unsigned char i;

 if (mount ("lastrites", LRTMPPATH, "tmpfs", 0, "")) {
  perror ("couldn't mount my tmpfs at " LRTMPPATH);
//  return -1;
 }

 if (mkdir (LRTMPPATH "/old", 0777)) perror ("couldn't mkdir '" LRTMPPATH "/old'");
 if (mkdir (LRTMPPATH "/proc", 0777)) perror ("couldn't mkdir '" LRTMPPATH "/proc'");
 if (mkdir (LRTMPPATH "/dev", 0777)) perror ("couldn't mkdir '" LRTMPPATH "/dev'");
 if (mkdir (LRTMPPATH "/dev/loop", 0777)) perror ("couldn't mkdir '" LRTMPPATH "/dev/loop'");
 if (mkdir (LRTMPPATH "/tmp", 0777)) perror ("couldn't mkdir '" LRTMPPATH "/tmp'");


 for (i = 0; i < 9; i++) {
  dev_t ldev = (((7) << 8) | (i));
  char tmppath[256];
  uint32_t ni = i;

  snprintf (tmppath, 256, LRTMPPATH "/dev/loop/%i", ni);
  mknod (tmppath, S_IFBLK, ldev);
 }

 dev_t ldev = (5 << 8) | 1;
 mknod (LRTMPPATH "/dev/console", S_IFCHR, ldev);
 ldev = (4 << 8) | 1;
 mknod (LRTMPPATH "/dev/tty1", S_IFCHR, ldev);
 ldev = (1 << 8) | 3;
 mknod (LRTMPPATH "/dev/null", S_IFCHR, ldev);

 if (mount ("lastrites-proc", LRTMPPATH "/proc", "proc", 0, "")) perror ("couldn't mount another 'proc' at '" LRTMPPATH "/proc'");
// if (mount ("/dev", LRTMPPATH "/dev", "", MS_BIND, "")) perror ("couldn't bind another 'dev'");

 if (chdir (LRTMPPATH "/old")) perror ("chdir failed");

 if (pivot_root (LRTMPPATH, LRTMPPATH "/old")) perror ("couldn't pivot_root('" LRTMPPATH "', '" LRTMPPATH "/old')");

 if (chdir ("/")) perror ("chdir failed");

 char max_retries = MAX_RETRIES;

 reopen_stdout_and_stderr_second_time ();

 do {
  if (max_retries != MAX_RETRIES)
   sleep (1);

  max_retries--;

  kill_everything();
  sync();
  close_all_loops();
  sync();
 } while (unmount_everything() && max_retries);

 sleep(1);

 return 0;
}

int main(int argc, char **argv) {
 char action = argv[1] ? argv[1][0] : '?';

 fprintf (stderr, "\e[2J >> eINIT " EINIT_VERSION_LITERAL " | last rites (%i) <<\n"
   "###############################################################################\n", getpid());

 prune_file_descriptors ();
 reopen_stdout_and_stderr ();

 lastrites();

 switch (action) {
  case 'k':

#if defined(LINUX_REBOOT_MAGIC1) && defined (LINUX_REBOOT_MAGIC2) && defined (LINUX_REBOOT_CMD_KEXEC) && defined (__NR_reboot)
   syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_KEXEC, 0);
   fprintf (stderr, "whoops, looks like the kexec failed!\n");
#else
   fprintf (stderr, "no support for kexec?\n");
#endif

  case 'r':
   reboot (LINUX_REBOOT_CMD_RESTART);
   fprintf (stderr, "can't reboot?\n");

  case 'h':
   reboot (LINUX_REBOOT_CMD_POWER_OFF);
   fprintf (stderr, "can't shut down?\n");

  default:
   fprintf (stderr, "exiting... this is bad.\n");
   exit (EXIT_FAILURE);
   break;
 }

 return 0;
}

#else

#include <stdio.h>

int main(int argc, char **argv) {
 fprintf (stderr, "last-rites (%c), only supported on linux, sorry.\n", argv[1] ? argv[1][0] : '?');

 return 0;
}

#endif
