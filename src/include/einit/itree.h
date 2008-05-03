/*
 *  itree-trinary-splay.h
 *  einit
 *
 *  Created by Magnus Deininger on 04/12/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2007-2008, Magnus Deininger All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution. *
 * Neither the name of the project nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef EINIT_ITREE_TRINARY_SPLAY_H
#define EINIT_ITREE_TRINARY_SPLAY_H

enum tree_search_base {
    tree_find_first = 0x1,
    tree_find_next = 0x2
};

#define tree_value_noalloc -1

struct itree {
    struct itree *left, *right, *equal, *parent;
    signed long key;
    void *value;
};

struct itree *itreeadd(struct itree *tree, signed long key, void *value,
                       ssize_t size);
struct itree *itreefind(struct itree *tree, signed long key,
                        enum tree_search_base base);
struct itree *itreedel(struct itree *tree);
struct itree *itreedel_by_key(struct itree *tree, signed long key);
struct itree *itreeroot(struct itree *tree);

void itreemap(struct itree *tree, void (*f) (struct itree *, void *),
              void *t);

void itreefree(struct itree *tree, void (*free_node) (void *));
void itreefree_all(struct itree *tree, void (*free_node) (void *));

#define itreefree_simple(tree) itreefree (tree, efree);
#define itreefree_simple_all(tree) itreefree_all (tree, efree);

#endif
