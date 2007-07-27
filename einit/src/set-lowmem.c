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
 uint32_t count = 0;
 uintptr_t size = 0;

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
  uint32_t strlen_item = 1+strlen(item);

  if (set) for (; set[count]; count++);

  if (count) {
   uint32_t strlencache[count]/*, cpp = 0*/;
/*   ssize_t copyblock_size[count+1];
   void *copyblock_ptr[count+1];

   copyblock_size[0] = 0;
   copyblock_ptr[0] = NULL;*/

   for (count = 0; set[count]; count++) {
    size += sizeof(void*) + (strlencache[count] = (1+strlen(set[count])));

/*    if (copyblock_size[cpp]) {
    } else {*/
/*    {
     copyblock_ptr[cpp] = set[count];
     copyblock_size[cpp] = strlencache[count];
     cpp++;
    }*/
   }
   size += sizeof(void*)*2 + strlen_item;

   newset = ecalloc (1, size);
   cpnt = ((char *)newset) + (count+2)*sizeof(void*);

   while (set[x]) {
/*    if (set[x] == item) {
     free (newset);
     return set;
    }*/
    memcpy (cpnt, set[x], strlencache[x]);
    newset [x] = cpnt;
    cpnt += strlencache[x];
    x++;
   }
   free (set);
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

/* not exactly efficient, but it works, better than before */
void setsort (void **set, enum set_sort_order task, signed int(*sortfunction)(const void *, const void*)) {
 uint32_t i = 0;

 if (!set || !set[0] || !set[1]) return; // need a set with at least two elements to do anything meaningful.

 if (task == set_sort_order_string_lexical)
  sortfunction = (signed int(*)(const void *, const void*))strcmp;
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
 free (input);
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
