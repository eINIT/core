/*
 *  configuration-network.c
 *  einit
 *
 *  Created by Magnus Deininger on 03/04/2006.
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

int configuration_network_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule configuration_network_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Configuration Helper (Network)",
 .rid       = "configuration-network",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = configuration_network_configure
};

module_register(configuration_network_self);

#endif

int configuration_network_cleanup (struct lmodule *this) {
 return 0;
}

#if 0
void add_module_through_template (cahr *id, char *template, char *interface, char *attr) {
 struct cfgnode newnode;

// net module
 char tmp[BUFFERSIZE];
 memset (&newnode, 0, sizeof(struct cfgnode));

 newnode.id = estrdup ("services-virtual-module-shell");
 newnode.source = self->rid;
 newnode.nodetype = EI_NODETYPE_CONFIG;

 esprintf (tmp, BUFFERSIZE, id, interface, attr);
 newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
 newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

 newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"based-on-template", SET_TYPE_STRING);
 newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"template-shell-net", SET_TYPE_STRING);

 newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"interface", SET_TYPE_STRING);
 newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)interfacename, SET_TYPE_STRING);

 cfg_addnode (&newnode);

}
#endif

char configuration_network_parse_configuration_and_add_nodes() {
 struct stree *network_nodes = cfg_prefix("configuration-network-interfaces-");

 if (network_nodes) {
  struct stree *cur = network_nodes;

  while (cur) {
   if (cur->value) {
    struct cfgnode *nn = cur->value;

    if (nn->arbattrs) {
     char *interfacename = cur->key+33; /* 33 = strlen("configuration-network-interfaces-") */
     uint32_t i = 0;

//     add_module_through_template ("shell-net-%s", "template-shell-net", interfacename, nn->arbattrs[i+1]);

     struct cfgnode newnode;

// net module
     char tmp[BUFFERSIZE];
     memset (&newnode, 0, sizeof(struct cfgnode));

     newnode.id = estrdup ("services-virtual-module-shell");
     newnode.source = self->rid;
     newnode.nodetype = EI_NODETYPE_CONFIG;

     esprintf (tmp, BUFFERSIZE, "shell-net-%s", interfacename, nn->arbattrs[i+1]);
     newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
     newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

     newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"based-on-template", SET_TYPE_STRING);
     newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"template-shell-net", SET_TYPE_STRING);

     newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"interface", SET_TYPE_STRING);
     newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)interfacename, SET_TYPE_STRING);

     cfg_addnode (&newnode);

     for (; nn->arbattrs[i]; i+=2) {
      if (strmatch(nn->arbattrs[i], "ip")) {
// ip module
       char tmp[BUFFERSIZE];
       memset (&newnode, 0, sizeof(struct cfgnode));

       newnode.id = estrdup ("services-virtual-module-shell");
       newnode.source = self->rid;
       newnode.nodetype = EI_NODETYPE_CONFIG;

       esprintf (tmp, BUFFERSIZE, "shell-ip-%s-%s", interfacename, nn->arbattrs[i+1]);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

       esprintf (tmp, BUFFERSIZE, "template-shell-ip-%s", nn->arbattrs[i+1]);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"based-on-template", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"interface", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)interfacename, SET_TYPE_STRING);

       cfg_addnode (&newnode);

// kernel module
       memset (&newnode, 0, sizeof(struct cfgnode));

       newnode.id = estrdup ("services-virtual-module-shell");
       newnode.source = self->rid;
       newnode.nodetype = EI_NODETYPE_CONFIG;

       esprintf (tmp, BUFFERSIZE, "shell-kern-%s", interfacename);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"based-on-template", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"template-shell-kern-module-loader", SET_TYPE_STRING);

       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"system", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)interfacename, SET_TYPE_STRING);

       cfg_addnode (&newnode);
      } else if (strmatch(nn->arbattrs[i], "control")) {
// control module (wpa-supplicant and the likes)
       char tmp[BUFFERSIZE];
       memset (&newnode, 0, sizeof(struct cfgnode));

       newnode.id = estrdup ("services-virtual-module-shell");
       newnode.source = self->rid;
       newnode.nodetype = EI_NODETYPE_CONFIG;

       esprintf (tmp, BUFFERSIZE, "shell-interface-%s-%s", interfacename, nn->arbattrs[i+1]);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

       esprintf (tmp, BUFFERSIZE, "template-shell-ifctl-%s", nn->arbattrs[i+1]);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"based-on-template", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"interface", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)interfacename, SET_TYPE_STRING);

       cfg_addnode (&newnode);
      } else if (strmatch(nn->arbattrs[i], "kernel-module")) {
       char tmp[BUFFERSIZE];
       memset (&newnode, 0, sizeof(struct cfgnode));

       esprintf (tmp, BUFFERSIZE, "configuration-kernel-modules-%s", interfacename);
       newnode.id = estrdup (tmp);
       newnode.source = self->rid;
       newnode.nodetype = EI_NODETYPE_CONFIG;

       esprintf (tmp, BUFFERSIZE, "kernel-module-%s", interfacename);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"id", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)tmp, SET_TYPE_STRING);

       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"s", SET_TYPE_STRING);
       newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)nn->arbattrs[i+1], SET_TYPE_STRING);

       newnode.svalue = newnode.arbattrs[3];

       cfg_addnode (&newnode);
      }
     }

     cur = streenext(cur);
    }
   }
  }

  streefree (network_nodes);
 }

 return 0;
}

void configuration_network_einit_event_handler (struct einit_event *ev) {
 if (ev->type == einit_core_update_configuration) {
  if (configuration_network_parse_configuration_and_add_nodes()) {
   ev->chain_type = einit_core_configuration_update;
  }
 }
}

int configuration_network_configure (struct lmodule *this) {
 module_init(this);

 thismodule->cleanup = configuration_network_cleanup;

 event_listen (einit_event_subsystem_core, configuration_network_einit_event_handler);

 configuration_network_parse_configuration_and_add_nodes();

 return 0;
}
