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

#define _BSD_SOURCE

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
#include <einit-modules/ipc.h>

#include <regex.h>

int einit_configuration_stree_configure(struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
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

#endif

struct {
    void **chunks;
} cfg_stree_garbage = {
.chunks = NULL};

struct stree *hconfiguration = NULL;

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
    struct stree *cur = streelinear_prepare(hconfiguration);
    struct cfgnode *node = NULL;
    while (cur) {
        if ((node = (struct cfgnode *) cur->value)) {
            if (node->id)
                efree(node->id);
        }
        cur = streenext(cur);
    }
    streefree(hconfiguration);
    hconfiguration = NULL;

    return 1;
}

#include <regex.h>

regex_t cfg_storage_allowed_duplicates;

int cfg_addnode_f(struct cfgnode *node)
{
    if (!node || !node->id) {
        return -1;
    }

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

    struct stree *cur = hconfiguration;
    char doop = 1;

    if (node->arbattrs) {
        uint32_t r = 0;
        for (; node->arbattrs[r]; r += 2) {
            if (strmatch("id", node->arbattrs[r]))
                node->idattr = node->arbattrs[r + 1];
        }
    }

    if (node->type & einit_node_mode) {
        /*
         * mode definitions only need to be modified -- it doesn't matter
         * if there's more than one, but only the first one would be used
         * anyway. 
         */
        if (cur)
            cur = streefind(cur, node->id, tree_find_first);
        while (cur) {
            if (cur->value
                && !(((struct cfgnode *) cur->value)->
                     type ^ einit_node_mode)) {
                // this means we found something that looks like it
                void *bsl = cur->luggage;

                // we risk not being atomic at this point but... it really 
                // 
                // 
                // 
                // 
                // 
                // 
                // 
                // is unlikely to go weird.
                ((struct cfgnode *) cur->value)->arbattrs = node->arbattrs;
                cur->luggage = node->arbattrs;

                efree(bsl);

                doop = 0;

                break;
            }
            // cur = streenext (cur);
            cur = streefind(cur, node->id, tree_find_next);
        }
    } else {
        /*
         * look for other definitions that are exactly the same, only
         * marginally different or that sport a matching id="" attribute 
         */

        if (cur)
            cur = streefind(cur, node->id, tree_find_first);
        while (cur) {
            // this means we found a node wit the same path
            char allow_multi = 0;
            char id_match = 0;

            if ((((struct cfgnode *) cur->value)->mode != node->mode)) {
                cur = streefind(cur, node->id, tree_find_next);
                continue;
            }
            // fprintf (stderr, " ** multicheck: %s*\n", node->id);
            if (regexec
                (&cfg_storage_allowed_duplicates, node->id, 0, NULL,
                 0) != REG_NOMATCH) {
                // fprintf (stderr, "allow multi: %s; %i %i %i\n",
                // node->id, allow_multi, node->idattr ? 1 : 0, id_match);
                allow_multi = 1;
            }
            /*
             * else { fprintf (stderr, " ** not multi*\n"); } fflush
             * (stderr);
             */
            if (cur->value && ((struct cfgnode *) cur->value)->idattr
                && node->idattr
                && strmatch(((struct cfgnode *) cur->value)->idattr,
                            node->idattr)) {
                id_match = 1;
            }

            if (((!allow_multi && (!node->idattr)) || id_match)) {
                // this means we found something that looks like it
                // fprintf (stderr, "replacing old config: %s; %i %i
                // %i\n", node->id, allow_multi, node->idattr ? 1 : 0,
                // id_match);
                // fflush (stderr);

                cfg_stree_garbage_add_chunk(cur->luggage);
                cfg_stree_garbage_add_chunk(((struct cfgnode *) cur->
                                             value)->arbattrs);

                ((struct cfgnode *) cur->value)->arbattrs = node->arbattrs;
                cur->luggage = node->arbattrs;

                ((struct cfgnode *) cur->value)->type = node->type;
                ((struct cfgnode *) cur->value)->mode = node->mode;
                ((struct cfgnode *) cur->value)->flag = node->flag;
                ((struct cfgnode *) cur->value)->value = node->value;
                ((struct cfgnode *) cur->value)->svalue = node->svalue;
                ((struct cfgnode *) cur->value)->idattr = node->idattr;

                doop = 0;

                cfg_run_callbacks_for_node(cur->value);

                break;
            } else
                // if (allow_multi || node->idattr) {
                // cur = streenext (cur);
                cur = streefind(cur, node->id, tree_find_next);
            // }
        }
    }

    if (doop) {
        hconfiguration =
            streeadd(hconfiguration, node->id, node,
                     sizeof(struct cfgnode), node->arbattrs);

        einit_new_node = 1;

        cfg_run_callbacks_for_node(node);
    }

    /*
     * hmmm.... 
     */
    /*
     * cfg_stree_garbage_add_chunk (node->arbattrs);
     */
    cfg_stree_garbage_add_chunk(node->id);
    return 0;
}

struct cfgnode *cfg_findnode_f(const char *id,
                               enum einit_cfg_node_options type,
                               const struct cfgnode *base)
{
    struct stree *cur = hconfiguration;
    if (!id) {
        return NULL;
    }

    if (base) {
        if (cur)
            cur = streefind(cur, id, tree_find_first);
        while (cur) {
            if (cur->value == base) {
                cur = streefind(cur, id, tree_find_next);
                break;
            }
            // cur = streenext (cur);
            cur = streefind(cur, id, tree_find_next);
        }
    } else if (cur) {
        cur = streefind(cur, id, tree_find_first);
    }

    while (cur) {
        if (cur->value
            && (!type
                || !(((struct cfgnode *) cur->value)->type ^ type))) {
            return cur->value;
        }
        cur = streefind(cur, id, tree_find_next);
    }
    return NULL;
}

// get string (by id)
char *cfg_getstring_f(const char *id, const struct cfgnode *mode)
{
    struct cfgnode *node = NULL;
    char *ret = NULL, **sub;
    uint32_t i;

    if (!id) {
        return NULL;
    }
    mode = mode ? mode : cmode;

    if (strchr(id, '/')) {
        char f = 0;
        sub = str2set('/', id);
        if (!sub[1]) {
            node = cfg_getnode(id, mode);
            if (node)
                ret = node->svalue;

            efree(sub);
            return ret;
        }

        node = cfg_getnode(sub[0], mode);
        if (node && node->arbattrs && node->arbattrs[0]) {
            if (node->arbattrs)

                for (i = 0; node->arbattrs[i]; i += 2) {
                    if ((f = (strmatch(node->arbattrs[i], sub[1])))) {
                        ret = node->arbattrs[i + 1];
                        break;
                    }
                }
        }

        efree(sub);
    } else {
        node = cfg_getnode(id, mode);
        if (node)
            ret = node->svalue;
    }

    return ret;
}

// get node (by id)
struct cfgnode *cfg_getnode_f(const char *id, const struct cfgnode *mode)
{
    struct cfgnode *node = NULL;
    struct cfgnode *ret = NULL;

    if (!id) {
        return NULL;
    }
    mode = mode ? mode : cmode;

    if (mode) {
        char *tmpnodename = NULL;
        tmpnodename = emalloc(6 + strlen(id));
        *tmpnodename = 0;

        strcat(tmpnodename, "mode-");
        strcat(tmpnodename, id);

        while ((node = cfg_findnode(tmpnodename, 0, node))) {
            if (node->mode == mode) {
                ret = node;
                break;
            }
        }

        efree(tmpnodename);
    }

    if (!ret && (node = cfg_findnode(id, 0, NULL)))
        ret = node;

    return ret;
}

// return a new stree with a certain prefix applied
struct stree *cfg_prefix_f(const char *prefix)
{
    struct stree *retval = NULL;

    if (prefix) {
        struct stree *cur = streelinear_prepare(hconfiguration);
        while (cur) {
            if (strprefix(cur->key, prefix)) {
                retval =
                    streeadd(retval, cur->key, cur->value, SET_NOALLOC,
                             NULL);
            }
            cur = streenext(cur);
        }
    }

    return retval;
}

/*
 * those i-could've-sworn-there-were-library-functions-for-that functions 
 */
char *cfg_getpath_f(const char *id)
{
    int mplen;
    char *svpath = cfg_getstring(id, NULL);
    if (!svpath) {
        return NULL;
    }
    mplen = strlen(svpath) + 1;
    if (svpath[mplen - 2] != '/') {
        // if (svpath->path) return svpath->path;
        char *tmpsvpath = (char *) emalloc(mplen + 1);
        tmpsvpath[0] = 0;

        strcat(tmpsvpath, svpath);
        tmpsvpath[mplen - 1] = '/';
        tmpsvpath[mplen] = 0;
        // svpath->svalue = tmpsvpath;
        // svpath->path = tmpsvpath;
        return tmpsvpath;
    }
    return svpath;
}

void einit_configuration_stree_einit_event_handler_core_configuration_update(struct einit_event *ev) {
    // update global environment here
    char **env = einit_global_environment;
    einit_global_environment = NULL;
    struct cfgnode *node = NULL;
    efree(env);

    env = NULL;
    while ((node =
            cfg_findnode("configuration-environment-global", 0, node))) {
        if (node->idattr && node->svalue) {
            env = straddtoenviron(env, node->idattr, node->svalue);
            setenv(node->idattr, node->svalue, 1);
        }
    }
    einit_global_environment = env;
}

void cfg_run_callback(char *prefix, void (*callback) (struct cfgnode *))
{
    if (hconfiguration) {
        struct stree *cur = streelinear_prepare(hconfiguration);

        while (cur) {
            if (strprefix(cur->key, prefix)) {
                callback((struct cfgnode *) cur->value);
            }

            cur = streenext(cur);
        }
    }
}

int cfg_callback_prefix_f(char *prefix,
                          void (*callback) (struct cfgnode *))
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

    function_register("einit-configuration-node-add", 1, cfg_addnode_f);
    function_register("einit-configuration-node-get", 1, cfg_getnode_f);
    function_register("einit-configuration-node-get-string", 1,
                      cfg_getstring_f);
    function_register("einit-configuration-node-get-find", 1,
                      cfg_findnode_f);
    function_register("einit-configuration-node-get-path", 1,
                      cfg_getpath_f);
    function_register("einit-configuration-node-get-prefix", 1,
                      cfg_prefix_f);

    function_register("einit-configuration-callback-prefix", 1,
                      cfg_callback_prefix_f);

    eregcomp(&cfg_storage_allowed_duplicates, ".*");

    return 0;
}
