/*
 *  parse-sh.c
 *  einit
 *
 *  Created by Magnus Deininger on 08/01/2006.
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
#include <unistd.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <einit-modules/parse-sh.h>

#include <einit-modules/ipc.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_parse_sh_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_parse_sh_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT Parser Library: SH",
 .rid       = "einit-parse-sh",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_parse_sh_configure
};

module_register(einit_parse_sh_self);

#endif

int parse_sh_f (const char *, void (*)(const char **, uint8_t, void *), void *);

int einit_parse_sh_cleanup (struct lmodule *irr) {
 function_unregister ("einit-parse-sh", 1, parse_sh_f);
 parse_sh_cleanup (irr);

 return 0;
}

// parse sh-style files and call back for each line
int parse_sh_f (const char *data, void (*callback)(const char **, uint8_t, void *), void *ud) {
 if (!data) return -1;

 char *ndp = emalloc(strlen(data)+1), *cdp = ndp, *sdp = cdp;
 const char *cur = data-1;
 enum einit_sh_parser_status stat = sh_parser_status_lw;
 char squote = 0, dquote = 0, lit = 0,
 **command = NULL;

 while (*(cur +1)) {
  cur++;

  if (stat == sh_parser_status_ignore_till_eol) {
   if (*cur == '\n')
    stat = sh_parser_status_lw;

   continue;
  }
//  putchar (*cur);

  if (lit) {
   lit = 0;
   *cdp = *cur;
   cdp++;
  } else switch (*cur) {
   case '#': stat = sh_parser_status_ignore_till_eol; break;
   case '\'': squote = !squote; break;
   case '\"': dquote = !dquote; break;
   case '\\': lit = 1; break;
   case '\n':
   case ';':
   case '&':
    if ((stat != sh_parser_status_lw) && (cdp != sdp)) {
     *cdp = 0;
     command = set_str_add (command, (void*)sdp);
     cdp++;
     sdp = cdp;
    }

    stat = sh_parser_status_lw;

    if (command) {
     if ((*cur) == '&')
      callback ((const char **)command, pa_new_context_fork, ud);
     else
      callback ((const char **)command, pa_new_context, ud);

     efree (command);
     command = NULL;
    }

    break;
   default:
    if (dquote || squote) {
     *cdp = *cur;
     cdp++;
    } else if (isspace(*cur)) {
     if ((stat != sh_parser_status_lw) && (cdp != sdp)) {
      *cdp = 0;
      command = set_str_add (command, (void*)sdp);
      cdp++;
      sdp = cdp;
     }
     stat = sh_parser_status_lw;
    } else {
     *cdp = *cur;
     cdp++;
     stat = sh_parser_status_read;
    }

    break;
  }
 }

/* commit last line */

 if ((stat != sh_parser_status_lw) && (cdp != sdp)) {
  *cdp = 0;
  command = set_str_add (command, (void*)sdp);
  cdp++;
  sdp = cdp;
 }

 stat = sh_parser_status_lw;

 if (command) {
  callback ((const char **)command, pa_new_context, ud);

  efree (command);
  command = NULL;
 }

 callback (NULL, pa_end_of_file, ud);
 efree (ndp);

 return 0;
}

/* passive module: no enable/disable */

int einit_parse_sh_configure (struct lmodule *irr) {
 module_init (irr);

 irr->cleanup = einit_parse_sh_cleanup;

 parse_sh_configure (irr);
 function_register ("einit-parse-sh", 1, parse_sh_f);

 return 0;
}
