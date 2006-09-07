/*
 *  utility.c
 *  einit
 *
 *  Created by Magnus Deininger on 25/03/2006.
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
#include <string.h>
#include <stdlib.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <ctype.h>
#include <stdio.h>

/* some common functions to work with null-terminated arrays */

void **setcombine (void **set1, void **set2, int32_t esize) {
 void **newset;
 int x = 0, y = 0, s = 1, p = 0;
 uint32_t count = 0, size = 0;
 char *strbuffer = NULL;
 if (!set1) return set2;
 if (!set1[0]) {
  free (set1);
  return set2;
 }
 if (!set2) return set1;
 if (!set2[0]) {
  free (set2);
  return set1;
 }

 if (esize == -1) {
  for (; set1[count]; count++);
  size = count+1;

  for (x = 0; set2[x]; x++);
  size += x;
  count += x;
  size *= sizeof (void*);

  newset = ecalloc (1, size);

  x = 0;
  while (set1[x])
   { newset [x] = set1[x]; x++; }
  y = x; x = 0;
  while (set2[x])
   { newset [y] = set2[x]; x++; y++; }
 } else if (esize == 0) {
  char *cpnt;

  for (; set1[count]; count++)
   size += sizeof(void*) + 1 + strlen(set1[count]);
  size += sizeof(void*);
  for (x = 0; set2[x]; x++)
   size += sizeof(void*) + 1 + strlen(set2[x]);
  count += x;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  x = 0;
  while (set1[x]) {
   esize = 1+strlen(set1[x]);
   memcpy (cpnt, set1[x], esize);
   newset [x] = cpnt;
   cpnt += esize;
   x++;
  }
  y = x; x = 0;
  while (set2[x]) {
   esize = 1+strlen(set2[x]);
   memcpy (cpnt, set2[x], esize);
   newset [y] = cpnt;
   cpnt += esize;
   x++;
   y++;
  }
 } else {
  char *cpnt;

  for (; set1[count]; count++)
   size += sizeof(void*) + 1 + esize;;
  size += sizeof(void*);
  for (x = 0; set2[x]; x++)
   size += sizeof(void*) + 1 + esize;
  count += x;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  x = 0;
  while (set1[x]) {
   memcpy (cpnt, set1[x], esize);
   newset [x] = cpnt;
   cpnt += esize;
   x++;
  }
  y = x; x = 0;
  while (set2[x]) {
   memcpy (cpnt, set2[x], esize);
   newset [y] = cpnt;
   cpnt += esize;
   x++;
   y++;
  }
 }

 return newset;
}

void **setadd (void **set, void *item, int32_t esize) {
 void **newset;
 int x = 0, y = 0, s = 1, p = 0;
 char *strbuffer = NULL;
 uint32_t count = 0, size = 0;
 if (!item) return NULL;
 if (!set) set = ecalloc (1, sizeof (void *));

 if (esize == -1) {
  for (; set[count]; count++);
  size = (count+2)*sizeof(void*);

  newset = ecalloc (1, size);

  while (set[x]) {
   if (set[x] == item) {
    free (newset);
    return set;
   }
   newset [x] = set[x];
   x++;
  }

  newset[x] = item;
  free (set);
 } else if (esize == 0) {
  char *cpnt;

//  puts ("adding object to string-set");
  for (; set[count]; count++)
   size += sizeof(void*) + 1 + strlen(set[count]);
  size += sizeof(void*)*2 + 1 +strlen(item);

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);

  while (set[x]) {
   if (set[x] == item) {
    free (newset);
    return set;
   }
   esize = 1+strlen(set[x]);
   memcpy (cpnt, set[x], esize);
   newset [x] = cpnt;
   cpnt += esize;
//   puts(set[x]);
   x++;
  }

  esize = 1+strlen(item);
  memcpy (cpnt, item, esize);
  newset [x] = cpnt;
//  puts(item);
//  cpnt += 1+strlen(item);

  free (set);
 } else {
  char *cpnt;

  for (; set[count]; count++)
   size += sizeof(void*) + esize;
  size += sizeof(void*)*2 + esize;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);

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

  memcpy (cpnt, item, esize);
  newset [x] = cpnt;
//  cpnt += esize;
  free (set);
 }

 return newset;
}

void **setdup (void **set, int32_t esize) {
 void **newset;
 uint32_t y = 0, count = 0, size = 0;
 if (!set) return NULL;
 if (!set[0]) return NULL;

 if (esize == -1) {
  newset = ecalloc (setcount(set) +1, sizeof (char *));
  while (set[y]) {
   newset[y] = set[y];
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

void **setdel (void **set, void *item) {
 void **newset = set;
 int x = 0, y = 0, s = 1, p = 0;
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

int setcount (void **set) {
 int i = 0;
 if (!set) return 0;
 if (!set[0]) return 0;
 while (set[i])
  i++;

 return i;
}

void setsort (void **set, char task, signed int(*sortfunction)(void *, void*)) {
 uint32_t c = 0, c2 = 0, x = 0, dc = 1;
 void *tmp;
 if (!set) return;

 if (task == SORT_SET_STRING_LEXICAL)
  sortfunction = (signed int(*)(void *, void*))strcmp;
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

/* some functions to work with string-sets */

char **str2set (const char sep, char *input) {
 int l, i = 0, sc = 1, cr = 1;
 char **ret;
 if (!input) return NULL;
 l = strlen (input)-1;

 for (; i < l; i++) {
  if (input[i] == sep) {
   sc++;
//   input[i] = 0;
  }
 }
 ret = ecalloc (1, ((sc+1)*sizeof(char *)) + 2 + l);
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

int strinset (char **haystack, const char *needle) {
 int c = 0;
 if (!haystack) return 0;
 if (!haystack[0]) return 0;
 if (!needle) return 0;
 for (; haystack[c] != NULL; c++) {
  if (!strcmp (haystack[c], needle)) return 1;
 }
 return 0;
}

char **strsetdel (char **set, char *item) {
 char **newset = set;
 int x = 0, y = 0, s = 1, p = 0;
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
 int x = 0, y = 0, s = 1, p = 0;
 if (!set) return NULL;

 while (set[y]) {
  char *tmp = set[y];
  set[y] = NULL;
  if (!strinset (set, tmp)) {
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

char **straddtoenviron (char **environment, char *key, char *value) {
 char **ret;
 char *newitem;
 int len = 2;
 if (key) len += strlen (key);
 if (value) len += strlen (value);
 newitem = ecalloc (1, sizeof(char)*len);
// newitem = ecalloc (1, len);
// newitem[0] = 0;
 if (key) newitem = strcat (newitem, key);
 if (value) newitem = strcat (newitem, "=");
 if (value) newitem = strcat (newitem, value);

// puts (newitem);

 ret = (char**) setadd ((void**)environment, (void*)newitem, SET_TYPE_STRING);
// free (newitem);

 return ret;
}

/* hashes */

struct uhash *hashadd (struct uhash *hash, char *key, void *value, int32_t vlen, void *luggage) {
 struct uhash *n;
 struct uhash *c = hash;
 uint32_t hklen;

 if (!key) return hash;
 hklen = strlen (key)+1;

 if (vlen == -1) {
  n = ecalloc (1, sizeof (struct uhash) + hklen);

  memcpy ((((char *)n) + sizeof (struct uhash)), key, hklen);

  n->key = (((char *)n) + sizeof (struct uhash));
  n->value = value;
 } else  {
  if (!value) return hash;
  if (vlen == 0)
   vlen = strlen (value)+1;

//  printf ("new hash-node: key=%s, vlen=%i\n", key, vlen);

  n = ecalloc (1, sizeof (struct uhash) + hklen + vlen);
  memcpy ((((char *)n) + sizeof (struct uhash)), key, hklen);
  memcpy ((((char *)n) + sizeof (struct uhash) + hklen), value, vlen);

  n->key = (((char *)n) + sizeof (struct uhash));
  n->value = (((char *)n) + sizeof (struct uhash) + hklen);
 }

 if (!hash)
  hash = n;
 else {
  while (c->next) {
   if (c->key && !strcmp (key, c->key) && c->value == value) {
    free (n);
    return hash;
   }
   c = c->next;
  }
  if (c->key && !strcmp (key, c->key) && c->value == value) {
   free (n);
   return hash;
  }
  c->next = n;
 }
// n->next = hash;
// hash = n;

 return hash;
}

struct uhash *hashdel (struct uhash *cur, struct uhash *subject) {
 if (!cur || !subject) return cur;
 if ((cur == subject) && !subject->next) {
  free (cur);
  return NULL;
 }
 while (cur && (cur->next != subject))
  cur = cur->next;
 if (cur && (cur->next == subject)) {
  cur->next = subject->next;
  if (subject->luggage) free (subject->luggage);
  free (subject);
  return cur;
 }
}

struct uhash *hashfind (struct uhash *hash, char *key) {
 struct uhash *c = hash;
 if (!hash || !key) return NULL;
 while ((!c->key || strcmp (key, c->key)) && c->next) c = c->next;
 if (!c->next && strcmp (key, c->key)) return NULL;
 return c;
}

void hashfree (struct uhash *hash) {
 struct uhash *c = hash;
 if (!hash) return;
 while (c) {
  struct uhash *d = c;
  c = c->next;
  if (d->luggage) free (d->luggage);
  free (d);
 }
}

/* safe malloc/calloc/realloc/strdup functions */

void *emalloc (size_t s) {
 void *p = NULL;

 while (!(p = malloc (s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

void *ecalloc (size_t c, size_t s) {
 void *p = NULL;

 while (!(p = calloc (c, s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

void *erealloc (void *c, size_t s) {
 void *p = NULL;

 while (!(p = realloc (c, s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

char *estrdup (char *s) {
 char *p = NULL;

 while (!(p = strdup (s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

/* nifty string functions */
void strtrim (char *s) {
 if (!s) return;
 uint32_t l = strlen (s), i = 0, offset = 0;

 for (; i < l; i++) {
  if (isspace (s[i])) offset++;
  else {
   if (offset)
    memmove (s, s+offset, l-offset+1);
   break;
  }
 }

 if (i == l) {
  s[0] = 0;
  return;
 }

 l -= offset+1;

 for (i = l; i >= 0; i--) {
  if (isspace (s[i])) s[i] = 0;
  else break;
 }
}
