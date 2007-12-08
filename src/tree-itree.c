/*
 *  tree-itree.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/12/2007.
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

#include <inttypes.h>
#include <einit/tree.h>
#include <einit/itree.h>
#include <einit/utility.h>

struct stree *streeadd (const struct stree *stree, const char *key, const void *value, int32_t vlen, const void *luggage) {
 if (!key) return NULL;
 signed long keyhash = hashp (key);
 size_t nodesize;
 struct stree *newnode;

 switch (vlen) {
  case SET_NOALLOC:
   nodesize = sizeof (struct stree);
   break;
  case SET_TYPE_STRING:
   vlen = strlen (key) + 1;
  default:
   nodesize = sizeof (struct stree) + vlen;
   break;
 }

 newnode = emalloc (nodesize);
 memset (newnode, 0, nodesize);

 switch (vlen) {
  case SET_NOALLOC:
   newnode->value = value;
   break;
  default:
   memcpy (((char *)newnode) + sizeof (struct stree), value, vlen);
   break;
 }

 newnode->luggage = luggage;

 newnode->treenode = itreeadd (stree ? stree->treenode : NULL, keyhash, newnode, tree_value_noalloc);
 return newnode;
}

struct stree *streedel (struct stree *subject) {
 struct itree *it = itreedel (subject->treenode);
 if (subject->luggage) free (subject->luggage);
 free (subject);

 if (it)
  return it->value;

 return NULL;
}

struct stree *streefind (const struct stree *stree, const char *key, enum tree_search_base options) {
 if (!key || !stree) return NULL;
 signed long keyhash;
 struct itree *it;

 switch (options) {
  case tree_find_next:
   keyhash = stree->treenode->key;
   break;
  default:
   keyhash = hashp (key);
   break;
 }

 it = itreefind (stree->treenode, keyhash, options);
 if (it)
  return it->value;

 return NULL;
}

void streefree (struct stree *stree) {
}

struct stree *streenext (struct stree *stree) {
}
