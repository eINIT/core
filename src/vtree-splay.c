/*
 *  vtree-splay.c
 *  einit
 *
 *  Forked by Magnus Deininger from btree-splay.c on 26/04/2008.
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
#include <einit/vtree.h>
#include <string.h>
#include <stddef.h>

struct vtree *vtree_rotate_left(struct vtree *tree)
{
    if (tree->right) {
        struct vtree *u, *v, *w;

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

struct vtree *vtree_rotate_right(struct vtree *tree)
{
    if (tree->left) {
        struct vtree *u, *v, *w;

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

struct vtree *vtree_splay(struct vtree *tree)
{
    struct vtree *rt = tree;

    while (tree->parent) {
        if (tree->parent->left == tree) {
#if 1
            if (tree->parent->parent) {
                if (tree->parent->parent->left == tree->parent) {
                    tree = vtree_rotate_right(tree->parent);
                    tree = vtree_rotate_right(tree->parent);
                } else {
                    tree = vtree_rotate_right(tree->parent);
                    tree = vtree_rotate_left(tree->parent);
                }
            } else {
                tree = vtree_rotate_right(tree->parent);
            }
#else
            tree = vtree_rotate_right(tree->parent);
#endif
        } else if (tree->parent->right == tree) {
#if 1
            if (tree->parent->parent) {
                if (tree->parent->parent->left == tree->parent) {
                    tree = vtree_rotate_left(tree->parent);
                    tree = vtree_rotate_right(tree->parent);
                } else {
                    tree = vtree_rotate_left(tree->parent);
                    tree = vtree_rotate_left(tree->parent);
                }
            } else {
                tree = vtree_rotate_left(tree->parent);
            }
#else
            tree = vtree_rotate_left(tree->parent);
#endif
        } else {
            fprintf(stderr, "BAD TREE!\n");
            fflush(stderr);
            return rt;
        }
    }

    return rt;
}

struct vtree *vtreeadd(struct vtree *tree, signed long key)
{
    struct vtree *newnode;

    newnode = emalloc(sizeof(struct vtree));
    newnode->key = key;

    newnode->left = NULL;
    newnode->right = NULL;
    newnode->parent = NULL;

    if (tree)
        tree = vtreeroot(tree);

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

struct vtree *vtreefind(struct vtree *tree, signed long key)
{
    tree = vtreeroot(tree);

    do {
        if (key == tree->key) {
            vtree_splay(tree);
            return tree;
        } else if (key < tree->key) {
            tree = tree->left;
        } else {                /* if (key > tree->key) */
            tree = tree->right;
        }
    } while (tree);

    return NULL;
}

struct vtree *vtreedel(struct vtree *tree)
{
    struct vtree *t;

    while (tree->left || tree->right) {
        if (tree->right) {
            vtree_rotate_left(tree);
        } else {
            vtree_rotate_right(tree);
        }
    }

    if (tree->parent) {
        if (tree->parent->left == tree) {
            tree->parent->left = NULL;
        } else {
            tree->parent->right = NULL;
        }

        t = vtreeroot(tree);
        efree(tree);
        return t;
    }

    efree(tree);

    return NULL;
}

struct vtree *vtreeroot(struct vtree *tree)
{
    if (tree->parent)
        return vtreeroot(tree->parent);

    return tree;
}

void vtreefree(struct vtree *tree)
{
    if (tree) {
        vtreefree(tree->left);
        vtreefree(tree->right);
        efree(tree);
    }
}

void vtreefree_all(struct vtree *tree)
{
    if (tree);
    tree = vtreeroot(tree);
    vtreefree(tree);
}
