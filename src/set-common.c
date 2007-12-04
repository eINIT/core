/*
 *  set-common.c
 *  einit
 *
 *  Split from set-lowmem.c on 04/12/2007.
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

 efree (set1);

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
  efree (set);
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

signed int sortfunction_lexical (const void *a, const void *b) {
 return -1 * strcmp (a, b);
}

/* not exactly efficient, but it works, better than before */
void setsort (void **set, enum set_sort_order task, signed int(*sortfunction)(const void *, const void*)) {
 uint32_t i = 0;

 if (!set || !set[0] || !set[1]) return; // need a set with at least two elements to do anything meaningful.

 if (task == set_sort_order_string_lexical)
  sortfunction = sortfunction_lexical;
 else if (!sortfunction) return;

 for (;set[i];i++) {
  uint32_t j = i;
  char ex = 0;

  for (;set[j];j++) {
   if ((ex = (sortfunction(set[i], set[j]) < 0))) break;
  }

  if (ex) {
   void *cur = set[i];

   uint32_t k = i+1;
   for (;set[k];k++) {
    set[k-1] = set[k];
   }

   set[k-1] = cur;

   i--;
  }
 }
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
#endif

char ** inset_pattern (const void **haystack, const void *needle, int32_t esize) {
#ifdef POSIXREGEX
 regex_t pattern;
#endif
 int c = 0;
 char **retval = NULL;

 if (!haystack) return 0;
 if (!haystack[0]) return 0;
 if (!needle) return 0;

 if (esize == SET_TYPE_STRING) {
#ifdef POSIXREGEX
  if (eregcomp (&pattern, needle)) {
#endif
   for (; haystack[c] != NULL; c++)
    if (strmatch (haystack[c], needle))
     retval = (char **)setadd ((void **)retval, haystack[c], SET_TYPE_STRING);
#ifdef POSIXREGEX
  } else {
   for (; haystack[c] != NULL; c++)
    if (!regexec (&pattern, haystack[c], 0, NULL, 0))
     retval = (char **)setadd ((void **)retval, haystack[c], SET_TYPE_STRING);

   regfree (&pattern);
  }
#endif
 } else if (esize == -1) {
  for (; haystack[c] != NULL; c++)
   if (haystack[c] == needle)
    retval = (char **)setadd ((void **)retval, haystack[c], SET_TYPE_STRING);
 }

 return retval;
}

/* some functions to work with string-sets */

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
  efree (set);
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
  efree (set);
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
  efree (set);
  return NULL;
 }

 newset[x] = NULL;

 return newset;
}
