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
#include <einit/tree.h>

#include <curses.h>

struct module_status {
 enum einit_module_status status;
 char *name;
};

#define attr_red 1
#define attr_blue 2
#define attr_green 3
#define attr_yellow 4

struct stree *status_tree = NULL;

int progress = 0;
char *mode = "(none)";
char *mode_to = "(none)";

void set_module_status (char *name, enum einit_module_status status) {
 struct stree *e;
 if (!status_tree || !(e = streefind (status_tree, name, tree_find_first))) {
  struct module_status s;

  s.status = status;
  s.name = einit_module_get_name(name);

  status_tree = streeadd (status_tree, name, &s, sizeof (s), NULL);
 } else {
  struct module_status *s = e->value;

  if (s->status & status_failed) {
    s->status = status | ((status & status_enabled) ? status_failed : 0);
  } else
   s->status = status;
 }
}

void move_to_right_border () {
 int x, y, maxx, maxy;

 getyx (stdscr, y, x);
 getmaxyx(stdscr, maxy, maxx);

 move (y, maxx - 13);
}

void move_to_left_border () {
 int x, y;

 getyx (stdscr, y, x);

 move (y, 0);
}

void progressbar (char *label, int p) {
 int maxx, maxy;

 getmaxyx(stdscr, maxy, maxx);

 move_to_left_border();

 addch (' ');

 if (progress != 100) {
  attron(COLOR_PAIR(attr_yellow));
  addstr (label);
  attroff(COLOR_PAIR(attr_yellow));
 } else {
  attron(COLOR_PAIR(attr_green));
  addstr (label);
  attroff(COLOR_PAIR(attr_green));
 }

 int offset = strlen (label) + 7;
 int numhashes = (progress * (maxx - offset) / 100);
 int totalspace = (maxx - offset);
 int i = 0;

 addstr (" [ ");

 for (; i < numhashes; i++) {
  addch ('#');
 }

 for (; i < totalspace; i++) {
  addch (' ');
 }

 addstr (" ]\n");

// usleep(100);
}

void update() {
 erase();

 char buffer[BUFFERSIZE];

 if (strmatch (mode, mode_to)) {
  progressbar (mode, progress);
 } else {
  snprintf (buffer, BUFFERSIZE, "%s -> %s", mode, mode_to);
  progressbar (buffer, progress);
 }

 if (status_tree) {
  struct stree *st = streelinear_prepare (status_tree);

  while (st) {
   struct module_status *s = st->value;

   /* "[  working ]" */
   /* "[  failed  ]" */
   /* "[  enabled ]" */
   /* "[ diasbled ]" */

   char *name = st->key;
   if (s->name) name = s->name;

   if (s->status & status_working) {
    snprintf (buffer, BUFFERSIZE, " :: %s", name);
    addstr (buffer);

    move_to_right_border();
    addstr ("[  ");

    attron(COLOR_PAIR(attr_blue));
    addstr ("working");
    attroff(COLOR_PAIR(attr_blue));

    addstr (" ]\n");
   } else if (s->status & status_failed) {
    snprintf (buffer, BUFFERSIZE, " :: %s", name);
    addstr (buffer);

    move_to_right_border();
    addstr ("[  ");

    attron(COLOR_PAIR(attr_red));
    addstr ("failed");
    attroff(COLOR_PAIR(attr_red));

    addstr ("  ]\n");
   } else if (s->status & status_enabled) {
    snprintf (buffer, BUFFERSIZE, " :: %s", name);
    addstr (buffer);

    move_to_right_border();
    addstr ("[  ");

    attron(COLOR_PAIR(attr_green));
    addstr ("enabled");
    attroff(COLOR_PAIR(attr_green));

    addstr (" ]\n");
   } else if (s->status & status_disabled) {
    snprintf (buffer, BUFFERSIZE, " :: %s", name);
    addstr (buffer);

    move_to_right_border();
    addstr ("[ ");

    attron(COLOR_PAIR(attr_yellow));
    addstr ("disabled");
    attroff(COLOR_PAIR(attr_yellow));

    addstr (" ]\n");
   } else {
    snprintf (buffer, BUFFERSIZE, " :: %s", name);
    addstr (buffer);

    move_to_right_border();
    addstr ("[ ");

    attron(COLOR_PAIR(attr_yellow));
    addstr ("idle");
    attroff(COLOR_PAIR(attr_yellow));

    addstr (" ]\n");
   }

   st = streenext (st);
  }
 }
 refresh();
}

void event_handler_mode_switching (struct einit_event *ev) {
 mode_to = estrdup(ev->string);

 update();
}

void event_handler_mode_switch_done (struct einit_event *ev) {
 mode = estrdup(ev->string);

 update();
}

void event_handler_update_module_status (struct einit_event *ev) {
 if (ev->rid) {
  set_module_status (ev->rid, ev->status);
 }

/* if (ev->string) {
  snprintf (buffer, BUFFERSIZE, " > %s: %s\n", ev->rid, ev->string);
  addstr (buffer);
 }*/

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

void event_handler_switch_progress (struct einit_event *ev) {
 progress = ev->integer;
}

int main(int argc, char **argv, char **env) {
 initscr();
 start_color();
 cbreak();
 noecho();

 init_pair(attr_red, COLOR_RED, COLOR_BLACK);
 init_pair(attr_blue, COLOR_BLUE, COLOR_BLACK);
 init_pair(attr_green, COLOR_GREEN, COLOR_BLACK);
 init_pair(attr_yellow, COLOR_YELLOW, COLOR_BLACK);

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
 event_listen (einit_feedback_switch_progress, event_handler_switch_progress);

 einit_event_loop();

 endwin ();

 einit_disconnect();

 return 0;
}
