/*
 *  tree-bst-splay.c
 *  einit
 *
 *  Created by Magnus Deininger on 14/11/2006.
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
#include <einit/event.h>
#include <ctype.h>
#include <stdio.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* strees */

struct stree *streeadd (struct stree *stree, char *key, void *value, int32_t vlen, void *luggage) {
 struct stree *n, *rootnode = (stree ? *(stree->root) : NULL);
 struct stree *c = stree;
 uint32_t hklen;

 if (!key) return stree;
 hklen = strlen (key)+1;

 if (vlen == -1) {
  n = ecalloc (1, sizeof (struct stree) + hklen);

  memcpy ((((char *)n) + sizeof (struct stree)), key, hklen);

  n->key = (((char *)n) + sizeof (struct stree));
  n->value = value;
 } else  {
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

 n->root = stree ? stree->root : NULL;

/* n->next = stree;
 stree = n;*/
 n->next = stree;
 stree = n;

 if (!rootnode) {
  stree->root = emalloc (sizeof(struct stree *));
  *(stree->root) = stree;
 } else {
  *(stree->root) = stree;
 }

/* else {
  struct stree *parent = rootnode, *current = rootnode;
  uint32_t e = 0;

  for (; key[e]; e++) {
   if (current) {
    parent = current;
    if (key[e] < current->key[e]) {
     current = current->left;
    } else {
     current = current->right;
    }
   } else {
    if (key[e] < parent->key[e])
     parent->left = n;
    else
     parent->right = n;

    printf ("%s now %i child of %s[%x], under %s\n", n->key, (key[e] < parent->key[e]), parent->key, parent, rootnode->key);

    return stree;
//    break;
   }
  }
 }*/

 return stree;
}

struct stree *streedel (struct stree *subject) {
 struct stree *cur = (subject ? *(subject->root) : NULL),
              *be = cur;

 if (!cur || !subject) return subject;

/* char tmp[2048];
 snprintf (tmp, 2048, "streedel(): need to remove 0x%zx from 0x%zx", subject, cur);
 puts (tmp);*/

 if (cur == subject) {
  be = cur->next;
  *(subject->root) = be;
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
//  return cur;
 }

 return be;
}

struct stree *streefind (struct stree *stree, char *key, char options) {
 struct stree *c;
 if (!stree || !key) return NULL;

/* char tmp[2048];
 snprintf (tmp, 2048, "streefind(): need to find %s in 0x%zx", key, stree);
 puts (tmp); */

 if (options == TREE_FIND_FIRST) c = *(stree->root);
 else /* if (options == TREE_FIND_NEXT) */ c = stree->next;

 if (!c) return NULL;
// printf ("streefind(): need to find %s in 0x%zx->%s\n", key, c, c->key);

 while (strcmp (key, c->key) && c->next) c = c->next;
//  printf ("%s==%s?\n", key, c->key);
// }
 if (!c->next && strcmp (key, c->key)) return NULL;
// if (strcmp (key, c->key)) return NULL;

// printf ("streefind(): returning 0x%zx->%s for %s\n", c, c->key, key);
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