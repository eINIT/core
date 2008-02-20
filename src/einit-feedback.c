/*
 *  einit-feedback.c
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
#include <pthread.h>

struct module_status {
 enum einit_module_status status;
 char *name;
 char feedback_job;
 int progress;
};

#define attr_red 1
#define attr_blue 2
#define attr_green 3
#define attr_yellow 4
#define attr_white 5

struct stree *status_tree = NULL;

struct textbuffer_entry {
 char *rid;
 char *message;
};

enum control_type {
 control_automatic,
 control_manual
};

struct textbuffer_entry **textbuffer = NULL;

int progress = 0;
int starting_bufferitem = 0;
char *mode = "(none)";
char *mode_to = "(none)";

char **broken = NULL;
char **unresolved = NULL;

enum control_type control_mode = control_automatic;

void set_module_status (char *name, enum einit_module_status status) {
 struct stree *e;
 if (!status_tree || !(e = streefind (status_tree, name, tree_find_first))) {
  struct module_status s;

  s.status = status;
  s.name = einit_module_get_name(name);
  s.progress = 0;

  char **t = einit_module_get_options(name);
  if (t) {
   s.feedback_job = inset ((const void **)t, "job-feedback", SET_TYPE_STRING);
   efree (t);
  }

  status_tree = streeadd (status_tree, name, &s, sizeof (s), NULL);
 } else {
  struct module_status *s = e->value;

  if (s->status & status_failed) {
    s->status = status | ((status & status_enabled) ? status_failed : 0);
  } else
   s->status = status;
 }
}

void set_module_progress (char *name, int p) {
 struct stree *e;
 if (!status_tree || !(e = streefind (status_tree, name, tree_find_first))) {
  struct module_status s;

  s.status = status_idle;
  s.name = einit_module_get_name(name);

  char **t = einit_module_get_options(name);
  s.feedback_job = inset ((const void **)t, "job-feedback", SET_TYPE_STRING);

  s.progress = p;

  status_tree = streeadd (status_tree, name, &s, sizeof (s), NULL);
 } else {
  struct module_status *s = e->value;

  s->progress = p;
 }
}

void add_text_buffer_entry (char *rid, char *message) {
 struct textbuffer_entry ne = {
  .rid = rid,
  .message = message
 };

 if (!message[0]) return;

 if (textbuffer) {
  if (strmatch (message, "status")) {
   int i = 0;
   for (; textbuffer[i]; i++) {
    if (strmatch (rid, textbuffer[i]->rid)) {
     return;
    }
   }
  } else {
   int i = 0;
   for (; textbuffer[i]; i++) {
    if (strmatch (rid, textbuffer[i]->rid)) {
     if (strmatch (message, textbuffer[i]->message)) return;
     if (strmatch (textbuffer[i]->message, "status")) {
      textbuffer[i]->message = message;
      return;
     }
    }
   }
  }
 }

 textbuffer = (struct textbuffer_entry **)set_fix_add ((void **)textbuffer, &ne, sizeof (ne));
}

void move_to_right_border (int offset) {
 int x, y, maxx, maxy;

 getyx (stdscr, y, x);
 getmaxyx(stdscr, maxy, maxx);

 move (y, maxx - offset);
}

void move_to_left_border () {
 int x, y;

 getyx (stdscr, y, x);

 move (y, 0);
}

char is_last_line () {
 int x, y, maxx, maxy;

 getyx (stdscr, y, x);
 getmaxyx(stdscr, maxy, maxx);

 return y >= (maxy -1);
}

void progressbar (char *label, int p) {
 int maxx, maxy;

 getmaxyx(stdscr, maxy, maxx);

 move_to_left_border();

// addch (' ');

 if (p != 100) {
  attron(COLOR_PAIR(attr_yellow));
  addstr (label);
  attroff(COLOR_PAIR(attr_yellow));
 } else {
  attron(COLOR_PAIR(attr_green));
  addstr (label);
  attroff(COLOR_PAIR(attr_green));
 }

 int offset = strlen (label) + 5;
 int numhashes = (p * (maxx - offset) / 100);
 int totalspace = (maxx - offset);
 int i = 0;

 addstr (" [ ");

 for (; i < numhashes; i++) {
  addch ('#');
 }

 for (; i < totalspace; i++) {
  addch (' ');
 }

 addstr (" ]");

// usleep(100);
}

void display_name(char *rid) {
 char buffer[BUFFERSIZE];

 if (status_tree) {
  struct stree *st = streefind (status_tree, rid, tree_find_first);

  if (st) {
   struct module_status *s = st->value;

   char *name = st->key;
   if (s->name) name = s->name;

   addch (' ');
   attron(A_BOLD);
   addstr (name);
   attroff(A_BOLD);
   addch ('\n');
  }
 }
}

void display_status(char *rid) {
 char buffer[BUFFERSIZE];

 if (status_tree) {
  struct stree *st = streefind (status_tree, rid, tree_find_first);

  if (st) {
   struct module_status *s = st->value;

   /* "[  working ]" */
   /* "[  failed  ]" */
   /* "[  enabled ]" */
   /* "[    OK    ]" */
   /* "[ diasbled ]" */

   char *name = st->key;
   if (s->name) name = s->name;

   if ((s->status & status_working) && s->progress) {
    progressbar (name, s->progress);
    return;
   }

   addch (' ');
   attron(A_BOLD);
   addstr (name);
   attroff(A_BOLD);

   if (!s->feedback_job) {
    if (s->status & status_enabled) {
     move_to_right_border(16);
     attron (A_DIM);
     addstr ("enabled :");
     attroff (A_DIM);
    } else if (s->status & status_disabled) {
     move_to_right_border(17);
     attron (A_DIM);
     addstr ("disabled :");
     attroff (A_DIM);
    }
   }

   move_to_right_border(6);

   attron(A_BOLD);

   attron(COLOR_PAIR(attr_blue));
   addstr ("[ ");
   attroff(COLOR_PAIR(attr_blue));

   if (s->status & status_working) {
    attron(COLOR_PAIR(attr_blue));
    addstr ("..");
    attroff(COLOR_PAIR(attr_blue));
   } else if (s->status & status_failed) {
    attron(COLOR_PAIR(attr_red));
    addstr ("!!");
    attroff(COLOR_PAIR(attr_red));
   } else if (s->status & status_ok) {
    attron(COLOR_PAIR(attr_green));
    addstr ("OK");
    attroff(COLOR_PAIR(attr_green));
   }

   attron(COLOR_PAIR(attr_blue));
   addstr (" ]");
   attroff(COLOR_PAIR(attr_blue));

   attroff(A_BOLD);

/*
   if (s->status & status_working) {
    move_to_right_border(12);
    addstr ("[  ");

    attron(COLOR_PAIR(attr_blue));
    addstr ("working");
    attroff(COLOR_PAIR(attr_blue));

    addstr (" ]");
   } else if (s->status & status_failed) {
    move_to_right_border();
    addstr ("[  ");

    attron(COLOR_PAIR(attr_red));
    addstr ("failed");
    attroff(COLOR_PAIR(attr_red));

    addstr ("  ]");
   } else if (s->status & status_enabled) {
    if (s->feedback_job) {
     move_to_right_border();
     addstr ("[    ");

     attron(COLOR_PAIR(attr_green));
     addstr ("OK");
     attroff(COLOR_PAIR(attr_green));

     addstr ("    ]");
    } else {
     move_to_right_border();
     addstr ("[  ");

     attron(COLOR_PAIR(attr_green));
     addstr ("enabled");
     attroff(COLOR_PAIR(attr_green));

     addstr (" ]");
    }
   } else if (s->status & status_disabled) {
    if (s->feedback_job) {
     move_to_right_border();
     addstr ("[    ");

     attron(COLOR_PAIR(attr_yellow));
     addstr ("OK");
     attroff(COLOR_PAIR(attr_yellow));

     addstr ("    ]");
    } else {
     move_to_right_border();
     addstr ("[ ");

     attron(COLOR_PAIR(attr_yellow));
     addstr ("disabled");
     attroff(COLOR_PAIR(attr_yellow));

     addstr (" ]");
    }
   } else {
    if (s->feedback_job) {
     move_to_right_border();
     addstr ("[    ");

     attron(COLOR_PAIR(attr_yellow));
     addstr ("OK");
     attroff(COLOR_PAIR(attr_yellow));

     addstr ("    ]");
    } else {
     move_to_right_border();
     addstr ("[ ");

     attron(COLOR_PAIR(attr_yellow));
     addstr ("idle");
     attroff(COLOR_PAIR(attr_yellow));

     addstr (" ]");
    }
   }
*/
   st = streenext (st);
  }
 }
}

char display_status_working_or_error(char *rid) {
 if (status_tree) {
  struct stree *st = streefind (status_tree, rid, tree_find_first);

  if (st) {
   struct module_status *s = st->value;

   if (s->status & (status_working | status_failed)) {
    display_status(rid);
    return 1;
   }
  }
 }

 return 0;
}

pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;

void update_do() {
 retry:

 erase();

 char buffer[BUFFERSIZE];
 char **have_status = NULL;
 char *lastrid = NULL;

 if (strmatch (mode, mode_to)) {
  progressbar (mode, progress);
 } else {
  snprintf (buffer, BUFFERSIZE, "%s -> %s", mode, mode_to);
  progressbar (buffer, progress);
 }

 if (unresolved) {
  attron(COLOR_PAIR(attr_yellow));
  addstr (" ** UNRESOLVED SERVICES:");

  int i = 0;
  for (; unresolved[i]; i++) {
   addch (' ');
   addstr (unresolved[i]);
  }

  addch ('\n');
  attroff(COLOR_PAIR(attr_yellow));
 }

 if (broken) {
  attron(COLOR_PAIR(attr_red));
  addstr (" ** BROKEN SERVICES:");

  int i = 0;
  for (; unresolved[i]; i++) {
   addch (' ');
   addstr (unresolved[i]);
  }

  addch ('\n');
  attroff(COLOR_PAIR(attr_red));
 }

 if (textbuffer) {
  int i;

  if (control_mode == control_automatic) {
   for (i = 0; (i < starting_bufferitem) && textbuffer[i]; i++) {
    if (!have_status || !inset ((const void **)have_status, textbuffer[i]->rid, SET_TYPE_STRING)) {
     if (display_status_working_or_error (textbuffer[i]->rid)) {
      have_status = set_str_add_stable (have_status, textbuffer[i]->rid);

      lastrid = textbuffer[i]->rid;
     }
    }

    if (is_last_line()) {
     if (have_status)
      free (have_status);

     return;
    }
   }
  }

  for (i = starting_bufferitem; textbuffer[i]; i++) {
   if (strmatch(textbuffer[i]->message, "status")) {
    if (!have_status || !inset ((const void **)have_status, textbuffer[i]->rid, SET_TYPE_STRING)) {
     display_status (textbuffer[i]->rid);

     have_status = set_str_add_stable (have_status, textbuffer[i]->rid);

     lastrid = textbuffer[i]->rid;
    }
   } else {
    if (!lastrid || !strmatch (lastrid, textbuffer[i]->rid)) {
     if (!have_status || !inset ((const void **)have_status, textbuffer[i]->rid, SET_TYPE_STRING)) {
      display_status (textbuffer[i]->rid);

      have_status = set_str_add_stable (have_status, textbuffer[i]->rid);
     } else {
      display_name (textbuffer[i]->rid);
     }
    }

    addch (' ');
    addch (' ');
    attron(COLOR_PAIR(attr_yellow));
    addch ('*');
    attroff(COLOR_PAIR(attr_yellow));
    addch (' ');
    addstr (textbuffer[i]->message);
    addch ('\n');

    lastrid = textbuffer[i]->rid;
   }

   if (is_last_line()) {
    if (have_status)
     free (have_status);

    if (control_mode == control_automatic) {
     starting_bufferitem++;
     goto retry;
    }

    return;
   }
  }
 }

 if (have_status)
  free (have_status);

 refresh();
}

void update () {
 pthread_mutex_lock (&update_mutex);
 update_do();
 pthread_mutex_unlock (&update_mutex);
}

void event_handler_mode_switching (struct einit_event *ev) {
 mode_to = (char *)str_stabilise(ev->string);

 if (broken) {
  efree (broken);
  broken = NULL;
 }
 if (unresolved) {
  efree (unresolved);
  broken = NULL;
 }

 update();
}

void event_handler_mode_switch_done (struct einit_event *ev) {
 mode = (char *)str_stabilise(ev->string);

 update();
}

void event_handler_update_module_status (struct einit_event *ev) {
 if (ev->rid) {
  set_module_status (ev->rid, ev->status);
  add_text_buffer_entry (ev->rid, "status");
 }

 if (ev->string) {
  add_text_buffer_entry (ev->rid, ev->string);
 }

 if (ev->flag)
  set_module_progress (ev->rid, ev->flag);

 update();
}

void event_handler_update_service_enabled (struct einit_event *ev) {
/* char buffer[BUFFERSIZE];
 snprintf (buffer, BUFFERSIZE, "enabled: %s\n", ev->string);
 addstr (buffer);

 update();*/
}

void event_handler_update_service_disabled (struct einit_event *ev) {
/* char buffer[BUFFERSIZE];
 snprintf (buffer, BUFFERSIZE, "disabled: %s\n", ev->string);
 addstr (buffer);

 update();*/
}

void event_handler_switch_progress (struct einit_event *ev) {
 progress = ev->integer;
 update();
}

void event_handler_unresolved_services (struct einit_event *ev) {
 if (ev->stringset) {
  int i = 0;
  for (; ev->stringset[i]; i++) {
   if (!broken || !inset ((const void **)unresolved, ev->stringset[i], SET_TYPE_STRING))
    unresolved = set_str_add_stable (unresolved, ev->stringset[i]);
  }
 }

 update();
}

void event_handler_broken_services (struct einit_event *ev) {
 if (ev->stringset) {
  int i = 0;
  for (; ev->stringset[i]; i++) {
   if (!broken || !inset ((const void **)broken, ev->stringset[i], SET_TYPE_STRING))
    broken = set_str_add_stable (broken, ev->stringset[i]);
  }
 }

 update();
}

void *input_thread (void *ignored) {
 while (1) {
  switch (getch ()) {
   case 'a':
    control_mode = control_automatic;
    update();
    break;

   case 'k':
   case 'p':
   case KEY_UP:
    control_mode = control_manual;
    if (starting_bufferitem > 0)
     starting_bufferitem--;
    update();
    break;

   case 'j':
   case 'n':
   case KEY_DOWN:
    control_mode = control_manual;
    if (textbuffer && textbuffer[starting_bufferitem] && textbuffer[starting_bufferitem+1])
     starting_bufferitem++;
    update();
    break;

   case 'q':
    endwin ();
    einit_disconnect();
    exit(EXIT_SUCCESS);
  }
 }
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
 init_pair(attr_white, COLOR_WHITE, COLOR_BLACK);

 nonl();
 intrflush(stdscr, FALSE);
 keypad(stdscr, TRUE);

 if (!einit_connect(&argc, argv)) {
//  perror ("Could not connect to eINIT");
  sleep (1);
  if (!einit_connect(&argc, argv)) {
//   perror ("Could not connect to eINIT, giving up");
   exit (EXIT_FAILURE);
  }
 }

 ethread_spawn_detached (input_thread, NULL);

 event_listen (einit_core_mode_switching, event_handler_mode_switching);
 event_listen (einit_core_mode_switch_done, event_handler_mode_switch_done);
 event_listen (einit_feedback_module_status, event_handler_update_module_status);
 event_listen (einit_core_service_enabled, event_handler_update_service_enabled);
 event_listen (einit_core_service_disabled, event_handler_update_service_disabled);
 event_listen (einit_feedback_switch_progress, event_handler_switch_progress);

 event_listen (einit_feedback_broken_services, event_handler_broken_services);
 event_listen (einit_feedback_unresolved_services, event_handler_unresolved_services);

 einit_event_loop();

 endwin ();

 einit_disconnect();

 return 0;
}
