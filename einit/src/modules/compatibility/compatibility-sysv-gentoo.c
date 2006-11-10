/*
 *  compatibility-sysv-gentoo.c
 *  einit
 *
 *  Created by Magnus Deininger on 10/11/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
#include <errno.h>
#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

#define SH_PARSER_STATUS_LW              0
#define SH_PARSER_STATUS_READ            1
#define SH_PARSER_STATUS_IGNORE_TILL_EOL 2

#define PA_END_OF_FILE                   0x01
#define PA_NEW_CONTEXT                   0x02


const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= 0,
	.options	= 0,
	.name		= "SysV-Gentoo Compatibility Module",
	.rid		= "compatibility-sysv-gentoo",
	.provides	= NULL,
	.requires	= NULL,
	.notwith	= NULL
};

// parse sh-style files and call back for each line
int parse_sh (char *data, void (*callback)(char **, uint8_t)) {
 if (!data) return -1;

 char *ndp = emalloc(strlen(data)), *cdp = ndp, *sdp = cdp,
      *cur = data-1,
      stat = SH_PARSER_STATUS_LW, squote = 0, dquote = 0, lit = 0,
      **command = NULL;

 while (*(cur +1)) {
  cur++;

  if (stat == SH_PARSER_STATUS_IGNORE_TILL_EOL) {
   if (*cur == '\n')
    stat = SH_PARSER_STATUS_LW;

   continue;
  }
//  putchar (*cur);

  if (lit) {
   lit = 0;
   *cdp = *cur;
   cdp++;
  } else switch (*cur) {
   case '#': stat = SH_PARSER_STATUS_IGNORE_TILL_EOL; break;
   case '\'': squote = !squote; break;
   case '\"': dquote = !dquote; break;
   case '\\': lit = 1; break;
   case '\n':
    if ((stat != SH_PARSER_STATUS_LW) && (cdp != sdp)) {
     *cdp = 0;
     command = (char**)setadd ((void**)command, (void*)sdp, SET_NOALLOC);
      cdp++;
      sdp = cdp;
    }

    stat = SH_PARSER_STATUS_LW;

    if (command) {
     callback (command, PA_NEW_CONTEXT);

     free (command);
     command = NULL;
    }

    break;
   default:
    if (dquote || squote) {
     *cdp = *cur;
     cdp++;
    } else if (isspace(*cur)) {
    if ((stat != SH_PARSER_STATUS_LW) && (cdp != sdp)) {
      *cdp = 0;
      command = (char**)setadd ((void**)command, (void*)sdp, SET_NOALLOC);
      cdp++;
      sdp = cdp;
     }
     stat = SH_PARSER_STATUS_LW;
    } else {
     *cdp = *cur;
     cdp++;
     stat = SH_PARSER_STATUS_READ;
    }

    break;
  }
 }

 callback (NULL, PA_END_OF_FILE);
 free (ndp);

 return 0;
}

void sh_add_environ_callback (char **data, uint8_t status) {
 char *x, *y;

 if (status == PA_NEW_CONTEXT) {
  if (data && (x = data[0])) {
   if ((strcmp (data[0], "export") || (x = data[1])) && (y = strchr (x, '='))) {
// if we get here, we got ourselves a variable definition
    struct cfgnode nnode;
    memset (&nnode, 0, sizeof(struct cfgnode));
    char *narb[4] = { "id", x, "s", (y+1) }, *yt = NULL;

    *y = 0; y++;

// exception for the PATH and ROOTPATH variable (gentoo usually mangles that around in /etc/profile)
    if (!strcmp (x, "PATH")) {
     return;
    } else if (!strcmp (x, "ROOTPATH")) {
     x = narb[1] = "PATH";
     yt = emalloc (strlen (y) + 30);
     *yt = 0;
     strcat (yt, "/sbin:/bin:/usr/sbin:/usr/bin");
     strcat (yt, y);
     narb[3] = yt;
    }

    nnode.id = estrdup ("configuration-environment-global");
    nnode.arbattrs = (char **)setdup ((void **)&narb, SET_TYPE_STRING);
    nnode.svalue = nnode.arbattrs[3];
    nnode.source = "compatibility-sysv-gentoo";
    nnode.source_file = "/etc/profile.env";

    cfg_addnode (&nnode);

    if (yt) {
     free (yt);
     yt = NULL;
    }
//    puts (x);
//    puts (y);
   }
  }
 }
}

void einit_event_handler (struct einit_event *ev) {
 if ((ev->type == EVE_UPDATE_CONFIGURATION) && ev->string) {
  char *cs = cfg_getstring("configuration-compatibility-sysv-distribution", NULL);
  if (cs && !strcmp("auto", cs) || !strcmp("gentoo", cs)) {
   struct cfgnode *node = cfg_getnode ("configuration-compatibility-sysv-distribution-gentoo-parse-env.d", NULL);
   if (node && node->flag) {
    char *data = readfile ("/etc/profile.env");

    if (data) {
//     puts ("compatibility-sysv-gentoo: updating configuration with env.d");
     parse_sh (data, sh_add_environ_callback);

     free (data);
    }
   }
  }
 }
}

int configure (struct lmodule *irr) {
 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
}

int cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
}

// no enable/disable functions: this is a passive module
