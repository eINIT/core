/*
 *  einit.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/scheduler.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

int main(int, char **);
int print_usage_info ();
int ipc_process (char *);
int ipc_wait ();
int cleanup ();

int print_usage_info () {
 fputs ("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006, Magnus Deininger\nUsage:\n einit [-c configfile] [-v] [-h]\n", stderr);
 return -1;
}

int cleanup () {
 mod_freemodules ();
 cfg_free ();
 bitch (BTCH_DL + BTCH_ERRNO);
}

int main(int argc, char **argv) {
 int i;
#ifdef SANDBOX
 char *cfgfile = "etc/einit/sandbox.xml";
#else
 char *cfgfile = "/etc/einit/default.xml";
#endif

 uname (&osinfo);

 for (i = 1; i < argc; i++) {
  if (argv[i][0] == '-')
   switch (argv[i][1]) {
    case 'c':
     if ((++i) < argc)
      cfgfile = argv[i];
     else
      return print_usage_info ();
     break;
    case 'h':
     return print_usage_info ();
     break;
    case 'v':
     puts("eINIT " EINIT_VERSION_LITERAL "\nCopyright (c) 2006, Magnus Deininger");
     return 0;
   }
 }
 printf ("eINIT %s: booting %s\n", EINIT_VERSION_LITERAL, osinfo.sysname);

 if (pthread_attr_init (&thread_attribute_detached)) {
  fputs ("pthread initialisation failed.\n", stderr);
  return -1;
 } else
  pthread_attr_setdetachstate (&thread_attribute_detached, PTHREAD_CREATE_DETACHED);

 if (cfg_load (cfgfile) == -1) {
  fputs ("ERROR: cfg_load() failed\n", stderr);
  return -1;
 }
 mod_scanmodules ();
 sched_init ();

 sched_queue (SCHEDULER_SWITCH_MODE, "feedback");
 sched_queue (SCHEDULER_SWITCH_MODE, "default");

 sched_run (NULL);

 cleanup ();

 return 0;
}
