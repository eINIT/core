/*
 *  linux-hotplug.c
 *  einit
 *
 *  Created on 14/10/2007.
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
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>

#include <einit-modules/exec.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_hotplug_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_hotplug_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Hotplug (Linux)",
 .rid       = "linux-hotplug",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_hotplug_configure
};

module_register(linux_hotplug_self);

#endif

void linux_hotplug_hotplug_event_handler (struct einit_event *ev) {
 if (ev->stringset) {
  char *subsystem = NULL;
  char **commands = NULL;
  int i = 0;

  for (; ev->stringset[i]; i+=2) {
   if (strmatch (ev->stringset[i], "SUBSYSTEM")) {
    subsystem = ev->stringset[i+1];
   }
  }

  if (subsystem) {
   char n = 0;

   for (; n < 4; n++) {
    char buffer[BUFFERSIZE];
    char *tbuffer = (n == 1) ? "/etc/einit/hotplug.d/default/" : "/etc/hotplug.d/default/";

    switch (n) {
     case 0:
      esprintf(buffer, BUFFERSIZE, "/etc/einit/hotplug.d/%s/", subsystem);
      tbuffer = buffer;
      break;
     case 2:
      esprintf(buffer, BUFFERSIZE, "/etc/hotplug.d/%s/", subsystem);
      tbuffer = buffer;
      break;
     case 1:
     case 3:
      break;
     default:
      tbuffer = NULL;
      break;
    }

    if (tbuffer) {
     struct stat st;

     if (!stat (tbuffer, &st) && S_ISDIR(st.st_mode)) {
      char **cm = readdirfilter (NULL, tbuffer, "\\.hotplug$", NULL, 0);

      if (cm) {
       commands = (char **)setcombine_nc ((void **)commands, (const void **)cm, SET_TYPE_STRING);
       free (cm);
      }
     }
    }
   }
  }

  if (commands) {
   char **env = NULL;
   char *command;
   ssize_t blen = strlen (subsystem) + 2;
   char **cd = NULL;

   for (i = 0; ev->stringset[i]; i+=2) {
    env = straddtoenviron (env, ev->stringset[i], ev->stringset[i+1]);
   }

   for (i = 0; commands[i]; i++) {
    int len = blen + strlen (commands[i]);
    char *t = emalloc (len);

    esprintf (t, len, "%s %s", commands[i], subsystem);
    cd = (char **)setadd ((void **)cd, t, SET_TYPE_STRING);
    free (t);
   }

   if (cd) {
    command = set2str (';', (const char **)cd);

    pexec(command, NULL, 0, 0, NULL, NULL, env, NULL);

//    fprintf (stderr, "executing: %s\n", command);

    free (cd);
    free (command);
   }

   free (env);
  }
 }
}

int linux_hotplug_cleanup (struct lmodule *tm) {
 exec_cleanup (tm);

 event_ignore (einit_event_subsystem_hotplug, linux_hotplug_hotplug_event_handler);

 return 1;
}

int linux_hotplug_configure (struct lmodule *tm) {
 module_init (tm);
 exec_configure (tm);

 event_listen (einit_event_subsystem_hotplug, linux_hotplug_hotplug_event_handler);

 return 1;
}
