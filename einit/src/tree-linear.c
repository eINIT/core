/*
 *  tree-linear.c
 *  einit
 *
 *  Created by Magnus Deininger on 14/11/2006.
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
#include <string.h>
#include <stdlib.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/tree.h>
#include <einit/event.h>
#include <ctype.h>
#include <stdio.h>

#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* strees */

// linear implementation

struct stree *streeadd (struct stree *stree, char *key, void *value, int32_t vlen, void *luggage) {
 struct stree *n, *base = (stree ? *(stree->lbase) : NULL);
 uint32_t hklen;

 if (!key) return stree;
 hklen = strlen (key)+1;

 if (vlen == -1) {
  n = ecalloc (1, sizeof (struct stree) + hklen);

  memcpy ((((char *)n) + sizeof (struct stree)), key, hklen);

  n->key = (((char *)n) + sizeof (struct stree));
  n->value = value;
 } else {
  if (!value) return stree;
  if (vlen == 0)
   vlen = strlen (value)+1;

  n = ecalloc (1, sizeof (struct stree) + hklen + vlen);
  memcpy ((((char *)n) + sizeof (struct stree)), key, hklen);
  memcpy ((((char *)n) + sizeof (struct stree) + hklen), value, vlen);

  n->key = (((char *)n) + sizeof (struct stree));
  n->value = (((char *)n) + sizeof (struct stree) + hklen);
 }

 n->luggage = luggage;

 n->lbase = stree ? stree->lbase : NULL;

// n->next = base;
 n->next = stree && stree->lbase ? *stree->lbase : base;
 stree = n;

 if (!base)
  stree->lbase = emalloc (sizeof(struct stree *));

 *(stree->lbase) = stree;

 return stree;
}

struct stree *streedel (struct stree *subject) {
 struct stree *cur = (subject ? *(subject->lbase) : NULL),
              *be = cur;

// puts (" >> called streedel");

 if (!cur || !subject) return subject;

 if (cur == subject) {
  be = cur->next;
  *(subject->lbase) = be;
  if (cur->luggage) free (cur->luggage);
  free (cur);

  return be;
 }

 while (cur && (cur->next != subject))
  cur = cur->next;

 if (cur && (cur->next == subject)) {
  cur->next = subject->next;
  if (subject->luggage) free (subject->luggage);
  free (subject);
 }

 return be;
}

struct stree *streefind (struct stree *stree, char *key, char options) {
 struct stree *c;
 if (!stree || !key) return NULL;

// printf (" >> streefind: %s in %x:%x:\n", key, stree, (stree->lbase ? *(stree->lbase) : NULL));
 if (!stree->lbase) {
  puts (" >> bad input");
  return NULL;
 }

 if (options == TREE_FIND_FIRST) {
  if (stree->lbase)
   c = *(stree->lbase);
  else
   c = stree;
 } else c = stree->next;

/* if (c)
  printf (" >> %x(%s), c=%x(%s)\n", *(stree->lbase), (*(stree->lbase))->key, c, c->key);
 else
  printf (" >> %x(%s), no (further) result(s)\n", *(stree->lbase), (*(stree->lbase))->key);*/

 if (!c) return NULL;

 while (c) {
/*  if (!c->key) c = c->next;
  printf (" >> streefind: testing %s==%s in %x:%x@%x\n", key, c->key, stree, (stree->lbase ? *(stree->lbase) : NULL), c);*/
  if (!strcmp (key, c->key)) {
//   printf (" >> streefind: found %s in %x:%x@%x\n", key, stree, (stree->lbase ? *(stree->lbase) : NULL), c);

   return c;
  }
  c = c->next;
 }
 if (c && (!c->next) && strcmp (key, c->key)) return NULL;

 return c;
}

void streefree (struct stree *stree) {
 struct stree *c = stree;
 if (!stree) return;
 while (c) {
  struct stree *d = c;
  c = c->next;
  if (d->luggage) free (d->luggage);
  free (d);
 }
}
