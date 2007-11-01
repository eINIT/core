/*
 *  configuration-secondary-sh-style.c
 *  einit
 *
 *  Created by Magnus Deininger on 01/08/2006.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <einit-modules/parse-sh.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "Secondary Configuration Module: SH-Style Files",
 .rid       = "configuration-secondary-sh-style",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

void einit_event_handler (struct einit_event *);
void ipc_event_handler (struct einit_event *);

char **curvars = NULL,
     **files = NULL;
time_t *mtimes = NULL;

/* functions that module tend to need */
int configure (struct lmodule *irr) {
 parse_sh_configure (irr);

 event_listen (einit_event_subsystem_core, einit_event_handler);
 event_listen (einit_event_subsystem_ipc, ipc_event_handler);
}

int cleanup (struct lmodule *irr) {
 event_ignore (einit_event_subsystem_ipc, ipc_event_handler);
 event_ignore (einit_event_subsystem_core, einit_event_handler);

 parse_sh_cleanup (irr);
}

void sh_configuration_callback (char **data, uint8_t status) {
 char *n = NULL;
 if (data && data[0] && (n = strchr (data[0],'='))) {
  char *xn;
  *n = 0;
  n++;

  xn = apply_variables (n, curvars);

  curvars = (char **)setadd ((void **)curvars, (void *)data[0], SET_TYPE_STRING);
  curvars = (char **)setadd ((void **)curvars, (void *)xn, SET_TYPE_STRING);

  n--;
  *n = '=';
 }
}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == einit_core_update_configuration) {
  uint32_t x = 0;
  char *data = NULL;
  struct cfgnode *node = NULL;
  struct stat st;

  while (node = cfg_findnode ("configuration-secondary-file-sh", 0, node)) {
   if (node->idattr && node->arbattrs) {
    if (stat (node->idattr, &st)) {
#ifdef DEBUG
     fputs (" >> file not found/readable, skipping\n", stderr);
#endif
     continue;
    }

    if (files) {
     for (x = 0; files[x]; x++) {
      if (!strcmp (files[x], node->idattr))
       break;
     }
    }

    if (!files || !files[x]) {
#ifdef DEBUG
     fputs (" >> file not recorded, adding\n", stderr);
#endif

     files = (char **)setadd ((void **)files, (void *)node->idattr, SET_TYPE_STRING);
     mtimes = (time_t *)setadd ((void **)mtimes, (void *)st.st_mtime, SET_NOALLOC);
    } else {
     if (mtimes[x] < st.st_mtime) {
#ifdef DEBUG
      fputs (" >> file updated\n", stderr);
#endif
      mtimes[x] = st.st_mtime;
     } else {
#ifdef DEBUG
      fputs (" >> file not updated since last parse, skipping\n", stderr);
#endif
      continue;
     }
    }

    curvars = NULL;

    data = readfile (node->idattr);

    if (data) {
     parse_sh (data, sh_configuration_callback);

     if (curvars) {
      uint32_t y = 0, z = 0;

      for (y = 0; node->arbattrs[y]; y+=2) {
       if (!strcmp (node->arbattrs[y], "id")) continue;

       char **nk = NULL, **nn = NULL, **nt = str2set(',', node->arbattrs[y+1]);
       int32_t sindex = -1, iindex = -1, bindex = -1;

       for (x = 0; nt[x]; x++) {
        char **xt = str2set (':', nt[x]);

        if (xt[0] && xt[1]) {
         nk = (char **)setadd ((void **)nk, (void *)xt[0], SET_TYPE_STRING);
         nn = (char **)setadd ((void **)nn, (void *)xt[1], SET_TYPE_STRING);
        }
       }

       if (nk && nn) {
        struct cfgnode newnode;
        char **arbattrs = NULL;

        memset (&newnode, 0, sizeof(struct cfgnode));

        for (x = 0; curvars[x] && curvars[x+1]; x+=2) {
         for (z = 0; nk[z]; z++) {
          if (!strcmp(curvars[x], nk[z])) {
           arbattrs = (char **)setadd ((void **)arbattrs, (void *)nn[z], SET_TYPE_STRING);
           arbattrs = (char **)setadd ((void **)arbattrs, (void *)curvars[x+1], SET_TYPE_STRING);

           if (!strcmp(nn[z], "s")) sindex = z*2;
           else if (!strcmp(nn[z], "i")) iindex = z*2;
           else if (!strcmp(nn[z], "b")) bindex = z*2;

           continue;
          }
         }
        }

        if (arbattrs) {
#ifdef DEBUG
         fprintf (stderr, "configuration-secondary-sh: %s: %s, %i %i %i\n", node->arbattrs[y], set2str (' ', arbattrs), sindex, iindex, bindex);
#endif

         newnode.id       = estrdup(node->arbattrs[y]);
         newnode.nodetype = EI_NODETYPE_CONFIG;
         newnode.arbattrs = arbattrs;

         if (sindex != -1) newnode.svalue = arbattrs[z];
         if (iindex != -1) newnode.value = parse_integer(arbattrs[z]);
         if (bindex != -1) newnode.flag = parse_boolean(arbattrs[z]);

         cfg_addnode (&newnode);
        }
       }
      }

      free (curvars);
      curvars = NULL;
     }

     free (data);
    }

   }
  }
 }
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-secondary-file-sh", NULL)) {
   fputs ("NOTICE: No configuration variables at \"configuration-secondary-file-sh\":\n  Nothing to import. (not a problem)\n", (FILE *)ev->para);
   ev->task++;
  }

  ev->flag = 1;
 }
}
