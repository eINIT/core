/*
 *  module-group.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/12/2007.
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

#include <einit/module.h>
#include <einit/set.h>
#include <einit/event.h>
#include <einit/tree.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/bitch.h>

#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_group_configure(struct lmodule *);

const struct smodule module_group_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = einit_module,
    .name = "Module Support (Service Groups)",
    .rid = "einit-module-group",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = module_group_configure
};

module_register(module_group_self);

#define MODULES_PREFIX "services-alias-"
#define MODULES_PREFIX_SIZE (sizeof (MODULES_PREFIX) -1)

enum seq_type {
    sq_any,
    sq_most,
    sq_all
};

int module_group_module_enable(char *nodename, struct einit_event *status)
{
    struct cfgnode *cn = cfg_getnode(nodename);

    if (cn && cn->arbattrs) {
        int i = 0;
        char **group = NULL;
        enum seq_type seq = sq_all;

        for (; cn->arbattrs[i]; i += 2) {
            if (strmatch(cn->arbattrs[i], "group")) {
                group = str2set(':', cn->arbattrs[i + 1]);
            } else if (strmatch(cn->arbattrs[i], "seq")) {
                if (strmatch(cn->arbattrs[i + 1], "any")
                    || strmatch(cn->arbattrs[i + 1], "any")) {
                    seq = sq_any;
                } else if (strmatch(cn->arbattrs[i + 1], "most")) {
                    seq = sq_most;
                } else if (strmatch(cn->arbattrs[i + 1], "all")) {
                    seq = sq_all;
                }
            }
        }

        if (group) {
            if ((seq == sq_all) || !group[1])
                /*
                 * we can bail at this point: if we only had one member,
                 * that one was set as requires=, if we had seq=all, then
                 * all of the members were set as requires=. 
                 */
                return status_ok;

            /*
             * see if any of these are enabled... we used the .uses field
             * and the .after field, so we're able to decide if this
             * worked 
             */
            int enabled = 0;
            for (i = 0; group[i]; i++) {
                if (mod_service_is_provided(group[i]))
                    enabled++;
            }

            efree(group);

            if (enabled) {
                /*
                 * if everything is enabled, then we do know for sure that 
                 * this group is up. 
                 */
                return status_ok;
            }

            return status_failed;
        }
    }

    return status_failed;
}

int module_group_module_disable(char *nodename, struct einit_event *status)
{
    return status_ok;
}

int module_group_module_configure(struct lmodule *tm)
{
    module_init(tm);

    tm->enable =
        (int (*)(void *, struct einit_event *)) module_group_module_enable;
    tm->disable = (int (*)(void *, struct einit_event *))
        module_group_module_disable;

    return 0;
}

void module_group_node_callback(struct cfgnode *node)
{
    if (node && node->arbattrs) {
        int i = 0;
        char **group = NULL, **before = NULL, **after = NULL;
        enum seq_type seq = sq_all;

        for (; node->arbattrs[i]; i += 2) {
            if (strmatch(node->arbattrs[i], "group")) {
                group = str2set(':', node->arbattrs[i + 1]);
            } else if (strmatch(node->arbattrs[i], "seq")) {
                if (strmatch(node->arbattrs[i + 1], "any")
                    || strmatch(node->arbattrs[i + 1], "any")) {
                    seq = sq_any;
                } else if (strmatch(node->arbattrs[i + 1], "most")) {
                    seq = sq_most;
                } else if (strmatch(node->arbattrs[i + 1], "all")) {
                    seq = sq_all;
                }
            } else if (strmatch(node->arbattrs[i], "before")) {
                before = str2set(':', node->arbattrs[i + 1]);
            } else if (strmatch(node->arbattrs[i], "after")) {
                after = str2set(':', node->arbattrs[i + 1]);
            }
        }

        if (group) {
            char **requires = NULL, **provides = NULL, **uses = NULL;
            char t[BUFFERSIZE];

            if ((seq == sq_all) || !group[1]) {
                if (!strmatch(group[0], "none"))
                    requires = set_str_dup_stable(group);
            } else {
                char *member_string = set2str('|', (const char **) group);

                esprintf(t, BUFFERSIZE, "^(%s)$", member_string);

                after = set_str_add(after, t);

                efree(member_string);

                uses = set_str_dup_stable(group);
            }

            provides =
                set_str_add(provides, (node->id + MODULES_PREFIX_SIZE));

            struct smodule *sm = emalloc(sizeof(struct smodule));
            memset(sm, 0, sizeof(struct smodule));

            esprintf(t, BUFFERSIZE, "group-%s",
                     node->id + MODULES_PREFIX_SIZE);
            sm->rid = (char *) str_stabilise(t);
            sm->configure = module_group_module_configure;

            struct lmodule *lm = NULL;

            esprintf(t, BUFFERSIZE, "Group (%s)",
                     node->id + MODULES_PREFIX_SIZE);
            sm->name = (char *) str_stabilise(t);
            sm->si.requires = requires;
            sm->si.provides = provides;
            sm->si.before = before;
            sm->si.after = after;
            sm->si.uses = uses;

            lm = mod_add_or_update(NULL, sm, substitue_and_prune);
            lm->param = (char *) str_stabilise(node->id);
        }
    }
}

int module_group_configure(struct lmodule *tm)
{
    module_init(tm);

    cfg_callback_prefix(MODULES_PREFIX, module_group_node_callback);

    return 0;
}
