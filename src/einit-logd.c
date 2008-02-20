/*
 *  einit-logd.c
 *  einit
 *
 *  Created by Magnus Deininger on 16/02/2008.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>

#include <einit/event.h>

#include <einit/configuration-static.h>

#include <einit/einit.h>

void event_handler_feedback_notice (struct einit_event *ev) {
 if (ev->string)
  fprintf (stdout, "[notice] %i: %s\n", ev->flag, ev->string);
}

void event_handler_update_module_status (struct einit_event *ev) {
 if (ev->string)
  fprintf (stdout, "[%s] %s\n", ev->rid, ev->string);
}

void event_handler_update_service_enabled (struct einit_event *ev) {
 fprintf (stdout, "[%s] enabled\n", ev->string);
}

void event_handler_update_service_disabled (struct einit_event *ev) {
 fprintf (stdout, "[%s] disabled\n", ev->string);
}

void help () {
}

int main(int argc, char **argv, char **env) {
 char follow = 0;
 int i = 1;

 for (; argv[i]; i++) {
  if (strmatch (argv[i], "-f") || strmatch (argv[i], "--follow")) {
   follow = 1;
  } else if (strmatch (argv[i], "-n") || strmatch (argv[i], "--replay-only")) {
   follow = 0;
  } else if (strmatch (argv[i], "-h") || strmatch (argv[i], "--help")) {
   help();
   return EXIT_SUCCESS;
  } else {
   help();
   return EXIT_FAILURE;
  }
 }

 if (!einit_connect(&argc, argv)) {
  perror ("Could not connect to eINIT");
  sleep (1);
  if (!einit_connect(&argc, argv)) {
   perror ("Could not connect to eINIT, giving up");
   exit (EXIT_FAILURE);
  }
 }

 event_listen (einit_feedback_notice, event_handler_feedback_notice);
 event_listen (einit_feedback_module_status, event_handler_update_module_status);
 event_listen (einit_core_service_enabled, event_handler_update_service_enabled);
 event_listen (einit_core_service_disabled, event_handler_update_service_disabled);

 if (follow)
  einit_event_loop();
 else
  einit_replay_events();

 einit_disconnect();

 return 0;
}
