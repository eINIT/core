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
#include <einit/tree.h>
#include <ctype.h>
#include <stdio.h>

#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* strees */

// this is the splay-tree implementation. it should be considerably faster than the linear implementation

uint32_t streesplay (struct stree *current, struct stree *parent, struct stree *grandparent) {
 if (!current || (current == *(current->root))) return; // bail out early if something is weird
 if (!parent) parent = current->parent;
 if (!grandparent && parent) grandparent = parent->parent;

 while (grandparent) {
  struct stree **gppsp;

//  puts ("splaying");

  if (grandparent->parent) { // find pointer to modify for the root
   if (grandparent->parent->left == grandparent)
    gppsp = &(grandparent->parent->left);
   else
    gppsp = &(grandparent->parent->right);
  } else
   gppsp = current->root;

  if ((parent->right == current) && (grandparent->left == parent)) {         // zig-zag left
   struct stree *cl = current->left;
   struct stree *cr = current->right;

   current->left = parent;
   current->right = grandparent;

   parent->right = cl;
   grandparent->left = cr;

   if (cr) cr->parent = grandparent;
   if (cl) cl->parent = parent;
   current->parent = grandparent->parent;

   parent->parent = current;
   grandparent->parent = current;

   *gppsp = current;
  } else if ((parent->left == current) && (grandparent->right == parent)) {  // zig-zag right
   struct stree *cl = current->left;
   struct stree *cr = current->right;

   current->left = grandparent;
   current->right = parent;

   parent->left = cr;
   grandparent->right = cl;

   if (cr) cr->parent = parent;
   if (cl) cl->parent = grandparent;
   current->parent = grandparent->parent;

   parent->parent = current;
   grandparent->parent = current;

   *gppsp = current;
  } else if ((parent->left == current) && (grandparent->left == parent)) {   // zig-zig left
   struct stree *cr = current->right;
   struct stree *pr = parent->right;

   current->right = parent;
   parent->right = grandparent;

   parent->left = cr;
   grandparent->left = pr;

   if (cr) cr->parent = parent;
   if (pr) pr->parent = grandparent;
   current->parent = grandparent->parent;

   parent->parent = current;
   grandparent->parent = parent;

   *gppsp = current;
  } else if ((parent->right == current) && (grandparent->right == parent)) { // zig-zig right
   struct stree *cl = current->left;
   struct stree *pl = parent->left;

   current->left = parent;
   parent->left = grandparent;

   parent->right = cl;
   grandparent->right = pl;

   if (cl) cl->parent = parent;
   if (pl) pl->parent = grandparent;
   current->parent = grandparent->parent;

   parent->parent = current;
   grandparent->parent = parent;

   *gppsp = current;
  }

  if (parent = current->parent)
   grandparent = parent->parent;
  else
   grandparent = NULL;
 }

 if (current->parent) {
  if (parent->left == current) {  // zig step left
   struct stree *cr = current->right;

   current->right = parent;

   parent->parent = current;
   current->parent = NULL;

   parent->left = cr;
   if (cr) cr->parent = parent;
  } else {                        // zig step left
   struct stree *cl = current->left;

   current->left = parent;

   parent->parent = current;
   current->parent = NULL;

   parent->right = cl;
   if (cl) cl->parent = parent;
  }

  *(current->root) = current;
 }


 return 0;
}

struct stree *streeadd (struct stree *stree, char *key, void *value, int32_t vlen, void *luggage) {
 struct stree *n,
              *rootnode = (stree ? *(stree->root) : NULL),
              *base = (stree ? *(stree->lbase) : NULL);
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
 n->lbase = stree ? stree->lbase : NULL;

 n->next = stree;
 stree = n;

 if (!base)
  stree->lbase = emalloc (sizeof(struct stree *));

 *(stree->lbase) = stree;

 if (!rootnode) {
  stree->root = emalloc (sizeof(struct stree *));
  *(stree->root) = stree;
 } else {
  struct stree *grandparent = NULL, *parent = NULL, *current = rootnode;
  uint32_t cres = 0;

  while (current) {
   grandparent = parent;
   parent = current;
   if ((cres = strcmp (key, current->key)) < 0) { // to the left... (<)
    current = current->left;
   } else { // to the right... (>=)
    current = current->right;
   }
  }

  current = stree;

  if (cres < 0) {
   parent->left = stree;
  } else {
   parent->right = stree;
  }
  stree->parent = parent;

  streesplay(stree, parent, grandparent);
 }

 return stree;
}

struct stree *streedel (struct stree *subject) {
 struct stree *cur = (subject ? *(subject->lbase) : NULL),
              *be = cur;

 if (!cur || !subject) return subject;

/* char tmp[2048];
 snprintf (tmp, 2048, "streedel(): need to remove 0x%zx from 0x%zx", subject, cur);
 puts (tmp);*/

 if (cur == subject) {
  be = cur->next;
  *(subject->lbase) = be;
  if (cur->luggage) free (cur->luggage);

  if (cur->parent) {
   if (cur->parent->left == cur) cur->parent->left = NULL;
   else cur->parent->right = NULL;
  }

  if (cur == *(cur->root)) *(cur->root) = (cur->parent ? cur->parent : (cur->left ? cur->left : cur->right));

  if (cur->left)  cur->left->parent  = cur->parent;
  if (cur->right) cur->right->parent = cur->parent;

  free (cur);
  return be;
 }

 while (cur && (cur->next != subject))
  cur = cur->next;

 if (cur && (cur->next == subject)) {
  cur->next = subject->next;
  if (subject->luggage) free (subject->luggage);

  if (subject->parent) {
   if (subject->parent->left == subject) subject->parent->left = NULL;
   else subject->parent->right = NULL;
  }

  if (subject == *(subject->root)) *(subject->root) = (subject->parent ? subject->parent : (subject->left ? subject->left : subject->right));

  if (subject->left)  subject->left->parent  = subject->parent;
  if (subject->right) subject->right->parent = subject->parent;

  free (subject);
//  return cur;
 }

 return be;
}

struct stree *streefind (struct stree *stree, char *key, char options) {
 struct stree *c;
 char cmp = 0;
 if (!stree || !key) return NULL;

/* char tmp[2048];
 snprintf (tmp, 2048, "streefind(): need to find %s in 0x%zx", key, stree);
 puts (tmp); */

 if (options == TREE_FIND_FIRST) c = *(stree->root);
 else if (options == TREE_FIND_NEXT) {
  if (!strcmp (key, stree->key))
   c = stree->right;
  else
   c = stree;
 }

 while (c && (cmp = strcmp (key, c->key))) {
  if (cmp < 0) c = c->right;
  else c = c->left;
 }

 if (!c) return NULL;

 if (c) streesplay (c, NULL, NULL);

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
