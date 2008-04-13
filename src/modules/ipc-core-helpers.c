/*
 *  ipc-core-helpers.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/03/2007.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/event.h>
#include <errno.h>
#include <string.h>

#include <einit/einit.h>
#include <einit-modules/ipc.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_ipc_core_helpers_configure(struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_ipc_core_helpers_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = 0,
    .name = "IPC Command Library: Core Helpers",
    .rid = "einit-ipc-core-helpers",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = einit_ipc_core_helpers_configure
};

module_register(einit_ipc_core_helpers_self);

#endif

struct lmodule *mlist;

void einit_ipc_core_helpers_ipc_read(struct einit_event *ev)
{
    char **path = ev->para;
    if (path && path[0] && path[1] && path[2] && path[3] && path[4]
        && strmatch(path[0], "services") && (strmatch(path[3], "users")
                                             || strmatch(path[3],
                                                         "modules")
                                             || strmatch(path[3],
                                                         "providers"))) {
        char **pn = set_str_add(NULL, "modules");

        pn = set_str_add(pn, "all");
        int i = 4;
        for (; path[i]; i++) {
            pn = set_str_add(pn, path[i]);
        }

        path = pn;
    }

    struct ipc_fs_node n;

    if (!path) {
        n.name = (char *) str_stabilise("modules");
        n.is_file = 0;
        ev->set = set_fix_add(ev->set, &n, sizeof(n));
        n.name = (char *) str_stabilise("mode");
        n.is_file = 1;
        ev->set = set_fix_add(ev->set, &n, sizeof(n));
    }
    if (path && path[0]) {
        if (strmatch(path[0], "modules")) {
            if (!path[1]) {
                n.name = (char *) str_stabilise("enabled");
                n.is_file = 0;
                ev->set = set_fix_add(ev->set, &n, sizeof(n));
                n.name = (char *) str_stabilise("all");
                n.is_file = 0;
                ev->set = set_fix_add(ev->set, &n, sizeof(n));
                n.name = (char *) str_stabilise("register");
                n.is_file = 1;
                ev->set = set_fix_add(ev->set, &n, sizeof(n));
            } else if (strmatch(path[1], "all") && !path[2]) {
                n.is_file = 0;

                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        n.name = (char *) str_stabilise(cur->module->rid);
                        ev->set = set_fix_add(ev->set, &n, sizeof(n));
                    }

                    cur = cur->next;
                }
            } else if (strmatch(path[1], "enabled") && !path[2]) {
                n.is_file = 0;

                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid
                        && (cur->status & status_enabled)) {
                        n.name = (char *) str_stabilise(cur->module->rid);
                        ev->set = set_fix_add(ev->set, &n, sizeof(n));
                    }

                    cur = cur->next;
                }
            } else if (path[2] && !path[3]) {
                n.is_file = 1;

                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            n.name = (char *) str_stabilise("name");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            n.name = (char *) str_stabilise("status");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            n.name = (char *) str_stabilise("provides");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            n.name = (char *) str_stabilise("requires");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            n.name = (char *) str_stabilise("after");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            n.name = (char *) str_stabilise("before");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            n.name = (char *) str_stabilise("actions");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            n.name = (char *) str_stabilise("options");
                            ev->set = set_fix_add(ev->set, &n, sizeof(n));
                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "status")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (cur->status == status_idle)
                                ev->stringset =
                                    set_str_add(ev->stringset, "idle");
                            else {
                                if (cur->status & status_enabled)
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    "enabled");
                                if (cur->status & status_working)
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    "working");
                                if (cur->status & status_disabled)
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    "disabled");
                            }

                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "options")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (cur->module->mode == 0)
                                ev->stringset =
                                    set_str_add(ev->stringset, "generic");
                            else {
                                if (cur->module->
                                    mode & einit_module_deprecated)
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    "deprecated");
                                if (cur->module->mode & einit_feedback_job)
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    "job-feedback");
                                if (cur->module->
                                    mode & einit_module_event_actions)
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    "event-based");
                            }

                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "name")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            ev->stringset =
                                set_str_add(ev->stringset,
                                            cur->module->name);
                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "provides")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (cur->si && cur->si->provides) {
                                int i = 0;

                                for (; cur->si->provides[i]; i++) {
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    cur->si->provides[i]);
                                }
                            } else {
                                ev->stringset =
                                    set_str_add(ev->stringset, "none");
                            }
                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "requires")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (cur->si && cur->si->requires) {
                                int i = 0;

                                for (; cur->si->requires[i]; i++) {
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    cur->si->requires[i]);
                                }
                            } else {
                                ev->stringset =
                                    set_str_add(ev->stringset, "none");
                            }
                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "before")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (cur->si && cur->si->before) {
                                int i = 0;

                                for (; cur->si->before[i]; i++) {
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    cur->si->before[i]);
                                }
                            } else {
                                ev->stringset =
                                    set_str_add(ev->stringset, "none");
                            }
                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "after")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (cur->si && cur->si->after) {
                                int i = 0;

                                for (; cur->si->after[i]; i++) {
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    cur->si->after[i]);
                                }
                            } else {
                                ev->stringset =
                                    set_str_add(ev->stringset, "none");
                            }
                            break;
                        }
                    }

                    cur = cur->next;
                }
            } else if (path[2] && path[3] && strmatch(path[3], "actions")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (cur->enable) {
                                ev->stringset =
                                    set_str_add(ev->stringset, "enable");
                            }
                            if (cur->disable) {
                                ev->stringset =
                                    set_str_add(ev->stringset, "disable");
                            }
                            if (cur->enable && cur->disable) {
                                ev->stringset =
                                    set_str_add(ev->stringset, "zap");
                            }

                            if (cur->functions) {
                                int i = 0;

                                for (; cur->functions[i]; i++) {
                                    ev->stringset =
                                        set_str_add(ev->stringset,
                                                    cur->functions[i]);
                                }
                            } else if (cur->custom) {
                                ev->stringset =
                                    set_str_add(ev->stringset, "*");
                            }

                            if (!ev->stringset) {
                                ev->stringset =
                                    set_str_add(ev->stringset, "none");
                            }
                            break;
                        }
                    }

                    cur = cur->next;
                }
            }
        } else if (!path[1] && strmatch(path[0], "mode")) {
            if (cmode) {
                if (cmode->idattr) {
                    ev->stringset =
                        set_str_add(ev->stringset, cmode->idattr);
                } else {
                    ev->stringset = set_str_add(ev->stringset, "unknown");
                }
            } else {
                ev->stringset = set_str_add(ev->stringset, "none");
            }
        }
    }

    if (path != ev->para)
        efree(path);
}

void einit_ipc_core_helpers_ipc_write(struct einit_event *ev)
{
    char **path = ev->para;
    if (path && path[0] && path[1] && path[2] && path[3] && path[4]
        && strmatch(path[0], "services") && (strmatch(path[3], "users")
                                             || strmatch(path[3],
                                                         "modules")
                                             || strmatch(path[3],
                                                         "providers"))) {
        char **pn = set_str_add(NULL, "modules");

        pn = set_str_add(pn, "all");
        int i = 4;
        for (; path[i]; i++) {
            pn = set_str_add(pn, path[i]);
        }

        path = pn;
    }

    if (path && ev->set && ev->set[0] && path[0]) {
        if (strmatch(path[0], "mode")) {
            struct einit_event ee = evstaticinit(einit_core_switch_mode);
            ee.string = (char *) str_stabilise(ev->set[0]);
            event_emit(&ee, 0);
            evstaticdestroy(ee);
        } else if (path[1] && strmatch(path[0], "modules")) {
            if (strmatch(path[1], "register")) {
                /*
                 * decode the new module's data here 
                 */
                mod_add_or_update(NULL,
                                  einit_decode_module_from_string(ev->
                                                                  set[0]),
                                  substitue_and_prune);
            } else if (path[2] && path[3] && strmatch(path[3], "status")) {
                struct lmodule *cur = mlist;

                while (cur) {
                    if (cur->module && cur->module->rid) {
                        if (strmatch(path[2], cur->module->rid)) {
                            if (strmatch(ev->set[0], "enable")) {
                                mod(einit_module_enable, cur, NULL);
                            } else if (strmatch(ev->set[0], "disable")) {
                                mod(einit_module_disable, cur, NULL);
                            } else {
                                mod(einit_module_custom, cur,
                                    (char *) str_stabilise(ev->set[0]));
                            }

                            break;
                        }
                    }

                    cur = cur->next;
                }
            }
        }
    }

    if (path != ev->para)
        efree(path);
}

void einit_ipc_core_helpers_ipc_stat(struct einit_event *ev)
{
    char **path = ev->para;
    if (path && path[0] && path[1] && path[2] && path[3] && path[4]
        && strmatch(path[0], "services") && (strmatch(path[3], "users")
                                             || strmatch(path[3],
                                                         "modules")
                                             || strmatch(path[3],
                                                         "providers"))) {
        char **pn = set_str_add(NULL, "modules");

        pn = set_str_add(pn, "all");
        int i = 4;
        for (; path[i]; i++) {
            pn = set_str_add(pn, path[i]);
        }

        path = pn;
    }

    if (path && path[0]) {
        if (strmatch(path[0], "modules")) {
            ev->flag = (path[1]
                        && (strmatch(path[1], "register")
                            || (path[2] && path[3])) ? 1 : 0);
        } else if (!path[1] && strmatch(path[0], "mode")) {
            ev->flag = 1;
        }
    }

    if (path != ev->para)
        efree(path);
}

int einit_ipc_core_helpers_configure(struct lmodule *r)
{
    module_init(r);

    event_listen(einit_ipc_read, einit_ipc_core_helpers_ipc_read);
    event_listen(einit_ipc_stat, einit_ipc_core_helpers_ipc_stat);
    event_listen(einit_ipc_write, einit_ipc_core_helpers_ipc_write);

    return 0;
}

/*
 * passive module, no enable/disable/etc 
 */
