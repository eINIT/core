/*
 *  crash-handler.c
 *  einit
 *
 *  Created by Magnus Deininger on 16/04/2007.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <errno.h>

#ifdef LINUX
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
/* okay, i think i found the proper file now */
#include <asm/ioctls.h>
#include <linux/vt.h>
#endif

int main(int, char **);

int main(int argc, char **argv) {
 int i;
 char *signal = NULL, *exitstatus = NULL;
 int tfd = 0;
 int arg;
 FILE *tmp;

 errno = 0;

 if (argc < 3) {
  eprintf (stderr, "crash-handler: not enough arguments (%i).\n", argc);
 }

 for (i = 1; argv[i]; i++) {
  if (strmatch (argv[i], "--signal")) {
   signal = argv[i+1];
   break;
  }
  if (strmatch (argv[i], "--exit")) {
   exitstatus = argv[i+1];
   break;
  }
 }

#ifdef LINUX
 if (tmp = freopen ("/dev/tty1", "w", stderr))
  stderr = tmp;
 if (tmp = freopen ("/dev/tty1", "w", stdout))
  stdout = tmp;
 if (stdin = freopen ("/dev/tty1", "r", stdin))
  stdin = tmp;

 arg = (1 << 8) | 11;
 errno = 0;

 ioctl(0, TIOCLINUX, &arg);
 if (errno)
  perror ("crash-handler: redirecting kernel messages");

 if ((tfd = open ("/dev/tty1", O_RDWR, 0)))
  ioctl (tfd, VT_ACTIVATE, 0);
 if (errno)
  perror ("crash-handler: activate terminal");
 if (tfd > 0) close (tfd);
#endif

 if (signal) {
  eprintf (stderr, "\r\neINIT has died by signal %s.\r\n", signal);
 }

 if (exitstatus) {
  eprintf (stderr, "\r\neINIT has exited with status %s.\r\n", exitstatus);
 }

 system ("/sbin/sulogin -t 60 /dev/console");

 return 0;
}
