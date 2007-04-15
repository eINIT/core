/*
 *  einit-network.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/10/2006.
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

#define _MODULE

#include <einit-modules/network.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <unistd.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"network/experimental", NULL};
char * requires[] = {"mount-critical", NULL};
const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Network Configuration",
 .rid       = "einit-network-experimental",
 .si        = {
  .provides = provides,
  .requires = requires,
  .after    = NULL,
  .before   = NULL
 }
};

struct network_control_block mncb = {
	.interfaces		= NULL,
	.add_network_interface	= add_network_interface
};

char *defaultinterfaces[] = { "sys", "proc", NULL };
pthread_mutex_t interfaces_mutex = PTHREAD_MUTEX_INITIALIZER;

void network_ipc_handler(struct einit_event *);
void network_update_handler(struct einit_event *);
void update ();

int configure (struct lmodule *this) {
 event_listen (EVENT_SUBSYSTEM_IPC, network_ipc_handler);
 event_listen (EVENT_SUBSYSTEM_NETWORK, network_update_handler);

 update ();
}

int cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_IPC, network_ipc_handler);
 event_ignore (EVENT_SUBSYSTEM_NETWORK, network_update_handler);
}

void update () {
 char **interfacessource = str2set (':', cfg_getstring("configuration-network-interfaces-source", NULL));
 void **functions = NULL;
 void (*f)(struct network_control_block *);
 uint32_t i = 0;

 if (!interfacessource) interfacessource = defaultinterfaces;

 functions = function_find ("find-network-interfaces", 1, interfacessource);
 if (functions && functions[0]) {
  for (i = 0; functions[i]; i++) {
   f = (void (*)(struct network_control_block *))functions[i];
   f (&mncb);
  }
  free (functions);
 }

 if (interfacessource != defaultinterfaces) free (interfacessource);
}

void network_ipc_handler(struct einit_event *event) {
 if (!event || !event->set) return;
 char **argv = (char **) event->set;
 uint32_t argc = setcount ((void **)argv);

 if ((argc >= 2) && !strcmp (argv[0], "list")) {
  if (!strcmp (argv[1], "network-interfaces")) {
   char buffer[BUFFERSIZE];
   struct interface_parametres *cval;
   struct stree *cur = mncb.interfaces;

   while (cur) {
    cval = cur->value;

    if (cval->name)
     snprintf (buffer, BUFFERSIZE, "%s (%s) [options=%x]\n", cur->key, cval->name, cval->options);
    else
     snprintf (buffer, BUFFERSIZE, "%s [options=%x]\n", cur->key, cval->options);
    fputs (buffer, (FILE *)event->para);

    cur = streenext (cur);
   }

   if (!event->flag) event->flag++;
  }
 }
}

void network_update_handler(struct einit_event *event) {
 if (event) {
  if (event->flag & EVENT_NETWORK_UPDATE_INTERFACES) update();
 }
}

void add_network_interface (char *id, char *name, uint32_t options) {
 struct interface_parametres ip = {
  .options = options
 };

 if (!id) return;
 ip.id = estrdup (id);
 ip.name = (name ? estrdup (name) : NULL);

 pthread_mutex_lock (&interfaces_mutex);

 mncb.interfaces = streeadd (mncb.interfaces, id, &ip, sizeof (struct interface_parametres), ip.name);

 pthread_mutex_unlock (&interfaces_mutex);
}

int enable (void *pa, struct einit_event *status) {
 update ();

 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 return STATUS_OK;
}
