/*
 *  einit-helper.c
 *  einit
 *
 *  Created by Magnus Deininger on 02/12/2007.
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

void help_preface () {
 fprintf (stdout, " :: Advanced Options ::\n"
                  " --wtf                Examine Configuration Files\n\n"
                  " :: Core Help ::\n");
}

int main(int argc, char **argv, char **env) {
 char c_version = 0;
 char c_licence = 0;
 char c_help = 0;
 char c_wtf = 0;

 int i = 0;

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
  }
 }

 if (!c_version && !c_licence && !c_wtf)
  c_help = 1;

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
      fprintf (stderr, "Found %i issues:\n", count);
     else
      fputs ("Found one issue:\n", stderr);

     while (issues[i]) {
      fprintf (stderr, " * %s:\n", issues[i]);

      argvx[1] = issues[i];

      char *r = einit_read(argvx);
      if (r) {
       fprintf (stderr, " >> %s\n", r);
      }

      i++;
     }
    } else {
     fprintf (stderr, "No issues found.\n");
    }

    fflush (stderr);

    einit_disconnect ();
   } else {
    fprintf (stderr, "Can't connect to eINIT.\n");
   }

   return 0;
  } else {
   if (c_help)
    help_preface();

   execve (EINIT_LIB_BASE "/bin/einit-core", c, env);
   perror ("couldn't execute eINIT!");
   return -1;
  }
 }

 perror ("what nao?");
 return 0;
}
