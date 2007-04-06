/*
 *  network.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/04/2006.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <expat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/event.h>
#include <dirent.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>

#define ECXE_MASTERTAG 0x00000001
#define IF_OK          0x1

int _network_scanmodules (struct lmodule *);
int _network_interface_enable (void *, struct einit_event *);
int _network_interface_disable (void *, struct einit_event *);
int _network_interface_custom (void *, char *, struct einit_event *);
int _network_cleanup (struct lmodule *);
int _network_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)
const struct smodule _network_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_LOADER,
 .options   = 0,
 .name      = "Network Interface Configuration",
 .rid       = "network",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _network_configure
};

module_register(_network_self);

#endif

#if 0
void network_einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_UPDATE_CONFIGURATION) {
  if (configuration_network_parse_and_add_nodes()) {
   ev->chain_type = EVE_CONFIGURATION_UPDATE;
  }
 }
}
#endif

int _network_scanmodules (struct lmodule *mainlist) {
 struct stree *network_nodes = cfg_prefix("configuration-network-interfaces-");

 if (network_nodes) {
  struct stree *cur = network_nodes;

  while (cur) {
   if (cur->value) {
    struct cfgnode *nn = cur->value;

    if (nn->arbattrs) {
     char *interfacename = cur->key+33; /* 33 = strlen("configuration-network-interfaces-") */
     uint32_t i = 0;
     struct smodule newmodule;
     char tmp[BUFFERSIZE];

     memset (&newmodule, 0, sizeof (struct smodule));
     esprintf (tmp, BUFFERSIZE, "net-%s", interfacename);

//     newmodule.rid = estrdup (tmp);

     for (; nn->arbattrs[i]; i+=2) {
      if (strmatch (nn->arbattrs[i], "ip")) {
      } else if (strmatch(nn->arbattrs[i], "control")) {
      } else if (strmatch(nn->arbattrs[i], "kernel-module")) {
      }
     }
    }
   }

   cur = streenext (cur);
  }

  streefree (network_nodes);
 }

 return 0;
}

int _network_interface_enable (void *p, struct einit_event *status) {
}

int _network_interface_disable (void *p, struct einit_event *status) {
}

int _network_interface_custom (void *p, char *action, struct einit_event *status) {
}

int _network_cleanup (struct lmodule *this) {
 return 0;
}

int _network_cleanup_after_module (struct lmodule *this) {
 return 0;
}

int _network_configure (struct lmodule *this) {
 module_init(this);

 thismodule->cleanup = _network_cleanup;
 thismodule->scanmodules = _network_scanmodules;

#if 0
 event_listen (EVENT_SUBSYSTEM_EINIT, network_einit_event_handler);
#endif

 return 0;
}
