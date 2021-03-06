/*
 *  einit-helper.c
 *  einit
 *
 *  Created by Magnus Deininger on 02/12/2007.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>

#include <einit/configuration-static.h>

#include <einit/einit.h>

char o_use_running_einit = 0;
char o_sandbox = 0;

void help_preface (char *argv0) {
 fprintf (stdout, "Usage: %s [options]\n\n"

                  " :: Manipulating a running instance of eINIT (Live) ::\n"
                  " -e <service>         Enable <service>\n"
                  " -d <service>         Disable <service>\n"
                  " -c <service> <f>     Call Custom Hook <f> on <service>\n\n"

                  " -em <module>         Enable <module>\n"
                  " -dm <module>         Disable <module>\n"
                  " -cm <module> <f>     Call Custom Hook <f> on <module>\n\n"

                  " -H, -D               Shut Down\n"
                  " -R                   Reboot\n\n"

                  " -m <mode>            Switch to <mode>\n\n"

                  " -u [file]            Update Configuration/Add [file]\n\n"

                  " :: Raw 9p I/O ::\n"
                  " [-]ls <path>         ls <path>\n"
                  " [-]read <path>       read <path>\n"
                  " [-]write <path> <t>  write <t> to <path>\n\n"

                  " :: Advanced Options ::\n"
                  " --wtf                Examine Configuration Files\n"
                  " -q                   Force doing all calls on a running instance of eINIT\n"
                  " -p                   Force doing all calls on a private instance of eINIT\n"
                  " -L, --licence        Display Licence\n"
                  " -v, --version        Display Version\n\n"
                  " -a <address>         Specify a custom 9p Address to use for IPC\n\n"

                  " :: Core Help ::\n", argv0);
}

int main(int argc, char **argv, char **env) {
 char c_version = 0;
 char c_licence = 0;
 char c_help = 0;
 char c_wtf = 0;

 int i = 0;

 char *c_service[2] = { NULL, NULL };
 char *c_module[2] = { NULL, NULL };
 char *c_mode = NULL;
 char c_down = 0;
 char c_reset = 0;

 char o_cake = 0;

 char *c_ls = NULL;
 char *c_read = NULL;
 char *c_write[2] = { NULL, NULL };

 char *c_update = NULL;

 for (i = 0; i < argc; i++) {
  if (strmatch (argv[i], "-v") || strmatch (argv[i], "--version")) {
   c_version = 1;
  } else if (strmatch (argv[i], "-L") || strmatch (argv[i], "--licence")) {
   c_licence = 1;
  } else if (strmatch (argv[i], "-h") || strmatch (argv[i], "--help")) {
   c_help = 1;
  } else if (strmatch (argv[i], "-q") || strmatch (argv[i], "--live")) {
   o_use_running_einit = 1;
  } else if (strmatch (argv[i], "--sandbox")) {
   o_sandbox = 1;
  } else if (strmatch (argv[i], "--wtf")) {
   c_wtf = 1;
  } else if (strmatch (argv[i], "-e") && ((i+1) < argc)) {
   c_service [0] = argv[i+1];
   c_service [1] = "enable";
   i++;
  } else if (strmatch (argv[i], "-d") && ((i+1) < argc)) {
   c_service [0] = argv[i+1];
   c_service [1] = "disable";
   i++;
  } else if (strmatch (argv[i], "-c") && ((i+2) < argc)) {
   c_service [0] = argv[i+1];
   c_service [1] = argv[i+2];
   i+=2;
  } else if (strmatch (argv[i], "-em") && ((i+1) < argc)) {
   c_module [0] = argv[i+1];
   c_module [1] = "enable";
   i++;
  } else if (strmatch (argv[i], "-dm") && ((i+1) < argc)) {
   c_module [0] = argv[i+1];
   c_module [1] = "disable";
   i++;
  } else if (strmatch (argv[i], "-cm") && ((i+2) < argc)) {
   c_module [0] = argv[i+1];
   c_module [1] = argv[i+2];
   i+=2;
  } else if (strmatch (argv[i], "-H") || strmatch (argv[i], "-D")) {
   c_down = 1;
  } else if (strmatch (argv[i], "-R")) {
   c_reset = 1;
  } else if (strmatch (argv[i], "-m") && ((i+1) < argc)) {
   c_mode = argv[i+1];
   i++;
  } else if (strmatch (argv[i], "-cake")) {
   o_cake = 1;
  } else if ((strmatch (argv[i], "-ls") || strmatch (argv[i], "ls")) && ((i+1) < argc)) {
   c_ls = argv[i+1];
   i++;
  } else if ((strmatch (argv[i], "-read") || strmatch (argv[i], "read")) && ((i+1) < argc)) {
   c_read = argv[i+1];
   i++;
  } else if ((strmatch (argv[i], "-write") || strmatch (argv[i], "write")) && ((i+2) < argc)) {
   c_write[0] = argv[i+1];
   c_write[1] = argv[i+2];
   i+=2;
  } else if (strmatch (argv[i], "-u")) {
   if ((i+1) < argc) {
    c_update = argv[i+1];
    i++;
   } else {
    c_update = "update";
   }
  }
 }

 if (!c_version && !c_licence && !c_wtf && !c_service[0] && !c_module[0] && !c_mode && !c_down && !c_reset && !o_cake && !c_ls && !c_read && !c_write[0] && !c_update)
  c_help = 1;

 if (o_cake) {
  printf ("THE CAKE IS A LIE\n");
 }

 if (c_mode || c_service[0] || c_module[0] || c_down || c_reset || c_ls || c_read || c_write[0] || c_update) {
  if (!einit_connect(&argc, argv)) {
   perror ("Could not connect to eINIT");
   exit (EXIT_FAILURE);
  }

  if (c_mode) {
   einit_switch_mode (c_mode);
  }

  if (c_down) {
   einit_power_down ();
  }

  if (c_reset) {
   einit_power_reset ();
  }

  if (c_service[0]) {
   einit_service_call (c_service[0], c_service[1]);
  }

  if (c_module[0]) {
   einit_module_call (c_module[0], c_module[1]);
  }

  if (c_update) {
   char *path[3];
   path[0] = "configuration";
   path[1] = "update";
   path[2] = NULL;

   einit_write (path, c_update);
  }

  char *t = NULL;
  if ((t = c_ls) || (t = c_read) || (t = c_write[0])) {
   char **path = NULL;
   while (t[0] == '/') t++;

   if (t[0]) {
    path = str2set ('/', t);
   }

   if (c_ls) {
    char **files = einit_ls (path);

    if (files) {
     for (i = 0; files[i]; i++) {
      puts (files[i]);
     }

     efree (files);
    }
   } else if (c_read) {
    char *b = einit_read (path);
    if (b) {
     fputs (b, stdout);

     efree (b);
    }
   } else if (c_write[1]) {
    einit_write (path, c_write[1]);
   }
  }

  einit_disconnect();

  exit (EXIT_SUCCESS);
 }

 if (c_version || c_licence || c_help || c_wtf) {
  char **c = NULL;

  c = set_str_add (c, argv[0]);
  if (c_version)
   c = set_str_add (c, "-v");
  if (c_licence)
   c = set_str_add (c, "-L");
  if (c_help)
   c = set_str_add (c, "--help");
  if (o_sandbox)
   c = set_str_add (c, "--sandbox");

  if (c_wtf) {
   int argcx = o_sandbox ? 2 : 1;
   char *argvx[3];

   argvx[0] = "einit";
   argvx[1] = o_sandbox ? "-ps" : NULL;
   argvx[2] = NULL;

   if (einit_connect_spawn (&argcx, argvx)) {
    argvx[0] = "issues";
    argvx[1] = NULL;

    char **issues = einit_ls (argvx);

    if (issues) {
     int count = setcount ((const void **)issues);
     int i = 0;

     if (count > 1)
      fprintf (stdout, "Found %i issues:\n", count);
     else
      fputs ("Found one issue:\n", stdout);

     while (issues[i]) {
      fprintf (stdout, " * %s:\n", issues[i]);

      argvx[1] = issues[i];

      char *r = einit_read(argvx);
      if (r) {
       fprintf (stdout, " >> %s\n", r);
      }

      i++;
     }
    } else {
     fprintf (stdout, "No issues found.\n");
    }

    fflush (stdout);

    einit_disconnect ();
   } else {
    perror ("Could not connect to eINIT");
   }

   return 0;
  } else {
   if (c_help)
    help_preface(argv[0]);

   execve (EINIT_LIB_BASE "/bin/einit-core", c, env);
   perror ("Could not execute eINIT!");
   return -1;
  }
 }

 return 0;
}
