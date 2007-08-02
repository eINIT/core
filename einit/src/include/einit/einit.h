/*
 *  einit.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 24/07/2007.
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

#include <einit/module.h>
#include <einit/tree.h>

#ifndef EINIT_EINIT_H
#define EINIT_EINIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: someone document these structs and enums! */

struct einit_module {
 char *id;
 char *name;
 enum einit_module_status status;

 char **provides;
 char **requires;
 char **after;
 char **before;

 char **functions;
};

enum einit_service_status {
 service_idle     = 0x0000,
 service_provided = 0x0001
};

struct einit_group {
 char **services;
 char *seq;
};

struct einit_service {
 char *name;
 enum einit_service_status status;
 char **used_in_mode;

 struct einit_group *group;
 struct stree *modules;
};

struct einit_xml_tree_node {
 struct stree *parent;
 struct stree *elements;
 struct stree *attributes;
};

struct einit_mode_summary {
 char *id;
 char **base;
 char **services;
 char **critical;
 char **disable;
};

struct einit_remote_event {
 enum einit_event_code type;       /*!< the event or subsystem to watch */

 union {
/*! these struct elements are for use with non-IPC events */
  struct {
   char *string;                 /*!< a string */
   int32_t integer,              /*!< generic integer */
           status,               /*!< generic integer */
           task;                 /*!< generic integer */
   unsigned char flag;           /*!< flags */

   char **stringset;             /*!< a (string-)set that should make sense in combination with the event type */
  };

/*! these struct elements are for use with IPC events */
  struct {
   char **argv;
   char *command;
   enum einit_ipc_options ipc_options;
   int argc;
  };
 };

 uint32_t seqid;
 time_t timestamp;
};

/* TODO: ... and these functions... */

char *einit_ipc_request(const char *);
char *einit_ipc_request_xml(const char *);
char *einit_ipc(const char *);
char *einit_ipc_safe(const char *);
char einit_connect();
char einit_disconnect();
void einit_receive_events();

struct stree *einit_get_all_modules ();
struct einit_module *einit_get_module_status (char *);

struct stree *xml2stree (char *);

struct stree *einit_get_all_services ();
struct einit_service *einit_get_service_status (char *);

void xmlstree_free(struct stree *);
void einit_module_free (struct einit_module *);
void einit_service_free (struct einit_service *);
void modulestree_free(struct stree *);
void servicestree_free(struct stree *);

void einit_power_down ();
void einit_power_reset ();

void einit_service_call (const char *, const char *);
void einit_service_enable (const char *);
void einit_service_disable (const char *);

void einit_module_id_call (const char *, const char *);
void einit_module_id_enable (const char *);
void einit_module_id_disable (const char *);

void einit_switch_mode (const char *);
void einit_reload_configuration ();

struct stree *einit_get_all_modes();
void modestree_free(struct stree *);

/* events... */
void einit_remote_event_listen (enum einit_event_subsystems, void (*)(struct einit_remote_event *));
void einit_remote_event_ignore (enum einit_event_subsystems, void (*)(struct einit_remote_event *));
void einit_remote_event_emit (struct einit_remote_event *ev, enum einit_event_emit_flags flags);
struct einit_remote_event *einit_remote_event_create (uint32_t type);
void einit_remote_event_destroy (struct einit_remote_event *);

#ifdef __cplusplus
}
#endif

#endif
