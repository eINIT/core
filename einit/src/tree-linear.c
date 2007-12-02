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

struct stree *streeadd (const struct stree *stree, const char *key, const void *value, int32_t vlen, const void *luggage) {
 struct stree *n, *base = (stree ? *(stree->lbase) : NULL);
 uint32_t hklen;

 if (!key) return (struct stree *)stree;
 hklen = strlen (key)+1;

 if (vlen == -1) {
  n = ecalloc (1, sizeof (struct stree) + hklen);

  memcpy ((((char *)n) + sizeof (struct stree)), key, hklen);

  n->key = (((char *)n) + sizeof (struct stree));
  n->value = (void *)value;
 } else {
  if (!value) return (struct stree *)stree;
  if (vlen == 0)
   vlen = strlen (value)+1;

  n = ecalloc (1, sizeof (struct stree) + hklen + vlen);
  memcpy ((((char *)n) + sizeof (struct stree)), key, hklen);
  memcpy ((((char *)n) + sizeof (struct stree) + hklen), value, vlen);

  n->key = (((char *)n) + sizeof (struct stree));
  n->value = (((char *)n) + sizeof (struct stree) + hklen);
 }

 n->luggage = (void *)luggage;

 n->lbase = stree ? stree->lbase : NULL;

 n->keyhash = hashp(key);

// n->next = base;
 n->next = stree && stree->lbase ? *stree->lbase : base;
 stree = n;

 if (!base)
  n->lbase = emalloc (sizeof(struct stree *));

 *(n->lbase) = (struct stree *)stree;

 return n;
}

struct stree *streedel (struct stree *subject) {
 struct stree *cur = (subject ? *(subject->lbase) : NULL),
              *be = cur;

 if (!cur || !subject) return subject;

 if (cur == subject) {
  be = cur->next;
  *(subject->lbase) = be;
  if (cur->luggage) efree (cur->luggage);
  efree (cur);

  return be;
 }

 while (cur && (cur->next != subject))
  cur = cur->next;

 if (cur && (cur->next == subject)) {
  cur->next = subject->next;
  if (subject->luggage) efree (subject->luggage);
  efree (subject);
 }

 return be;
}

struct stree *streefind (const struct stree *stree, const char *key, enum tree_search_base options) {
 const struct stree *c;

 if (!stree || !key) return NULL;

 uintptr_t khash = hashp(key);

 c = (options == tree_find_first) ? *(stree->lbase) : stree->next;

 while (c) {
  if ((khash == c->keyhash) && strmatch (key, c->key)) {
   return (struct stree *)c;
  }
  c = c->next;
 }

 return (struct stree *)c;
}

void streefree (struct stree *stree) {
 struct stree *c = stree;
 if (!stree) return;
 struct stree **base = stree->lbase;
 while (c) {
  struct stree *d = c;
  c = c->next;
  if (d->luggage) efree (d->luggage);
  efree (d);
 }

 efree (base);
}
