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

void **setcombine (const void **set1, const void **set2, const int32_t esize) {
 void **newset = NULL;
 int x = 0;

 if (!set1) return setdup(set2, esize);
 if (!set2) return setdup(set1, esize);

 for (x = 0; set1[x]; x++) {
  if (!inset ((const void **)newset, set1[x], esize)) {
   newset = setadd (newset, set1[x], esize);
  }
 }
 for (x = 0; set2[x]; x++) {
  if (!inset ((const void **)newset, set2[x], esize)) {
   newset = setadd (newset, set2[x], esize);
  }
 }

 return newset;
}

void **setcombine_nc (void **set1, const void **set2, const int32_t esize) {
 int x = 0;

 if (!set1) return setdup(set2, esize);
 if (!set2) return set1;

 for (x = 0; set2[x]; x++) {
  if (!inset ((const void **)set1, set2[x], esize)) {
   set1 = setadd (set1, set2[x], esize);
  }
 }

 return set1;
}

void **setslice (const void **set1, const void **set2, const int32_t esize) {
 void **newset = NULL;
 int x = 0;

 if (!set1) return NULL;
 if (!set2) return setdup(set1, esize);

 for (; set1[x]; x++) {
  if (!inset (set2, set1[x], esize)) {
   newset = setadd (newset, set1[x], esize);
  }
 }

 return newset;
}

void **setslice_nc (void **set1, const void **set2, const int32_t esize) {
 void **newset = NULL;
 int x = 0;

 if (!set1) return NULL;
 if (!set2) return set1;

 for (; set1[x]; x++) {
  if (!inset (set2, set1[x], esize)) {
   newset = setadd (newset, set1[x], esize);
  }
 }

 free (set1);

 return newset;
}

void **setadd (void **set, const void *item, int32_t esize) {
 void **newset = NULL;
 int x = 0;
 uint32_t count = 0, size = 0;

 if (!item) return NULL;
// if (!set) set = ecalloc (1, sizeof (void *));

 if (esize == SET_NOALLOC) {
  if (set) for (; set[count]; count++);
  else count = 1;
  size = (count+2)*sizeof(void*);

  newset = ecalloc (1, size);

  if (set) {
   while (set[x]) {
    if (set[x] == item) {
     free (newset);
     return set;
    }
    newset [x] = set[x];
    x++;
   }
   free (set);
  }

  newset[x] = (void *)item;
 } else if (esize == SET_TYPE_STRING) {
  char *cpnt;

  if (set) for (; set[count]; count++) {
   size += sizeof(void*) + 1 + strlen(set[count]);
  }
  size += sizeof(void*)*2 + 1 +strlen(item);

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);

  if (set) {
   while (set[x]) {
    if (set[x] == item) {
     free (newset);
     return set;
    }
    esize = 1+strlen(set[x]);
    memcpy (cpnt, set[x], esize);
    newset [x] = cpnt;
    cpnt += esize;
    x++;
   }
   free (set);
  }

  esize = 1+strlen(item);
  memcpy (cpnt, item, esize);
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
     free (newset);
     return set;
    }
    memcpy (cpnt, set[x], esize);
    newset [x] = cpnt;
    cpnt += esize;
    x++;
   }
   free (set);
  }

  memcpy (cpnt, item, esize);
  newset [x] = cpnt;
//  cpnt += esize;
 }

 return newset;
}

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

void **setdel (void **set, const void *item) {
 void **newset = set;
 int x = 0, y = 0;

 if (!item || !set) return NULL;

 while (set[y]) {
  if (set[y] != item) {
   newset [x] = set[y];
   x++;
  }
  y++;
/*  else {
   set = set+1;
  }*/
 }

 if (!x) {
  free (set);
  return NULL;
 }

 newset[x] = NULL;

 return newset;
}

int setcount (const void **set) {
 int i = 0;
 if (!set) return 0;
 if (!set[0]) return 0;
 while (set[i])
  i++;

 return i;
}

void setsort (void **set, const char task, signed int(*sortfunction)(const void *, const void*)) {
 uint32_t c = 0, c2 = 0, x = 0, dc = 1;
 void *tmp;
 if (!set) return;

 if (task == SORT_SET_STRING_LEXICAL)
  sortfunction = (signed int(*)(const void *, const void*))strcmp;
 else if (!sortfunction) return;

/* this doesn't work, yet */
 for (; set[c]; c++) {
  for (c2 = c+1; set[c2]; c2++) {
   if ((x = sortfunction(set[c], set[c2])) > 0) {
    dc = 1;
    tmp = set[c2];
    set[c2] = set[c];
    set[c] = tmp;
   }
  }
 }

 return;
}

int inset (const void **haystack, const void *needle, int32_t esize) {
 int c = 0;

 if (!haystack) return 0;
 if (!haystack[0]) return 0;
 if (!needle) return 0;

 if (esize == SET_TYPE_STRING) {
  for (; haystack[c] != NULL; c++)
   if (strmatch (haystack[c], needle)) return 1;
 } else if (esize == -1) {
  for (; haystack[c] != NULL; c++)
   if (haystack[c] == needle) return 1;
 }
 return 0;
}

#ifdef POSIXREGEX
#include <regex.h>

int inset_pattern (const void **haystack, const void *needle, int32_t esize) {
 regex_t pattern;
 int c = 0;

 if (!haystack) return 0;
 if (!haystack[0]) return 0;
 if (!needle) return 0;

 if (esize == SET_TYPE_STRING) {
  if (eregcomp (&pattern, needle)) {
   for (; haystack[c] != NULL; c++)
    if (strmatch (haystack[c], needle)) return 1;
  } else {
   for (; haystack[c] != NULL; c++)
    if (!regexec (&pattern, haystack[c], 0, NULL, 0)) return 1;

   regfree (&pattern);
  }
 } else if (esize == -1) {
  for (; haystack[c] != NULL; c++)
   if (haystack[c] == needle) return 1;
 }

 return 0;
}
#endif

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

char *set2str (const char sep, const char **input) {
 char *ret = NULL;
 size_t slen = 0;
 uint32_t i = 0;
 char nsep[2] = {sep, 0};

 if (!input) return NULL;

 for (; input[i]; i++) {
  slen += strlen (input[i])+1;
 }

 ret = emalloc (slen);
 *ret = 0;

 for (i = 0; input[i]; i++) {
  if (i != 0)
   strcat (ret, nsep);

  strcat (ret, input[i]);
 }

 return ret;
}

char **strsetdel (char **set, char *item) {
 char **newset = set;
 int x = 0, y = 0;

 if (!item || !set) return NULL;
 if (!set[0]) {
  free (set);
  return NULL;
 }

 while (set[y]) {
  if (strcmp(set[y], item)) {
   newset [x] = set[y];
   x++;
  }
  y++;
/*  else {
   set = set+1;
  }*/
 }

 if (!x) {
//  free (set);
  return NULL;
 }

 newset[x] = NULL;

 return newset;
}

char **strsetdeldupes (char **set) {
 char **newset = set;
 int x = 0, y = 0;

 if (!set) return NULL;

 while (set[y]) {
  char *tmp = set[y];
  set[y] = NULL;
  if (!inset ((const void **)set, (const void *)tmp, SET_TYPE_STRING)) {
   newset [x] = tmp;
   x++;
  }
  y++;
/*  else {
   set = set+1;
  }*/
 }

 if (!x) {
  free (set);
  return NULL;
 }

 newset[x] = NULL;

 return newset;
}
