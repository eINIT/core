/*
 *  bitch.c
 *  einit
 *
 *  Created by Magnus Deininger on 14/02/2006.
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

#include <einit/bitch.h>
#include <einit/event.h>
#include <einit/config.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#define BITCH2_ERROR_TEMPLATE "%s: %s (System Error #%i [%s])\n"
unsigned char mortality[BITCH_SAUCES] = { 1, 1, 1, 1, 1, 1, 1, 1 };

int bitch (unsigned int opt) {
 if (opt & BTCH_ERRNO) {
  if (errno) {
   fputs (strerror (errno), stderr);
   fputs ("\n", stderr);
   errno = 0;
  }
 }
 if (opt & BTCH_DL) {
  char *dlerr = dlerror();
  if (dlerr)
   puts (dlerr);
 }
 return -1;
}

int bitch2 (unsigned char sauce, const char *location, int error, const char *reason) {
 char *llocation      = location ? location : "unknown";
 char *lreason        = reason ? reason : "unknown";
 int lerror         = error ? error : errno;
 unsigned char lsauce = (sauce < BITCH_SAUCES) ? sauce : BITCH_BAD_SAUCE;

 switch (mortality[lsauce]) {
  case 0: // 0: ignore the problem
   return error;
  case 1: // 1: print error or stderr
   if ((fprintf(stderr, BITCH2_ERROR_TEMPLATE, llocation, lreason, lerror, strerror(lerror)) < 0))
    perror ("bitch2: writing error message");
   return error;
  case 255: // 255: just die
   if ((fprintf(stderr, BITCH2_ERROR_TEMPLATE, llocation, lreason, lerror, strerror(lerror)) < 0))
    perror ("bitch2: writing error message");

   exit(error);
 }

 return error;
}

void bitchin_einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_CONFIGURATION_UPDATE) {
  struct cfgnode *node;
  if (node = cfg_getnode ("core-mortality-bad-malloc", NULL))
   mortality[BITCH_EMALLOC] = node->value;

  if (node = cfg_getnode ("core-mortality-bad-stdio", NULL))
   mortality[BITCH_STDIO] = node->value;

  if (node = cfg_getnode ("core-mortality-bad-regex", NULL))
   mortality[BITCH_REGEX] = node->value;

  if (node = cfg_getnode ("core-mortality-bad-expat", NULL))
   mortality[BITCH_EXPAT] = node->value;

  if (node = cfg_getnode ("core-mortality-bad-dl", NULL))
   mortality[BITCH_DL] = node->value;

  if (node = cfg_getnode ("core-mortality-bad-lookup", NULL))
   mortality[BITCH_LOOKUP] = node->value;

  if (node = cfg_getnode ("core-mortality-bad-pthreads", NULL))
   mortality[BITCH_EPTHREADS] = node->value;
 }
}
