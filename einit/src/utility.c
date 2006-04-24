/***************************************************************************
 *            utlity.c
 *
 *  Sat Mar 25 18:15:21 2006
 *  Copyright  2006  Magnus Deininger
 *  dma05@web.de
 ****************************************************************************/
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

 newset = calloc (setcount(set1) + setcount(set2) +1, sizeof (void *));
 if (!newset) {
  bitch (BTCH_ERRNO);
  return NULL;
 }

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
 if (!set) set = calloc (1, sizeof (void *));

 newset = calloc (setcount(set) + 2, sizeof (void *));
 if (!newset) {
  bitch (BTCH_ERRNO);
  return NULL;
 }

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

 newset = calloc (setcount(set) +1, sizeof (char *));
 if (!newset) {
  bitch (BTCH_ERRNO);
  return NULL;
 }
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
 ret = calloc (sc+1, sizeof(char *));
 if (!ret) {
  bitch (BTCH_ERRNO);
  for (i = 0; i < l; i++) {
   if (input[i] == 0) {
    input[i] = sep;
   }
  }
  return NULL;
 }
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

 strbuffer = malloc (s*sizeof(char));
 if (!strbuffer) {
  bitch (BTCH_ERRNO);
  return NULL;
 }
 ssetbuffer = (char **)calloc (setcount ((void **)sset)+1, sizeof(char *));
 if (!ssetbuffer) {
  free (strbuffer);
  bitch (BTCH_ERRNO);
  return NULL;
 }
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

 newset = calloc (setcount((void **)sset) +1, sizeof (char *));
 if (!newset) {
  bitch (BTCH_ERRNO);
  return NULL;
 }
 while (sset[y]) {
  newset[y] = strdup (sset[y]);
  if (!newset [y]) {
   bitch (BTCH_ERRNO);
   for (y = 0; newset[y] != NULL; y++)
    free (newset[y]);
   free (newset);
   return NULL;
  }
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

/* hashes */

struct uhash *hashadd (struct uhash *hash, char *key, void *value) {
 struct uhash *n = calloc (1, sizeof (struct uhash));
 struct uhash *c = hash;
 if (!n) {
  bitch (BTCH_ERRNO);
  hashfree (hash);
  return NULL;
 }

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

/* those i-could've-sworn-there-were-library-functions-for-that functions */

char *cfg_getpath (char *id) {
 int mplen;
 struct cfgnode *svpath = cfg_findnode (id, 0, NULL);
 if (!svpath || !svpath->svalue) return NULL;
 mplen = strlen (svpath->svalue) +1;
 if (svpath->svalue[mplen-2] != '/') {
  char *tmpsvpath = (char *)realloc (svpath->svalue, mplen+1);
  if (!tmpsvpath) {
   bitch (BTCH_ERRNO);
   return NULL;
  }

  tmpsvpath[mplen-1] = '/';
  tmpsvpath[mplen] = 0;
  svpath->svalue = tmpsvpath;
 }
 return svpath->svalue;
}
