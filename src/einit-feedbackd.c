/*
 *  einit-feedbackd.c
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

#include <curses.h>

void update() {
 refresh();
}

void event_handler_mode_switching (struct einit_event *ev) {
 char buffer[BUFFERSIZE];

 snprintf (buffer, BUFFERSIZE, "switching to mode: %s\n", ev->string);
 addstr (buffer);

 update();
}

void event_handler_mode_switch_done (struct einit_event *ev) {
 char buffer[BUFFERSIZE];
 snprintf (buffer, BUFFERSIZE, "switch complete: %s\n", ev->string);
 addstr (buffer);

 update();
}

void event_handler_update_module_status (struct einit_event *ev) {
 char buffer[BUFFERSIZE];
 if (ev->status & status_working) {
  snprintf (buffer, BUFFERSIZE, "now working on: %s\n", ev->rid);
  addstr (buffer);
 } else if (ev->status & status_failed) {
  snprintf (buffer, BUFFERSIZE, "manipulation failed: %s\n", ev->rid);
  addstr (buffer);
 } else if (ev->status & status_enabled) {
  snprintf (buffer, BUFFERSIZE, "now enabled: %s\n", ev->rid);
  addstr (buffer);
 } else if (ev->status & status_enabled) {
  snprintf (buffer, BUFFERSIZE, "now disabled: %s\n", ev->rid);
  addstr (buffer);
 }

 if (ev->string) {
  snprintf (buffer, BUFFERSIZE, " > %s: %s\n", ev->rid, ev->string);
  addstr (buffer);
 }

 update();
}

void event_handler_update_service_enabled (struct einit_event *ev) {
 char buffer[BUFFERSIZE];
 snprintf (buffer, BUFFERSIZE, "enabled: %s\n", ev->string);
 addstr (buffer);

 update();
}

void event_handler_update_service_disabled (struct einit_event *ev) {
 char buffer[BUFFERSIZE];
 snprintf (buffer, BUFFERSIZE, "disabled: %s\n", ev->string);
 addstr (buffer);

 update();
}

int main(int argc, char **argv, char **env) {
 initscr();
 cbreak();
 noecho();

 nonl();
 intrflush(stdscr, FALSE);
 keypad(stdscr, TRUE);

 if (!einit_connect(&argc, argv)) {
  perror ("Could not connect to eINIT");
  exit (EXIT_FAILURE);
 }

 event_listen (einit_core_mode_switching, event_handler_mode_switching);
 event_listen (einit_core_mode_switch_done, event_handler_mode_switch_done);
 event_listen (einit_feedback_module_status, event_handler_update_module_status);
 event_listen (einit_core_service_enabled, event_handler_update_service_enabled);
 event_listen (einit_core_service_disabled, event_handler_update_service_disabled);

 einit_event_loop();

 endwin ();

 einit_disconnect();

 return 0;
}
