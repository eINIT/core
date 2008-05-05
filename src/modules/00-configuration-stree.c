/*
 *  configuration-stree.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Split from config-xml-expat.c on 22/10/2006
 *  Renamed/moved from config.c on 20/03/2007
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2006-2008, Magnus Deininger All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/event.h>

#include <regex.h>

#define SUPERMODE "global-supermode"

int einit_configuration_stree_configure(struct lmodule *);

const struct smodule einit_configuration_stree_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = 0,
    .name = "Core Configuration Storage and Retrieval (stree-based)",
    .rid = "einit-configuration-stree",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = einit_configuration_stree_configure
};

module_register(einit_configuration_stree_self);

char *cmode = SUPERMODE;

struct {
    void **chunks;
} cfg_stree_garbage = {
.chunks = NULL};

struct stree *configuration_by_mode = NULL;

struct stree *einit_configuration_stree_callbacks = NULL;

void cfg_stree_garbage_add_chunk(void *chunk)
{
    if (!chunk)
        return;
    if (!cfg_stree_garbage.chunks
        ||
        (!inset
         ((const void **) cfg_stree_garbage.chunks, chunk, SET_NOALLOC)))
        cfg_stree_garbage.chunks =
            set_noa_add(cfg_stree_garbage.chunks, chunk);
}

void cfg_stree_garbage_free()
{
    if (cfg_stree_garbage.chunks) {
        int i = 0;

        for (; cfg_stree_garbage.chunks[i]; i++) {
            efree(cfg_stree_garbage.chunks[i]);
        }

        efree(cfg_stree_garbage.chunks);
        cfg_stree_garbage.chunks = NULL;
    }
}

time_t einit_configuration_stree_garbage_free_timer = 0;

void cfg_run_callbacks_for_node(struct cfgnode *node)
{
    if (!node || !node->id)
        return;

    if (einit_configuration_stree_callbacks) {
        struct stree *cur =
            streelinear_prepare(einit_configuration_stree_callbacks);

        while (cur) {
            if (strprefix(node->id, cur->key)) {
                void (*callback) (struct cfgnode *) =
                    (void (*)(struct cfgnode *)) cur->value;

                callback(node);
            }

            cur = streenext(cur);
        }
    }
}

int cfg_free()
{
    /*
     * struct stree *cur = streelinear_prepare(hconfiguration); struct
     * cfgnode *node = NULL; while (cur) { if ((node = (struct cfgnode *)
     * cur->value)) { if (node->id) efree(node->id); } cur =
     * streenext(cur); } streefree(hconfiguration); hconfiguration = NULL;
     */

    return 1;
}

#include <regex.h>

regex_t cfg_storage_allowed_duplicates;

int cfg_addnode(struct cfgnode *node)
{
    char ismode = 0;

    if (!node || !node->id) {
        return -1;
    }

    if (!node->modename)
        node->modename = SUPERMODE;

    if (node->arbattrs) {
        uint32_t r = 0;
        for (; node->arbattrs[r]; r += 2) {
            if (strmatch("id", node->arbattrs[r]))
                node->idattr = node->arbattrs[r + 1];
        }
    }

    if ((ismode = strmatch(node->id, "mode"))) {
        if (!node->idattr)
            return -1;
        node->modename = node->idattr;
    }

    struct stree *mtree = NULL;

    if (configuration_by_mode) {
        mtree =
            streefind(configuration_by_mode, node->modename,
                      tree_find_first);
    }

    if (!mtree) {
        mtree =
            streeadd(NULL, node->id, node, sizeof(struct cfgnode),
                     node->arbattrs);

        configuration_by_mode =
            streeadd(configuration_by_mode, node->modename, mtree,
                     sizeof(struct stree), NULL);

        return 0;
    }

    struct stree *ptree = mtree->value;

    if (strmatch
        (node->id, "core-settings-configuration-multi-node-variables")) {
        if (!node->arbattrs) {
            /*
             * without arbattrs, this node is invalid 
             */
            return -1;
        } else {
            int i = 0;
            for (; node->arbattrs[i]; i += 2) {
                if (strmatch(node->arbattrs[i], "allow")) {
                    // fprintf (stderr, " ** new: %s\n",
                    // node->arbattrs[i+1]);
                    // fflush (stderr);

                    // sleep (1);

                    eregfree(&cfg_storage_allowed_duplicates);
                    if (eregcomp
                        (&cfg_storage_allowed_duplicates,
                         node->arbattrs[i + 1])) {
                        // fprintf (stderr, " ** backup: .*\n");
                        // fflush (stderr);

                        // sleep (1);

                        eregcomp(&cfg_storage_allowed_duplicates, ".*");
                    }
                    // fprintf (stderr, " ** done\n");
                    // fflush (stderr);
                }
            }
        }
    }

    struct stree *cur;
    char doop = 1;

    /*
     * look for other definitions that are exactly the same, only
     * marginally different or that sport a matching id="" attribute 
     */

    cur = streefind(ptree, node->id, tree_find_first);
    while (cur) {
        // this means we found a node wit the same path
        char allow_multi = 0;
        char id_match = 0;
        struct cfgnode *vnode = cur->value;

        // fprintf (stderr, " ** multicheck: %s*\n", node->id);
        if (regexec(&cfg_storage_allowed_duplicates, node->id, 0, NULL, 0)
            != REG_NOMATCH) {
            // fprintf (stderr, "allow multi: %s; %i %i %i\n",
            // node->id, allow_multi, node->idattr ? 1 : 0, id_match);
            allow_multi = 1;
        }
        /*
         * else { fprintf (stderr, " ** not multi*\n"); } fflush
         * (stderr);
         */
        if (cur->value && vnode->idattr && node->idattr
            && strmatch(vnode->idattr, node->idattr)) {
            id_match = 1;
        }

        if (((!allow_multi && !node->idattr) || id_match)) {
            // fprintf (stderr, "overwriting old node: %s\n", node->id);
            // this means we found something that looks like it
            // fprintf (stderr, "replacing old config: %s; %i %i
            // %i\n", node->id, allow_multi, node->idattr ? 1 : 0,
            // id_match);
            // fflush (stderr);

            cfg_stree_garbage_add_chunk(cur->luggage);
            cfg_stree_garbage_add_chunk(vnode->arbattrs);

            vnode->arbattrs = node->arbattrs;
            cur->luggage = node->arbattrs;

            vnode->flag = node->flag;
            vnode->value = node->value;
            vnode->svalue = node->svalue;
            vnode->idattr = node->idattr;

            doop = 0;

            cfg_run_callbacks_for_node(cur->value);

            break;
        } else
            // if (allow_multi || node->idattr) {
            // cur = streenext (cur);
            cur = streefind(cur, node->id, tree_find_next);
        // }
    }

    if (doop) {
        ptree =
            streeadd(ptree, node->id, node, sizeof(struct cfgnode),
                     node->arbattrs);

        einit_new_node = 1;

        cfg_run_callbacks_for_node(node);
    }

    return 0;
}

struct cfgnode *cfg_lookup_x(const char *id, const char **vptr)
{
    struct cfgnode *node = NULL;
    char **sub;
    uint32_t i;

    if (!id) {
        return NULL;
    }

    if (strchr(id, '/')) {
        char f = 0;
        sub = str2set('/', id);

        node = cfg_getnode(sub[0]);
        if (node && node->arbattrs) {
            for (i = 0; node->arbattrs[i]; i += 2) {
                if ((f = (strmatch(node->arbattrs[i], sub[1])))) {
                    (*vptr) = (node->arbattrs[i + 1]);
                    break;
                }
            }
        }

        efree(sub);
    } else {
        return cfg_getnode(id);
    }

    return node;
}

char cfg_getboolean(const char *id)
{
    const char *vptr = NULL;
    struct cfgnode *node = cfg_lookup_x(id, &vptr);

    if (vptr)
        return parse_boolean(vptr);
    if (node)
        return node->flag;
    return 0;
}

int cfg_getinteger(const char *id)
{
    const char *vptr = NULL;
    struct cfgnode *node = cfg_lookup_x(id, &vptr);

    if (vptr)
        return parse_integer(vptr);
    if (node)
        return node->value;
    return 0;
}

// get string (by id)
char *cfg_getstring(const char *id)
{
    const char *vptr = NULL;
    struct cfgnode *node = cfg_lookup_x(id, &vptr);

    if (vptr)
        return (char *) vptr;
    if (node)
        return node->svalue;
    return NULL;
}

// get node (by id)
struct cfgnode *cfg_getnode(const char *id)
{
    if (!id || !configuration_by_mode) {
        return NULL;
    }

    char *modename = cmode;
    struct stree *mtree = NULL;

  retry:

    // fprintf (stderr, "cfg_getnode(%s, %s)\n", id, modename);

    mtree = streefind(configuration_by_mode, modename, tree_find_first);

    if (mtree) {
        struct stree *x = streefind((struct stree *) (mtree->value), id,
                                    tree_find_first);
        if (x)
            return x->value;
    }

    if (!strmatch(modename, SUPERMODE)) {
        modename = SUPERMODE;
        goto retry;
    }

    return NULL;
}

// return a new stree with a certain prefix applied
struct cfgnode **cfg_prefix(const char *prefix)
{
    char *modename = cmode;
    struct stree *mtree = NULL;
    struct cfgnode **retval = NULL;

    if (!configuration_by_mode || !prefix)
        return NULL;

  retry:

    mtree = streefind(configuration_by_mode, modename, tree_find_first);

    if (mtree) {
        struct stree *cur =
            streelinear_prepare((struct stree *) (mtree->value));

        while (cur) {
            if (strprefix(cur->key, prefix)) {
                retval =
                    (struct cfgnode **) set_noa_add((void **) retval,
                                                    cur->value);
            }
            cur = streenext(cur);
        }
    }
    if (!strmatch(modename, SUPERMODE)) {
        modename = SUPERMODE;
        goto retry;
    }

    return retval;
}

// return a new stree with a certain prefix applied
struct cfgnode **cfg_match(const char *name)
{
    if (!configuration_by_mode || !name)
        return NULL;

    char *modename = cmode;
    struct stree *mtree = NULL;
    struct cfgnode **retval = NULL;

  retry:

    mtree = streefind(configuration_by_mode, modename, tree_find_first);

    if (mtree) {
        struct stree *cur =
            streefind((struct stree *) (mtree->value), name,
                      tree_find_first);

        while (cur) {
            retval =
                (struct cfgnode **) set_noa_add((void **) retval,
                                                cur->value);

            cur = streefind(cur, name, tree_find_next);
        }
    }
    if (!strmatch(modename, SUPERMODE)) {
        modename = SUPERMODE;
        goto retry;
    }

    return retval;
}

void
einit_configuration_stree_einit_event_handler_core_configuration_update
(struct einit_event *ev)
{
    // update global environment here
    char **env = einit_global_environment;
    einit_global_environment = NULL;
    if (env)
        efree(env);

    env = NULL;

    struct cfgnode **p = cfg_prefix("configuration-environment-global");
    if (p) {
        struct cfgnode **tcur = p;
        while (*tcur) {
            struct cfgnode *node = *tcur;

            if (node->idattr && node->svalue) {
                env = straddtoenviron(env, node->idattr, node->svalue);
                setenv(node->idattr, node->svalue, 1);
            }

            tcur++;
        }
        efree(p);
    }
    einit_global_environment = env;
}

void cfg_run_callback(char *prefix, void (*callback) (struct cfgnode *))
{
    if (!configuration_by_mode)
        return;

    char *modename = cmode;
    struct stree *mtree = NULL;

  retry:

    mtree = streefind(configuration_by_mode, modename, tree_find_first);

    if (mtree) {
        struct stree *cur =
            streelinear_prepare((struct stree *) (mtree->value));

        while (cur) {
            if (strprefix(cur->key, prefix)) {
                callback((struct cfgnode *) cur->value);
            }

            cur = streenext(cur);
        }
    }
    if (!strmatch(modename, SUPERMODE)) {
        modename = SUPERMODE;
        goto retry;
    }
}

int cfg_callback_prefix(char *prefix, void (*callback) (struct cfgnode *))
{
    if (!prefix || !callback)
        return 0;

    einit_configuration_stree_callbacks =
        streeadd(einit_configuration_stree_callbacks, prefix, callback,
                 tree_value_noalloc, NULL);

    cfg_run_callback(prefix, callback);

    return 1;
}

int einit_configuration_stree_configure(struct lmodule *tm)
{
    module_init(tm);

    event_listen(einit_core_configuration_update,
                 einit_configuration_stree_einit_event_handler_core_configuration_update);

    eregcomp(&cfg_storage_allowed_duplicates, ".*");

    return 0;
}

void cfg_set_current_mode(char *modename)
{
    cmode = (char *) str_stabilise(modename);
    // fprintf (stderr, "cfg_set_current_mode(%s)\n", modename);
}
