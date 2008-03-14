/*
 *  tree-itree.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/12/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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
#include <stdio.h>
#include <stdlib.h>

#include <inttypes.h>
#include <einit/tree.h>
#include <einit/itree.h>
#include <einit/utility.h>
#include <string.h>

struct stree *streeadd (const struct stree *stree, const char *key, const void *value, int32_t vlen, const void *luggage) {
 if (!key) return NULL;
 signed long keyhash = hashp (key);
 //int len;
 //uint32_t keyhash = StrSuperFastHash(key, &len);

 size_t nodesize;
 struct stree *newnode;

 switch (vlen) {
  case tree_value_noalloc:
   nodesize = sizeof (struct stree);
   break;
  case tree_value_string:
   vlen = strlen (value) + 1;
  default:
   nodesize = sizeof (struct stree) + vlen;
   break;
 }

 newnode = emalloc (nodesize);
 memset (newnode, 0, sizeof (struct stree));

 newnode->key = (char *)str_stabilise (key);

 switch (vlen) {
  case tree_value_noalloc:
   newnode->value = (void *)value;
   break;
  default:
   newnode->value = ((char *)newnode) + sizeof (struct stree);
   memcpy (newnode->value, value, vlen);
   break;
 }

 newnode->luggage = (void *)luggage;

 newnode->treenode = itreeadd (stree ? stree->treenode : NULL, keyhash, newnode, tree_value_noalloc);

// newnode->treenode = itreefind (newnode->treenode, keyhash, tree_find_first);

/* if (newnode->treenode->value != newnode) {
  fprintf (stderr, "TOOT TOOT!\n");
 }*/
 return newnode;
}

struct stree *streedel (struct stree *subject) {
 struct itree *it = itreedel (subject->treenode);
 if (subject->luggage) efree (subject->luggage);
 efree (subject);

 if (it)
  return it->value;

 return NULL;
}

struct stree *streefind (const struct stree *stree, const char *key, enum tree_search_base options) {
 if (!key || !stree) return NULL;
 signed long keyhash;
 struct itree *it = stree->treenode;

 //int len = 0;

 switch (options) {
  case tree_find_next:
   keyhash = stree->treenode->key;
   break;
  default:
   keyhash = hashp (key);
   //keyhash = StrSuperFastHash (key, &len);
   break;
 }

// fprintf (stderr, "search: %i, it=%i, sv=%i, options=%i\n", keyhash, it, stree, options);

 while ((it = itreefind (it, keyhash, options))) {
//  fprintf (stderr, "candidate: %i, %i, %i\n", keyhash, it, it->value);

  struct stree *st = it->value;
  if (strmatch (st->key, key)) {
   return st;
  } else {
   options = tree_find_next;
  }
 }

#if 0
 fprintf (stderr, "i'm outta options...\n");
#ifdef DEBUG
 itreedump (stree->treenode);
#endif
 fflush (stderr);
#endif
 return NULL;
}

void streefree_node (struct stree *st) {
 if (st->luggage) efree (st->luggage);
 efree (st);
}

void streefree (struct stree *stree) {
 if (stree) {
  itreefree_all (stree->treenode, (void (*)(void *))streefree_node);
 }
}

void streelinear_prepare_iterator (struct itree *it, struct stree **p) {
 struct stree *st = it->value;

 st->next = *p;
 *p = st;
}

struct stree *streelinear_prepare (struct stree *st) {
 if (st) {
  struct itree *it = itreeroot (st->treenode);
  struct stree *t = NULL;

  itreemap(it, (void (*)(struct itree *, void *))streelinear_prepare_iterator, (void *)&t);

  it = itreeroot (it);
  return it->value;
 }

 return NULL;
}
