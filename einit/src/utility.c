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

/* some common functions to work with null-terminated arrays */

void **setcombine (void **set1, void **set2) {
 void **newset;
 int x = 0, y = 0, s = 1, p = 0;
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

 newset = ecalloc (setcount(set1) + setcount(set2) +1, sizeof (void *));

 while (set1[x])
  { newset [x] = set1[x]; x++; }
 y = x; x = 0;
 while (set2[x])
  { newset [y] = set2[x]; x++; y++; }

 return newset;
}

void **setadd (void **set, void *item) {
 void **newset;
 int x = 0, y = 0, s = 1, p = 0;
 char *strbuffer = NULL;
 if (!item) return NULL;
 if (!set) set = ecalloc (1, sizeof (void *));

 newset = ecalloc (setcount(set) + 2, sizeof (void *));

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

 return newset;
}

void **setdup (void **set) {
 void **newset;
 int y = 0;
 if (!set) return NULL;
 if (!set[0]) return NULL;

 newset = ecalloc (setcount(set) +1, sizeof (char *));
 while (set[y]) {
  newset[y] = set[y];
  y++;
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

/* some functions to work with string-sets */

char **str2set (const char sep, char *input) {
 int l, i = 0, sc = 1, cr = 1;
 char **ret;
 if (!input) return NULL;
 l = strlen (input)-1;

 for (; i < l; i++) {
  if (input[i] == sep) {
   sc++;
   input[i] = 0;
  }
 }
 ret = ecalloc (sc+1, sizeof(char *));
 ret[0] = input;
 for (i = 0; i < l; i++) {
  if (input[i] == 0) {
   ret[cr] = input+i+1;
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

char **strsetrebuild (char **sset) {
 int y = 0, p = 0, s = 1;
 char *strbuffer;
 char **ssetbuffer;
 if (!sset) return NULL;
 if (!sset[0]) return NULL;

 while (sset[y])
  { y++; s += strlen (sset[y]); }

 strbuffer = emalloc (s*sizeof(char));
 ssetbuffer = (char **)ecalloc (setcount ((void **)sset)+1, sizeof(char *));
 strbuffer[s-1] = 0;
 while (sset[y])
  { y++; s += strlen (sset[y]); }
 return ssetbuffer;
}

char **strsetdup (char **sset) {
 char **newset;
 int y = 0;
 if (!sset) return NULL;
 if (!sset[0]) return NULL;

 newset = ecalloc (setcount((void **)sset) +1, sizeof (char *));
 while (sset[y]) {
  newset[y] = estrdup (sset[y]);
  y++;
 }

 return newset;
}

char **strsetdel (char **set, char *item) {
 char **newset = set;
 int x = 0, y = 0, s = 1, p = 0;
 if (!item || !set) return NULL;

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
  free (set);
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
 char *newitem;
 int len = 1;
 if (key) len += strlen (key);
 if (value) len += strlen (value);
 newitem = ecalloc (1, sizeof(char)*len);
 if (key) newitem = strcat (newitem, key);
 if (value) newitem = strcat (newitem, "=");
 if (value) newitem = strcat (newitem, value);

 return (char**) setadd ((void**)environment, (void*)newitem);
}

/* hashes */

struct uhash *hashadd (struct uhash *hash, char *key, void *value) {
 struct uhash *n = ecalloc (1, sizeof (struct uhash));
 struct uhash *c = hash;

 n->key = key;
 n->value = value;

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

 return hash;
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
