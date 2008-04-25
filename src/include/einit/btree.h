/*
 *  btree.h
 *  einit
 *
 *  Forked from itree.h by Magnus Deininger on 25/04/2008.
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

#ifndef EINIT_BTREE_SPLAY_H
#define EINIT_BTREE_SPLAY_H

#define tree_value_string 0
#define tree_value_noalloc -1

struct btree {
    struct btree *left, *right, *parent;
    signed long key;
    union {
        void *value;
        char data[0];           /* yeah, this hack is old and dirty... */
    };
};

struct btree *btreeadd(struct btree *tree, signed long key, void *value,
                       ssize_t size);
struct btree *btreefind(struct btree *tree, signed long key);
struct btree *btreedel(struct btree *tree);
struct btree *btreeroot(struct btree *tree);

void btreemap(struct btree *tree, void (*f) (struct btree *, void *),
              void *t);

void btreefree(struct btree *tree, void (*free_node) (void *));
void btreefree_all(struct btree *tree, void (*free_node) (void *));

#define btreefree_simple(tree) btreefree (tree, efree);
#define btreefree_simple_all(tree) btreefree_all (tree, efree);

#endif
