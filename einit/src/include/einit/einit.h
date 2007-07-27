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
};

enum einit_service_status {
 service_idle     = 0x0000,
 service_provided = 0x0001
};

struct einit_service {
 char *name;
 enum einit_service_status status;
};

struct einit_xml_tree_node {
 struct stree *parent;
 struct stree *elements;
 struct stree *attributes;
};

/* TODO: ... and these functions... */

char *einit_ipc_request(char *);
char *einit_ipc_request_xml(char *);
char *einit_ipc(char *);
char einit_connect();

struct stree *einit_get_all_modules ();

struct stree *xml2stree (char *);

void xmlstree_free(struct stree *);
void modulestree_free(struct stree *);

void einit_power_down ();
void einit_power_reset ();

void einit_service_call (char *, char *);
void einit_service_enable (char *);
void einit_service_disable (char *);

#ifdef __cplusplus
}
#endif

#endif
