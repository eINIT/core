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
#include <string.h>

struct itree *itree_rotate_left (struct itree *tree) {
 if (tree->left) {
  struct itree *u, *v, *w;
 }

 return tree;
}

struct itree *itree_rotate_right (struct itree *tree) {
 return tree;
}

struct itree *itree_splay (struct itree *tree) {
 struct itree *rt = tree;

 while (tree->parent) {
  if (tree->parent->equal == tree) {
   tree = tree->parent;
  } else if (tree->parent->left == tree) {
   if (tree->parent->parent) {
   }
  } else if (tree->parent->right == tree) {
   if (tree->parent->parent) {
   }
  }
 }

 return rt;
}

#define itree_splay(t) t

struct itree *itreeadd (struct itree *tree, signed long key, void *value, ssize_t size) {
 size_t lsize = sizeof (struct itree);
 struct itree *newnode;
 size_t ssize;

 switch (size) {
  case tree_value_noalloc:
   lsize = sizeof (struct itree);
   break;
  case tree_value_string:
   ssize = strlen (value) + 1;
   lsize = sizeof (struct itree) + ssize;
   break;
  default:
   lsize = sizeof (struct itree) + size;
   break;
 }

 newnode = emalloc (lsize);
 newnode->key = key;

 switch (size) {
  case tree_value_noalloc:
   newnode->value = value;
   break;
  case tree_value_string:
   newnode->value = ((char *)newnode) + sizeof (struct itree);
   memcpy (newnode->value, value, ssize);
   break;
  default:
   newnode->value = ((char *)newnode) + sizeof (struct itree);
   memcpy (newnode->value, value, size);
   break;
 }

 newnode->left = NULL;
 newnode->right = NULL;
 newnode->equal = NULL;
 newnode->parent = NULL;

 while (tree) {
  if (key == tree->key) {
   tree->parent = newnode;
   newnode->equal = tree;

   if (tree->left) {
    newnode->left = tree->left;
    tree->left = NULL;
    newnode->left->parent = newnode;
   }
   if (tree->right) {
    newnode->right = tree->right;
    tree->right = NULL;
    newnode->right->parent = newnode;
   }
   if (newnode->parent) {
    if (newnode->parent->left == tree) {
     newnode->parent->left = newnode;
	}
    if (newnode->parent->right == tree) {
     newnode->parent->right = newnode;
	}
   }

   return itreebase(newnode);
  } else if (key < tree->key) {
   newnode->parent = tree;
   tree = tree->left;
  } else /* if (key > tree->key) */ {
   newnode->parent = tree;
   tree = tree->right;
  }
 }

 if (newnode->parent) {
  signed long pkey = newnode->parent->key;

  if (key < pkey) newnode->parent->left = newnode;
  else newnode->parent->right = newnode;
 }

 return itreebase(newnode);
}

struct itree *itreefind (struct itree *tree, signed long key, enum tree_search_base base) {
 if (base == tree_find_first)
  tree = itreebase (tree);

 do {
  if (key == tree->key) {
   if (base == tree_find_first) {
    return itree_splay (tree);
   } else {
    return itree_splay (tree->equal);
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
 return tree;
}

struct itree *itreedel_by_key (struct itree *tree, signed long key) {
 struct itree *it = itreefind (it, tree->key, tree_find_first);

#if 0
 while (it) {
  struct itree *n = it->equal;
  if (n->parent) {
   tree = itreebase (n);
  } else {

  }
  tree = itreedel (n);
  it = ;
 }
#endif

 return tree;
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

#ifdef DEBUG
void itreedump (struct itree *tree) {
 static int indent = 0;

 if (tree) {
  int i = 0;
  for (; i < indent; i++) {
   fprintf (stderr, " ");
  }

  if (tree->parent) {
   if (tree->parent->left == tree) {
    fprintf (stderr, "(left) ");
   } else if (tree->parent->right == tree) {
    fprintf (stderr, "(right) ");
   } else if (tree->parent->equal == tree) {
    fprintf (stderr, "(equal) ");
   } else {
    fprintf (stderr, "(?) ");
   }
  }
  fprintf (stderr, "node: key=%i, value=%i\n", tree->key, (int)tree->value);

  fflush (stderr);

  if (tree->left) {
   indent++;
   itreedump (tree->left);
   indent--;
  }
  if (tree->right) {
   indent++;
   itreedump (tree->right);
   indent--;
  }
  if (tree->equal) {
   indent++;
   itreedump (tree->equal);
   indent--;
  }
 }
}
#endif