/*
 *  einit-control.c
 *  einit
 *
 *  Created by Magnus Deininger on 03/05/2006.
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

#include <stdio.h>
#include <unistd.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <einit/config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <libgen.h>

int main(int, char **);
int print_usage_info ();
int ipc (char *);

char *ctrlsocket = "/etc/einit-control";

int print_usage_info () {
 fputs ("eINIT " EINIT_VERSION_LITERAL " Control\nCopyright (c) 2006, Magnus Deininger\nUsage:\n einit-control [-s control-socket] [-v] [-h] [function] [--] command\n [function] [-s control-socket] [-v] [-h] [--] command\n\npossible commands for function \"power\":\n down   tell einit to shut down the computer\n reset  reset/reboot the computer\n\nNOTE: calling einit-control [function] [command] is equivalent to calling [function] [command] directly.\n  (provided that the proper symlinks are in place.)\n", stderr);
 return -1;
}

int ipc (char *cmd) {
 int sock = socket (AF_UNIX, SOCK_STREAM, 0);
 char buffer[1024];
 struct sockaddr_un saddr;
 int len;
 if (sock == -1)
  return bitch (BTCH_ERRNO);

 saddr.sun_family = AF_UNIX;
 strncpy (saddr.sun_path, ctrlsocket, sizeof(saddr.sun_path) - 1);

 if (connect(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un))) {
  close (sock);
  return bitch (BTCH_ERRNO);
 }

 while ((len = write (sock, cmd, strlen(cmd)+1)) != -1) {
  if (len < strlen (cmd)) cmd += len;
  else break;
 }
 if (len == -1) bitch (BTCH_ERRNO);
 while ((len = read (sock, buffer, 1023)) > 0) {
  buffer[len] = 0;
  fputs (buffer, stdout);
 }
 if (len == -1) bitch (BTCH_ERRNO);

 close (sock);
 return 0;
}

int main(int argc, char **argv) {
 int i, l;
 char *c = emalloc (1*sizeof (char));
 char *name = estrdup ((char *)basename(argv[0]));
 c[0] = 0;
 if (!strcmp (name, "erc")) {
  c = (char *)erealloc (c, 3*sizeof (char));
  c = strcat (c, "rc");
 } else if (strcmp (name, "einit-control")) {
  c = (char *)erealloc (c, (1+strlen(name))*sizeof (char));
  c = strcat (c, name);
 }

 for (i = 1; i < argc; i++) {
  if (argv[i][0] == '-')
   switch (argv[i][1]) {
    case 's':
     if ((++i) < argc)
      ctrlsocket = argv[i];
     else
      return print_usage_info ();
     break;
    case 'h':
     return print_usage_info ();
     break;
    case 'v':
     puts("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006, Magnus Deininger");
     return 0;
    case '-':
     i++;
     if (i < argc) goto copy_remainder_verbatim;
     return 0;
   }
  else while (i < argc) {
   copy_remainder_verbatim:
   l = strlen(c);
   if (l) {
    c = erealloc (c, (l+2+strlen(argv[i]))*sizeof (char));
    c[l] = ' ';
    c[l+1] = 0;
   } else {
    c = erealloc (c, (1+strlen(argv[i]))*sizeof (char));
   }
   c = strcat (c, argv[i]);

   i++;
  }
 }

 l = strlen(c);
 c = erealloc (c, (l+11)*sizeof (char));
 c = strcat (c, "\nIPC//out\n\0");

 ipc(c);

 bitch (BTCH_DL + BTCH_ERRNO);
 return 0;
}
