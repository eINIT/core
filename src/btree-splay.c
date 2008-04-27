/*
 *  btree-splay.c
 *  einit
 *
 *  Forked by Magnus Deininger from itree-trinary-splay.c on 25/04/2008.
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

#include <einit/utility.h>
#include <einit/btree.h>
#include <string.h>
#include <stddef.h>

struct btree *btree_rotate_left(struct btree *tree)
{
    if (tree->right) {
        struct btree *u, *v, *w;

        u = tree->right;
        v = tree->left;
        w = tree;

        w->right = u->left;
        u->left = w;
        if (w->right)
            w->right->parent = w;

        u->parent = w->parent;
        w->parent = u;

        if (u->parent) {
            if (u->parent->right == w) {
                u->parent->right = u;
            } else if (u->parent->left == w) {
                u->parent->left = u;
            }
        }

        return u;
    }

    return tree;
}

struct btree *btree_rotate_right(struct btree *tree)
{
    if (tree->left) {
        struct btree *u, *v, *w;

        u = tree->left;
        v = tree->right;
        w = tree;

        w->left = u->right;
        u->right = w;
        if (w->left)
            w->left->parent = w;

        u->parent = w->parent;
        w->parent = u;

        if (u->parent) {
            if (u->parent->left == w) {
                u->parent->left = u;
            } else if (u->parent->right == w) {
                u->parent->right = u;
            }
        }

        return u;
    }

    return tree;
}

struct btree *btree_splay(struct btree *tree)
{
    struct btree *rt = tree;

    while (tree->parent) {
        if (tree->parent->left == tree) {
#if 1
            if (tree->parent->parent) {
                if (tree->parent->parent->left == tree->parent) {
                    tree = btree_rotate_right(tree->parent);
                    tree = btree_rotate_right(tree->parent);
                } else {
                    tree = btree_rotate_right(tree->parent);
                    tree = btree_rotate_left(tree->parent);
                }
            } else {
                tree = btree_rotate_right(tree->parent);
            }
#else
            tree = btree_rotate_right(tree->parent);
#endif
        } else if (tree->parent->right == tree) {
#if 1
            if (tree->parent->parent) {
                if (tree->parent->parent->left == tree->parent) {
                    tree = btree_rotate_left(tree->parent);
                    tree = btree_rotate_right(tree->parent);
                } else {
                    tree = btree_rotate_left(tree->parent);
                    tree = btree_rotate_left(tree->parent);
                }
            } else {
                tree = btree_rotate_left(tree->parent);
            }
#else
            tree = btree_rotate_left(tree->parent);
#endif
        } else {
            fprintf(stderr, "BAD TREE!\n");
            fflush(stderr);
            return rt;
        }
    }

    return rt;
}

struct btree *btreeadd(struct btree *tree, signed long key, void *value,
                       ssize_t size)
{
    size_t lsize = sizeof(struct btree);
    struct btree *newnode;
    size_t ssize = 0;

    switch (size) {
    case tree_value_noalloc:
        lsize = sizeof(struct btree);
        break;
    case tree_value_string:
        ssize = strlen(value) + 1;
        lsize = offsetof(struct btree, data) + ssize;
        break;
    default:
        lsize = offsetof(struct btree, data) + size;
        break;
    }

    newnode = emalloc(lsize);
    newnode->key = key;

    switch (size) {
    case tree_value_noalloc:
        newnode->value = value;
        break;
    case tree_value_string:
        memcpy(newnode->data, value, ssize);
        break;
    default:
        memcpy(newnode->data, value, size);
        break;
    }

    newnode->left = NULL;
    newnode->right = NULL;
    newnode->parent = NULL;

    if (tree)
        tree = btreeroot(tree);

    while (tree) {
        if (key < tree->key) {
            newnode->parent = tree;
            tree = tree->left;
        } else {                /* if (key > tree->key) */

            newnode->parent = tree;
            tree = tree->right;
        }
    }

    if (newnode->parent) {
        signed long pkey = newnode->parent->key;

        if (key < pkey)
            newnode->parent->left = newnode;
        else
            newnode->parent->right = newnode;
    }

    return newnode;
}

struct btree *btreefind(struct btree *tree, signed long key)
{
    tree = btreeroot(tree);

    do {
        if (key == tree->key) {
            btree_splay(tree);
            return tree;
        } else if (key < tree->key) {
            tree = tree->left;
        } else {                /* if (key > tree->key) */
            tree = tree->right;
        }
    } while (tree);

    return NULL;
}

struct btree *btreedel(struct btree *tree)
{
    struct btree *t;

    while (tree->left || tree->right) {
        if (tree->right) {
            btree_rotate_left(tree);
        } else {
            btree_rotate_right(tree);
        }
    }

    if (tree->parent) {
        if (tree->parent->left == tree) {
            tree->parent->left = NULL;
        } else {
            tree->parent->right = NULL;
        }

        t = btreeroot(tree);
        efree(tree);
        return t;
    }

    efree(tree);

    return NULL;
}

struct btree *btreeroot(struct btree *tree)
{
    if (tree->parent)
        return btreeroot(tree->parent);

    return tree;
}

void btreefree(struct btree *tree, void (*free_node) (void *))
{
    if (tree) {
        btreefree(tree->left, free_node);
        btreefree(tree->right, free_node);

        if (free_node)
            free_node(tree->value);
        efree(tree);
    }
}

void btreefree_all(struct btree *tree, void (*free_node) (void *))
{
    if (tree);
    tree = btreeroot(tree);
    btreefree(tree, free_node);
}
