/*
 *  einit-parse-sh.c
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

#define _MODULE

#include <stdio.h>
#include <unistd.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/scheduler.h>
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

int _einit_parse_sh_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)
const struct smodule _einit_parse_sh_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "eINIT Parser Library: SH",
 .rid       = "einit-parse-sh",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _einit_parse_sh_configure
};

module_register(_einit_parse_sh_self);

#endif

int __parse_sh (char *, void (*)(char **, uint8_t));

int _einit_parse_sh_cleanup (struct lmodule *irr) {
 function_unregister ("einit-parse-sh", 1, __parse_sh);
 parse_sh_cleanup (irr);

 return 0;
}

// parse sh-style files and call back for each line
int __parse_sh (char *data, void (*callback)(char **, uint8_t)) {
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

/* commit last line */

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

 callback (NULL, PA_END_OF_FILE);
 free (ndp);

 return 0;
}

/* passive module: no enable/disable */

int _einit_parse_sh_configure (struct lmodule *irr) {
 module_init (irr);

 irr->cleanup = _einit_parse_sh_cleanup;

 parse_sh_configure (irr);
 function_register ("einit-parse-sh", 1, __parse_sh);

 return 0;
}
