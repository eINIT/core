/*
 *  itree-trinary-splay.c
 *  einit
 *
 *  Created by Magnus Deininger on 04/12/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

#include <einit/utility.h>
#include <einit/itree.h>

struct itree *itree_splay (struct itree *tree) {
}

struct itree *itreeadd (struct itree *tree, signed long key, void *value, ssize_t size) {
}

struct itree *itreefind (struct itree *tree, signed long key, enum tree_search_base base) {
 if (base == tree_find_first)
  tree = itreebase (tree);

 do {
  if (key == tree->key) {
   if (base == tree_find_first) {
    return tree;
   } else {
    return tree->equal;
   }
  } else if (key < tree->key) {
   tree = tree->left;
  } else /* if (key > tree->key) */ {
   tree = tree->right;
  }
 } while (tree);
 return NULL;
}

struct itree *itreedel (struct itree *tree) {
}

struct itree *itreedel_by_key (struct itree *tree, signed long key) {
 struct itree *it = itreefind (it, tree->key, tree_find_first);

 while (it) {
  struct itree *n = it->equal;
  itreedel (n);
  it = n;
 }
}

struct itree *itreebase (struct itree *tree) {
 if (tree->parent)
  return itreebase (tree->parent);

 return tree;
}

void itreefree (struct itree *tree, void (*free_node)(void *)) {
 itreefree (tree->left, free_node);
 itreefree (tree->right, free_node);
 itreefree (tree->equal, free_node);

 if (free_node) free_node (tree->value);
 efree (tree);
}

void itreefree_all (struct itree *tree, void (*free_node)(void *)) {
 tree = itreebase (tree);
 itreefree (tree, free_node);
}
