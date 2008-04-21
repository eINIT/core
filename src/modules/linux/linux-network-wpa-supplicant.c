/*
 *  linux-network-wpa-supplicant.c
 *  einit
 *
 *  Created on 04/01/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2008, Magnus Deininger All rights reserved.
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
#include <unistd.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <string.h>

#include <einit-modules/network.h>
#include <einit-modules/exec.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_network_wpa_supplicant_configure(struct lmodule *);

const struct smodule linux_network_wpa_supplicant_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = einit_module,
    .name = "Network Helpers (Linux, WPA Supplicant)",
    .rid = "linux-network-wpa-supplicant",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = linux_network_wpa_supplicant_configure
};

module_register(linux_network_wpa_supplicant_self);

void linux_network_wpa_supplicant_interface_construct(struct einit_event
                                                      *ev)
{
    struct network_event_data *d = ev->para;

    if (strprefix(d->static_descriptor->rid, "interface-carrier-")) {
        struct cfgnode *node =
            d->functions->get_option(ev->string, "wpa-supplicant");
        if (node) {
            char *configuration_file =
                "/etc/wpa_supplicant/wpa_supplicant.conf";
            char *driver = "wext";

            char buffer[BUFFERSIZE];

            int i = 0;

            if (node->arbattrs) {
                for (; node->arbattrs[i]; i += 2) {
                    if (strmatch(node->arbattrs[i], "configuration-file")) {
                        configuration_file = node->arbattrs[i + 1];
                    } else if (strmatch(node->arbattrs[i], "driver")) {
                        driver = node->arbattrs[i + 1];
                    }
                }
            }

            esprintf(buffer, BUFFERSIZE, "wpa-supplicant-%s", ev->string);

            if (!inset
                ((const void **) d->static_descriptor->si.requires, buffer,
                 SET_TYPE_STRING)) {
                d->static_descriptor->si.requires =
                    set_str_add(d->static_descriptor->si.requires, buffer);
            }

            struct cfgnode newnode;

            memset(&newnode, 0, sizeof(struct cfgnode));

            esprintf(buffer, BUFFERSIZE, "configuration-wpa-supplicant-%s",
                     ev->string);
            newnode.id = (char *) str_stabilise(buffer);
            newnode.type = einit_node_regular;

            esprintf(buffer, BUFFERSIZE, "wpa-supplicant-%s", ev->string);
            newnode.arbattrs =
                set_str_add_stable(newnode.arbattrs, (void *) "id");
            newnode.arbattrs =
                set_str_add_stable(newnode.arbattrs, (void *) buffer);

            newnode.arbattrs =
                set_str_add_stable(newnode.arbattrs, (void *) "driver");
            newnode.arbattrs =
                set_str_add_stable(newnode.arbattrs, (void *) driver);

            newnode.arbattrs =
                set_str_add_stable(newnode.arbattrs,
                                   (void *) "configuration-file");
            newnode.arbattrs =
                set_str_add_stable(newnode.arbattrs,
                                   (void *) configuration_file);

            newnode.svalue = newnode.arbattrs[3];

            cfg_addnode(&newnode);
        }
    }
}

char **linux_network_wpa_supplicant_get_as_option_set(char *interface,
                                                      char *wpa_command)
{
    char command[BUFFERSIZE];

    esprintf(command, BUFFERSIZE, "wpa_cli -i%s %s", interface,
             wpa_command);

    char **pr = pget(command);
    char **rv = NULL;

    if (pr) {
        int i = 0;
        for (; pr[i]; i++) {
            char *linebuffer = pr[i];
            if (linebuffer[0]) {
                char *s = strchr(linebuffer, '=');
                if (s) {
                    *s = 0;
                    s++;

                    rv = set_str_add(rv, linebuffer);
                    rv = set_str_add(rv, s);
                }
            }
        }

        efree (pr);
    }

    return rv;
}

void linux_network_wpa_supplicant_verify_carrier(struct einit_event *ev)
{
    struct network_event_data *d = ev->para;

    if (d->functions->get_option(ev->string, "wpa-supplicant")) {
        char **wpa_options = NULL;
        int not_ok = 1;
        int retries = 30;       /* 30 sec timeout... should be enough */

        fbprintf(d->feedback,
                 "making sure wpa_supplicant associated properly");

        while (not_ok && (retries > 0)) {
            if ((wpa_options =
                 linux_network_wpa_supplicant_get_as_option_set(ev->string,
                                                                "status")))
            {
                int i = 0;
                for (; wpa_options[i]; i += 2) {
                    if (strmatch(wpa_options[i], "wpa_state")) {
                        if (strmatch(wpa_options[i + 1], "COMPLETED")) {
                            not_ok = 0;
                        }

                        break;
                    }
                }
            }

            if (not_ok) {
                if (!(retries % 5))
                    fbprintf(d->feedback, "uh-oh!");

                retries--;
                sleep(1);
            }
        }

        if (not_ok) {           /* things didn't go smoothly... */
            fbprintf(d->feedback, "can't seem to associate, giving up");

            d->status = status_failed;
        }
    }
}

#define MPREFIX "configuration-wpa-supplicant-"

int linux_network_wpa_supplicant_module_enable(struct dexecinfo
                                               *interface_daemon,
                                               struct einit_event *status)
{
    if (interface_daemon) {
        return startdaemon(interface_daemon, status);
    }

    return status_failed;
}

int linux_network_wpa_supplicant_module_disable(struct dexecinfo
                                                *interface_daemon,
                                                struct einit_event *status)
{
    if (interface_daemon) {
        return stopdaemon(interface_daemon, status);
    }

    return status_failed;
}

int linux_network_wpa_supplicant_module_configure(struct lmodule *this)
{
    this->enable = (int (*)(void *, struct einit_event *))
        linux_network_wpa_supplicant_module_enable;
    this->disable = (int (*)(void *, struct einit_event *))
        linux_network_wpa_supplicant_module_disable;

    char buffer[BUFFERSIZE];

    esprintf(buffer, BUFFERSIZE, MPREFIX "%s", this->module->rid + 21);

    struct cfgnode *node = cfg_getnode(buffer, NULL);

    if (node) {
        struct dexecinfo *interface_daemon =
            emalloc(sizeof(struct dexecinfo));
        memset(interface_daemon, 0, sizeof(struct dexecinfo));
        char *configuration_file =
            "/etc/wpa_supplicant/wpa_supplicant.conf";
        char *driver = "wext";

        int i = 0;

        if (node->arbattrs) {
            for (; node->arbattrs[i]; i += 2) {
                if (strmatch(node->arbattrs[i], "configuration-file")) {
                    configuration_file = node->arbattrs[i + 1];
                } else if (strmatch(node->arbattrs[i], "driver")) {
                    driver = node->arbattrs[i + 1];
                }
            }
        }

        interface_daemon->id = this->module->rid;

        esprintf(buffer, BUFFERSIZE,
                 "wpa_supplicant -i%s -D%s -C/var/run/wpa_supplicant -c%s",
                 (this->module->rid + 21), driver, configuration_file);

        interface_daemon->command = (char *) str_stabilise(buffer);
        interface_daemon->restart = 1;

        interface_daemon->prepare = NULL;
        interface_daemon->cleanup = NULL;
        interface_daemon->is_up = NULL;
        interface_daemon->is_down = NULL;
        interface_daemon->variables = NULL;
        interface_daemon->user = NULL;
        interface_daemon->group = NULL;
        interface_daemon->cb = NULL;
        interface_daemon->environment = NULL;
        interface_daemon->pidfile = NULL;
        interface_daemon->need_files = NULL;
        interface_daemon->oattrs = NULL;

        this->param = interface_daemon;
    }

    return 0;
}

void linux_network_wpa_supplicant_node_callback(struct cfgnode *node)
{
    char *interface = node->id + sizeof(MPREFIX) - 1;

    char *configuration_file = "/etc/wpa_supplicant/wpa_supplicant.conf";
    char *driver = "wext";

    int i = 0;

    if (node->arbattrs) {
        for (; node->arbattrs[i]; i += 2) {
            if (strmatch(node->arbattrs[i], "configuration-file")) {
                configuration_file = node->arbattrs[i + 1];
            } else if (strmatch(node->arbattrs[i], "driver")) {
                driver = node->arbattrs[i + 1];
            }
        }
    }

    char tmp[BUFFERSIZE];
    struct lmodule *m;
    struct smodule *sm;

    esprintf(tmp, BUFFERSIZE, "linux-wpa-supplicant-%s", interface);

    if ((m = mod_lookup_rid(tmp))) {
        mod_update(m);
        return;
    }

    sm = emalloc(sizeof(struct smodule));
    memset(sm, 0, sizeof(struct smodule));

    sm->rid = (char *) str_stabilise(tmp);

    esprintf(tmp, BUFFERSIZE, "WPA Supplicant Supervisor (%s)", interface);
    sm->name = (char *) str_stabilise(tmp);

    sm->eiversion = EINIT_VERSION;
    sm->eibuild = BUILDNUMBER;
    sm->mode =
        einit_module | einit_feedback_job | einit_module_fork_actions;

    esprintf(tmp, BUFFERSIZE, "wpa-supplicant-%s", interface);
    sm->si.provides = set_str_add(sm->si.provides, tmp);

    /*
     * let's just assume that we'll need /var, /var/run, /usr, /usr/bin,
     * /usr/sbin, /usr/local, /usr/local/bin and /usr/local/sbin 
     */
    sm->si.after =
        set_str_add(sm->si.after,
                    "^fs-(root|var-run|var|usr(-local)?(-s?bin)?)$");

    sm->configure = linux_network_wpa_supplicant_module_configure;

    mod_add(NULL, sm);
}

int linux_network_wpa_supplicant_configure(struct lmodule *pa)
{
    module_init(pa);
    exec_configure(pa);

    event_listen(einit_network_verify_carrier,
                 linux_network_wpa_supplicant_verify_carrier);
    event_listen(einit_network_interface_construct,
                 linux_network_wpa_supplicant_interface_construct);
    event_listen(einit_network_interface_update,
                 linux_network_wpa_supplicant_interface_construct);

    cfg_callback_prefix(MPREFIX,
                        linux_network_wpa_supplicant_node_callback);

    return 0;
}
