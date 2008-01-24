/*
 *  set-lowmem.c
 *  einit
 *
 *  Split out from utility.c on 20/01/2007.
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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <ctype.h>
#include <stdio.h>

#include <errno.h>
#include <limits.h>
#include <sys/types.h>

/* some common functions to work with null-terminated arrays */

#if 0
void **setadd (void **set, const void *item, int32_t esize) {
 void **newset = NULL;
 int x = 0;
 uint32_t count = 0;
 uintptr_t size = 0;

 if (!item) return NULL;

 if (esize == SET_NOALLOC) {
  if (set) for (; set[count]; count++);
  else count = 1;
  size = (count+2)*sizeof(void*);

  newset = ecalloc (1, size);

  if (set) {
   while (set[x]) {
    if (set[x] == item) {
     efree (newset);
     return set;
    }
    newset [x] = set[x];
    x++;
   }
   efree (set);
  }

  newset[x] = (void *)item;
 } else if (esize == SET_TYPE_STRING) {
  char *cpnt;
  uint32_t strlen_item = 1+strlen(item);

  if (set) for (; set[count]; count++);

  if (count) {
   uint32_t strlencache[count];

   for (count = 0; set[count]; count++) {
    size += sizeof(void*) + (strlencache[count] = (1+strlen(set[count])));
   }
   size += sizeof(void*)*2 + strlen_item;

   newset = ecalloc (1, size);
   cpnt = ((char *)newset) + (count+2)*sizeof(void*);

   while (set[x]) {
    memcpy (cpnt, set[x], strlencache[x]);
    newset [x] = cpnt;
    cpnt += strlencache[x];
    x++;
   }
   efree (set);
  } else {
   newset = ecalloc (1, sizeof(void*)*2 + strlen_item);
   cpnt = ((char *)newset) + (count+2)*sizeof(void*);
  }

  memcpy (cpnt, item, strlen_item);
  newset [x] = cpnt;
 } else {
  char *cpnt;

  if (set) for (; set[count]; count++) {
   size += sizeof(void*) + esize;
  }
  size += sizeof(void*)*2 + esize;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);

  if (set) {
   while (set[x]) {
    if (set[x] == item) {
     efree (newset);
     return set;
    }
    memcpy (cpnt, set[x], esize);
    newset [x] = cpnt;
    cpnt += esize;
    x++;
   }
   efree (set);
  }

  memcpy (cpnt, item, esize);
  newset [x] = cpnt;
 }

 return newset;
}
#else
char **set_str_add (char **set, char *item) {
 char **newset = NULL;
 int x = 0;
 uint32_t count = 0;
 uintptr_t size = 0;

 if (!item) return NULL;

 char *cpnt;
 uint32_t strlen_item = 1+strlen(item);

 if (set) for (; set[count]; count++);

 if (count) {
  uint32_t strlencache[count];

  for (count = 0; set[count]; count++) {
   size += sizeof(void*) + (strlencache[count] = (1+strlen(set[count])));
  }
  size += sizeof(void*)*2 + strlen_item;

  newset = (char **)ecalloc (1, size);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);

  while (set[x]) {
   memcpy (cpnt, set[x], strlencache[x]);
   newset [x] = cpnt;
   cpnt += strlencache[x];
   x++;
  }
  efree (set);
 } else {
  newset = (char **)ecalloc (1, sizeof(void*)*2 + strlen_item);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);
 }

 memcpy (cpnt, item, strlen_item);
 newset [x] = cpnt;

 return newset;
}

void **set_noa_add (void **set, void *item) {
 void **newset = NULL;
 int x = 0;
 uint32_t count = 0;
 uintptr_t size = 0;

 if (!item) return NULL;

 if (set) for (; set[count]; count++);
 else count = 1;
 size = (count+2)*sizeof(void*);

 newset = (void **)ecalloc (1, size);

 if (set) {
  while (set[x]) {
   if (set[x] == item) {
    efree (newset);
    return set;
   }
   newset [x] = set[x];
   x++;
  }
  efree (set);
 }

 newset[x] = (void *)item;

 return newset;
}

void **set_fix_add (void **set, void *item, int32_t esize) {
 void **newset = NULL;
 int x = 0;
 uint32_t count = 0;
 uintptr_t size = 0;

 if (!item) return NULL;

 char *cpnt;

 if (set) for (; set[count]; count++) {
  size += sizeof(void*) + esize;
 }
 size += sizeof(void*)*3 + esize;

 newset = (void **)ecalloc (1, size);
 cpnt = ((char *)newset) + (count+2)*sizeof(void*);

 if (set) {
  while (set[x]) {
   if (set[x] == item) {
    efree (newset);
    return set;
   }
   memcpy (cpnt, set[x], esize);
   newset [x] = cpnt;
   cpnt += esize;
   x++;
  }
  efree (set);
 }

 memcpy (cpnt, item, esize);
 newset [x] = cpnt;

 return newset;
}
#endif

void **setdup (const void **set, int32_t esize) {
 void **newset;
 uint32_t y = 0, count = 0, size = 0;
 if (!set) return NULL;
 if (!set[0]) return NULL;

 if (esize == -1) {
  newset = ecalloc (setcount(set) +1, sizeof (char *));
  while (set[y]) {
   newset[y] = (void *)set[y];
   y++;
  }
 } else if (esize == 0) {
  char *cpnt;

  for (; set[count]; count++)
   size += sizeof(void*) + 1 + strlen(set[count]);
  size += sizeof(void*)*2;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  while (set[y]) {
   esize = 1+strlen(set[y]);
   memcpy (cpnt, set[y], esize);
   newset [y] = cpnt;
   cpnt += esize;
   y++;
  }
 } else {
  char *cpnt;

  for (; set[count]; count++)
   size += sizeof(void*) + esize;
  size += sizeof(void*)*2;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  while (set[y]) {
   memcpy (cpnt, set[y], esize);
   newset [y] = cpnt;
   cpnt += esize;
   y++;
  }
 }

 return newset;
}

/* some functions to work with string-sets */

char **str2set (const char sep, const char *oinput) {
 int l, i = 0, sc = 1, cr = 1;
 char **ret;
 char *input;
 if (!oinput || !(input = estrdup (oinput))) return NULL;
 l = strlen (input)-1;

 for (; i < l; i++) {
  if (input[i] == sep) {
   sc++;
//   input[i] = 0;
  }
 }
 ret = ecalloc (1, ((sc+1)*sizeof(char *)) + 3 + l);
 memcpy ((((char *)ret) + ((sc+1)*sizeof(char *))), input, 2 + l);
 efree (input);
 input = (char *)(((char *)ret) + ((sc+1)*sizeof(char *)));
 ret[0] = input;
 for (i = 0; i < l; i++) {
  if (input[i] == sep) {
   ret[cr] = input+i+1;
   input[i] = 0;
   cr++;
  }
 }
 return ret;
}

