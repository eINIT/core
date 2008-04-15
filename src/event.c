/*
 *  event.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 25/06/2006.
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

#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/config.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/bitch.h>
#include <errno.h>
#include <einit/itree.h>
#include <einit/einit.h>
#include <einit/ipc.h>
#include <time.h>
#include <fcntl.h>

struct itree *event_handlers = NULL;

void *event_emit(struct einit_event *event,
                 enum einit_event_emit_flags flags)
{
    if (!event || !event->type)
        return NULL;

    if (flags & einit_event_flag_remote) {
        const char *s = einit_event_encode (event);
        int fd = einit_ipc_get_fd(), r, len = strlen (s);

        fcntl(fd, F_SETFL, 0);

        r = write (fd, s, len);

        fcntl(fd, F_SETFL, O_NONBLOCK);

        if (!r || r < 0) return NULL;
        if (r < len) {
            fprintf (stderr, "BAD EVENT! tried to write %d bytes,"
                             " but only wrote %d bytes\n", len, r);
        }

        return NULL;
    }

    struct event_function **f = NULL;
    uint32_t subsystem = event->type & EVENT_SUBSYSTEM_MASK;

    if (event_handlers) {
        struct itree *it = NULL;

        if (event->type != subsystem) {
            it = itreefind(event_handlers, event->type, tree_find_first);

            while (it) {
                f = (struct event_function **) set_fix_add((void **) f,
                                                           it->data,
                                                           sizeof(struct
                                                                  event_function));

                it = itreefind(it, event->type, tree_find_next);
            }
        }

        it = itreefind(event_handlers, subsystem, tree_find_first);

        while (it) {
            f = (struct event_function **) set_fix_add((void **) f,
                                                       it->data,
                                                       sizeof(struct
                                                              event_function));

            it = itreefind(it, subsystem, tree_find_next);
        }

        it = itreefind(event_handlers, einit_event_subsystem_any,
                       tree_find_first);

        while (it) {
            f = (struct event_function **) set_fix_add((void **) f,
                                                       it->data,
                                                       sizeof(struct
                                                              event_function));

            it = itreefind(it, einit_event_subsystem_any, tree_find_next);
        }
    }

    if (f) {
        int i = 0;
        for (; f[i]; i++) {
            f[i]->handler(event);
        }

        efree(f);
    }

    if (event->chain_type) {
        event->type = event->chain_type;
        event->chain_type = 0;
        event_emit(event, flags);
    }

    return NULL;
}

void event_listen(enum einit_event_subsystems type,
                  void (*handler) (struct einit_event *))
{
    char doadd = 1;

    struct itree *it = event_handlers ? itreefind(event_handlers, type,
                                                  tree_find_first) : NULL;
    while (it) {
        struct event_function *f = (struct event_function *) it->data;

        if (f->handler == handler) {
            doadd = 0;
            break;
        }
        it = itreefind(it, type, tree_find_next);
    }

    if (doadd) {
        struct event_function f = {.handler = handler };
        event_handlers =
            itreeadd(event_handlers, type, &f,
                     sizeof(struct event_function));
    }
}

void event_ignore(enum einit_event_subsystems type,
                  void (*handler) (struct einit_event *))
{
    struct itree *it = NULL;
    if (event_handlers) {
        it = itreefind(event_handlers, type, tree_find_first);

        while (it) {
            struct event_function *f = (struct event_function *) it->data;
            if (f->handler == handler)
                break;

            it = itreefind(it, type, tree_find_next);
        }
    }

    if (it) {
        event_handlers = itreedel(it);
    }

    return;
}

void function_register_type(const char *name, uint32_t version,
                            void const *function, enum function_type type,
                            struct lmodule *module)
{
    if (!name || !function)
        return;

    char added = 0;

    if (module) {
        struct stree *ha = exported_functions;
        ha = streefind(exported_functions, name, tree_find_first);
        while (ha) {
            struct exported_function *ef = ha->value;
            if (ef && (ef->version == version) && (ef->type == type)
                && (ef->module == module)) {
                ef->function = function;
                added = 1;
                break;
            }

            ha = streefind(ha, name, tree_find_next);
        }
    }


    if (!added) {
        struct exported_function *fstruct =
            ecalloc(1, sizeof(struct exported_function));

        fstruct->type = type;
        fstruct->version = version;
        fstruct->function = function;
        fstruct->module = module;

        exported_functions =
            streeadd(exported_functions, name, (void *) fstruct,
                     sizeof(struct exported_function), NULL);

        efree(fstruct);
    }
}

void function_unregister_type(const char *name, uint32_t version,
                              void const *function,
                              enum function_type type,
                              struct lmodule *module)
{
    if (!exported_functions)
        return;
    struct stree *ha = exported_functions;

    ha = streefind(exported_functions, name, tree_find_first);
    while (ha) {
        struct exported_function *ef = ha->value;
        if (ef && (ef->version == version) && (ef->type == type)
            && (ef->module == module)) {
            // exported_functions = streedel (ha);
            ef->function = NULL;
            ha = streefind(exported_functions, name, tree_find_first);
        }

        ha = streefind(ha, name, tree_find_next);
    }

    return;
}

void **function_find(const char *name, const uint32_t version,
                     const char **sub)
{
    if (!exported_functions || !name)
        return NULL;
    void **set = NULL;
    struct stree *ha = exported_functions;

    if (!sub) {
        ha = streefind(exported_functions, name, tree_find_first);
        while (ha) {
            struct exported_function *ef = ha->value;
            if (ef && (ef->version == version))
                set = set_noa_add(set, (void *) ef->function);
            ha = streefind(ha, name, tree_find_next);
        }
    } else {
        uint32_t i = 0, k = strlen(name) + 1;
        char *n = emalloc(k + 1);
        *n = 0;
        strcat(n, name);
        *(n + k - 1) = '-';

        for (; sub[i]; i++) {
            *(n + k) = 0;
            n = erealloc(n, k + 1 + strlen(sub[i]));
            strcat(n, sub[i]);

            ha = streefind(exported_functions, n, tree_find_first);

            while (ha) {

                struct exported_function *ef = ha->value;
                if (ef && (ef->version == version))
                    set = set_noa_add(set, (void *) ef->function);

                ha = streefind(ha, n, tree_find_next);
            }
        }

        if (n)
            efree(n);
    }

    return set;
}

void *function_find_one(const char *name, const uint32_t version,
                        const char **sub)
{
    void **t = function_find(name, version, sub);
    void *f = (t ? t[0] : NULL);

    if (t)
        efree(t);

    return f;
}

struct exported_function **function_look_up(const char *name,
                                            const uint32_t version,
                                            const char **sub)
{
    if (!exported_functions || !name)
        return NULL;
    struct exported_function **set = NULL;
    struct stree *ha = exported_functions;

    if (!sub) {
        ha = streefind(exported_functions, name, tree_find_first);
        while (ha) {
            struct exported_function *ef = ha->value;

            if (!(ef->name))
                ef->name = ha->key;

            if (ef && (ef->version == version))
                set = (struct exported_function **) set_noa_add((void **)
                                                                set,
                                                                (struct
                                                                 exported_function
                                                                 *) ef);
            ha = streefind(ha, name, tree_find_next);
        }
    } else {
        uint32_t i = 0, k = strlen(name) + 1;
        char *n = emalloc(k + 1);
        *n = 0;
        strcat(n, name);
        *(n + k - 1) = '-';

        for (; sub[i]; i++) {
            *(n + k) = 0;
            n = erealloc(n, k + 1 + strlen(sub[i]));
            strcat(n, sub[i]);

            ha = streefind(exported_functions, n, tree_find_first);

            while (ha) {
                struct exported_function *ef = ha->value;

                if (!(ef->name))
                    ef->name = ha->key;

                if (ef && (ef->version == version))
                    set =
                        (struct exported_function **) set_noa_add((void **)
                                                                  set,
                                                                  (struct
                                                                   exported_function
                                                                   *) ef);

                ha = streefind(ha, n, tree_find_next);
            }
        }

        if (n)
            efree(n);
    }

    return set;
}

struct exported_function *function_look_up_one(const char *name,
                                               const uint32_t version,
                                               const char **sub)
{
    struct exported_function **t = function_look_up(name, version, sub);
    struct exported_function *f = (t ? t[0] : NULL);

    if (t)
        efree(t);

    return f;
}

char *event_code_to_string(const uint32_t code)
{
    switch (code) {
    case einit_core_panic:
        return "core/panic";
    case einit_core_service_update:
        return "core/service-update";
    case einit_core_configuration_update:
        return "core/configuration-update";
    case einit_core_module_list_update:
        return "core/module-list-update";
    case einit_core_module_list_update_complete:
        return "core/module-list-update-complete";

    case einit_core_update_configuration:
        return "core/update-configuration";
    case einit_core_change_service_status:
        return "core/change-service-status";
    case einit_core_switch_mode:
        return "core/switch-mode";
    case einit_core_update_modules:
        return "core/update-modules";
    case einit_core_update_module:
        return "core/update-module";
    case einit_core_manipulate_services:
        return "core/manipulate-services";

    case einit_core_mode_switching:
        return "core/mode-switching";
    case einit_core_mode_switch_done:
        return "core/mode-switch-done";
    case einit_core_switching:
        return "core/switching";
    case einit_core_done_switching:
        return "core/done-switching";

    case einit_core_service_enabling:
        return "core/service-enabling";
    case einit_core_service_enabled:
        return "core/service-enabled";
    case einit_core_service_disabling:
        return "core/service-disabling";
    case einit_core_service_disabled:
        return "core/service-disabled";

    case einit_core_module_action_execute:
        return "core/module-action-execute";
    case einit_core_module_action_complete:
        return "core/module-action-complete";

    case einit_core_forked_subprocess:
        return "core/forked-subprocess";

    case einit_mount_do_update:
        return "mount/do-update";
    case einit_mount_node_mounted:
        return "mount/node-mounted";
    case einit_mount_node_unmounted:
        return "mount/node-unmounted";
    case einit_mount_new_mount_level:
        return "mount/new-mount-level";

    case einit_feedback_module_status:
        return "feedback/module-status";
    case einit_feedback_notice:
        return "feedback/notice";

    case einit_feedback_broken_services:
        return "feedback/broken-services";
    case einit_feedback_unresolved_services:
        return "feedback/unresolved-services";

    case einit_feedback_switch_progress:
        return "feedback/switch-progress";

    case einit_power_down_scheduled:
        return "power/down-scheduled";
    case einit_power_down_imminent:
        return "power/down-imminent";
    case einit_power_reset_scheduled:
        return "power/reset-scheduled";
    case einit_power_reset_imminent:
        return "power/reset-imminent";

    case einit_power_failing:
        return "power/failing";
    case einit_power_failure_imminent:
        return "power/failure-imminent";
    case einit_power_restored:
        return "power/restored";

    case einit_power_source_ac:
        return "power/source-ac";
    case einit_power_source_battery:
        return "power/source-battery";

    case einit_power_down_requested:
        return "power/down-requested";
    case einit_power_reset_requested:
        return "power/reset-requested";
    case einit_power_sleep_requested:
        return "power/sleep-requested";
    case einit_power_hibernation_requested:
        return "power/hibernation-requested";

    case einit_timer_tick:
        return "timer/tick";
    case einit_timer_set:
        return "timer/set";
    case einit_timer_cancel:
        return "timer/cancel";

    case einit_network_interface_construct:
        return "network/interface-construct";
    case einit_network_interface_configure:
        return "network/interface-configure";
    case einit_network_interface_update:
        return "network/interface-update";

    case einit_network_interface_prepare:
        return "network/interface-prepare";
    case einit_network_verify_carrier:
        return "network/verify-carrier";
    case einit_network_kill_carrier:
        return "network/kill-carrier";
    case einit_network_address_automatic:
        return "network/address-automatic";
    case einit_network_address_static:
        return "network/address-static";
    case einit_network_interface_done:
        return "network/interface-done";

    case einit_network_interface_cancel:
        return "network/interface-cancel";

    case einit_process_died:
        return "process/died";

    case einit_boot_early:
        return "boot/early";
    case einit_boot_load_kernel_extensions:
        return "boot/load-kernel-extensions";
    case einit_boot_devices_available:
        return "boot/devices-available";
    case einit_boot_root_device_ok:
        return "boot/root-device-ok";

    case einit_hotplug_add:
        return "hotplug/add";
    case einit_hotplug_remove:
        return "hotplug/remove";
    case einit_hotplug_change:
        return "hotplug/change";
    case einit_hotplug_online:
        return "hotplug/online";
    case einit_hotplug_offline:
        return "hotplug/offline";
    case einit_hotplug_move:
        return "hotplug/move";
    case einit_hotplug_generic:
        return "hotplug/generic";

    case einit_laptop_lid_open:
        return "laptop/lid-open";
    case einit_laptop_lid_closed:
        return "laptop/lid-closed";
    }

    switch (code & EVENT_SUBSYSTEM_MASK) {
    case einit_event_subsystem_core:
        return "core/{unknown}";
    case einit_event_subsystem_mount:
        return "mount/{unknown}";
    case einit_event_subsystem_feedback:
        return "feedback/{unknown}";
    case einit_event_subsystem_power:
        return "power/{unknown}";
    case einit_event_subsystem_timer:
        return "timer/{unknown}";

    case einit_event_subsystem_network:
        return "network/{unknown}";
    case einit_event_subsystem_process:
        return "process/{unknown}";
    case einit_event_subsystem_boot:
        return "boot/{unknown}";
    case einit_event_subsystem_hotplug:
        return "hotplug/{unknown}";

    case einit_event_subsystem_laptop:
        return "laptop/{unknown}";

    case einit_event_subsystem_any:
        return "any";
    case einit_event_subsystem_custom:
        return "custom";
    }

    return "unknown/custom";
}

uint32_t event_string_to_code(const char *code)
{
    char **tcode = str2set('/', code);
    uint32_t ret = einit_event_subsystem_custom;

    if (tcode) {
        if (strmatch(tcode[0], "core"))
            ret = einit_event_subsystem_core;
        else if (strmatch(tcode[0], "mount"))
            ret = einit_event_subsystem_mount;
        else if (strmatch(tcode[0], "feedback"))
            ret = einit_event_subsystem_feedback;
        else if (strmatch(tcode[0], "power"))
            ret = einit_event_subsystem_power;
        else if (strmatch(tcode[0], "timer"))
            ret = einit_event_subsystem_timer;
        else if (strmatch(tcode[0], "network"))
            ret = einit_event_subsystem_network;
        else if (strmatch(tcode[0], "process"))
            ret = einit_event_subsystem_process;
        else if (strmatch(tcode[0], "boot"))
            ret = einit_event_subsystem_boot;
        else if (strmatch(tcode[0], "hotplug"))
            ret = einit_event_subsystem_hotplug;
        else if (strmatch(tcode[0], "laptop"))
            ret = einit_event_subsystem_laptop;
        else if (strmatch(tcode[0], "any"))
            ret = einit_event_subsystem_any;
        else if (strmatch(tcode[0], "custom"))
            ret = einit_event_subsystem_custom;

        if (tcode[1])
            switch (ret) {
            case einit_event_subsystem_core:
                if (strmatch(tcode[1], "panic"))
                    ret = einit_core_panic;
                else if (strmatch(tcode[1], "service-update"))
                    ret = einit_core_service_update;
                else if (strmatch(tcode[1], "configuration-update"))
                    ret = einit_core_configuration_update;
                else if (strmatch(tcode[1], "module-list-update"))
                    ret = einit_core_module_list_update;
                else if (strmatch(tcode[1], "module-list-update-complete"))
                    ret = einit_core_module_list_update_complete;

                else if (strmatch(tcode[1], "update-configuration"))
                    ret = einit_core_update_configuration;
                else if (strmatch(tcode[1], "change-service-status"))
                    ret = einit_core_change_service_status;
                else if (strmatch(tcode[1], "switch-mode"))
                    ret = einit_core_switch_mode;
                else if (strmatch(tcode[1], "update-modules"))
                    ret = einit_core_update_modules;
                else if (strmatch(tcode[1], "update-module"))
                    ret = einit_core_update_module;
                else if (strmatch(tcode[1], "manipulate-services"))
                    ret = einit_core_manipulate_services;

                else if (strmatch(tcode[1], "mode-switching"))
                    ret = einit_core_mode_switching;
                else if (strmatch(tcode[1], "mode-switch-done"))
                    ret = einit_core_mode_switch_done;
                else if (strmatch(tcode[1], "switching"))
                    ret = einit_core_switching;
                else if (strmatch(tcode[1], "done-switching"))
                    ret = einit_core_done_switching;

                else if (strmatch(tcode[1], "service-enabling"))
                    ret = einit_core_service_enabling;
                else if (strmatch(tcode[1], "service-enabled"))
                    ret = einit_core_service_enabled;
                else if (strmatch(tcode[1], "service-disabling"))
                    ret = einit_core_service_disabling;
                else if (strmatch(tcode[1], "service-disabled"))
                    ret = einit_core_service_disabled;

                else if (strmatch(tcode[1], "module-action-execute"))
                    ret = einit_core_module_action_execute;
                else if (strmatch(tcode[1], "module-action-complete"))
                    ret = einit_core_module_action_complete;

                else if (strmatch(tcode[1], "forked-subprocess"))
                    ret = einit_core_forked_subprocess;

                break;
            case einit_event_subsystem_mount:
                if (strmatch(tcode[1], "do-update"))
                    ret = einit_mount_do_update;
                else if (strmatch(tcode[1], "node-mounted"))
                    ret = einit_mount_node_mounted;
                else if (strmatch(tcode[1], "node-unmounted"))
                    ret = einit_mount_node_unmounted;
                else if (strmatch(tcode[1], "new-mount-level"))
                    ret = einit_mount_new_mount_level;
                break;
            case einit_event_subsystem_feedback:
                if (strmatch(tcode[1], "module-status"))
                    ret = einit_feedback_module_status;
                else if (strmatch(tcode[1], "notice"))
                    ret = einit_feedback_notice;

                else if (strmatch(tcode[1], "broken-services"))
                    ret = einit_feedback_broken_services;
                else if (strmatch(tcode[1], "unresolved-services"))
                    ret = einit_feedback_unresolved_services;

                else if (strmatch(tcode[1], "switch-progress"))
                    ret = einit_feedback_switch_progress;
                break;
            case einit_event_subsystem_power:
                if (strmatch(tcode[1], "down-scheduled"))
                    ret = einit_power_down_scheduled;
                else if (strmatch(tcode[1], "down-imminent"))
                    ret = einit_power_down_imminent;
                else if (strmatch(tcode[1], "reset-scheduled"))
                    ret = einit_power_reset_scheduled;
                else if (strmatch(tcode[1], "reset-imminent"))
                    ret = einit_power_reset_imminent;

                else if (strmatch(tcode[1], "failing"))
                    ret = einit_power_failing;
                else if (strmatch(tcode[1], "failure-imminent"))
                    ret = einit_power_failure_imminent;
                else if (strmatch(tcode[1], "restored"))
                    ret = einit_power_restored;

                else if (strmatch(tcode[1], "source-ac"))
                    ret = einit_power_source_ac;
                else if (strmatch(tcode[1], "source-battery"))
                    ret = einit_power_source_battery;

                else if (strmatch(tcode[1], "down-requested"))
                    ret = einit_power_down_requested;
                else if (strmatch(tcode[1], "reset-requested"))
                    ret = einit_power_reset_requested;
                else if (strmatch(tcode[1], "sleep-requested"))
                    ret = einit_power_sleep_requested;
                else if (strmatch(tcode[1], "hibernation-requested"))
                    ret = einit_power_hibernation_requested;

                break;
            case einit_event_subsystem_timer:
                if (strmatch(tcode[1], "tick"))
                    ret = einit_timer_tick;
                else if (strmatch(tcode[1], "set"))
                    ret = einit_timer_set;
                else if (strmatch(tcode[1], "cancel"))
                    ret = einit_timer_cancel;
                break;
            case einit_event_subsystem_network:
                if (strmatch(tcode[1], "interface-construct"))
                    ret = einit_network_interface_construct;
                else if (strmatch(tcode[1], "interface-configure"))
                    ret = einit_network_interface_configure;
                else if (strmatch(tcode[1], "interface-update"))
                    ret = einit_network_interface_update;

                else if (strmatch(tcode[1], "interface-prepare"))
                    ret = einit_network_interface_prepare;
                else if (strmatch(tcode[1], "verify-carrier"))
                    ret = einit_network_verify_carrier;
                else if (strmatch(tcode[1], "kill-carrier"))
                    ret = einit_network_kill_carrier;
                else if (strmatch(tcode[1], "address-automatic"))
                    ret = einit_network_address_automatic;
                else if (strmatch(tcode[1], "address-static"))
                    ret = einit_network_address_static;
                else if (strmatch(tcode[1], "interface-done"))
                    ret = einit_network_interface_done;

                else if (strmatch(tcode[1], "interface-cancel"))
                    ret = einit_network_interface_cancel;
                break;
            case einit_event_subsystem_process:
                if (strmatch(tcode[1], "died"))
                    ret = einit_process_died;
                break;
            case einit_event_subsystem_boot:
                if (strmatch(tcode[1], "early"))
                    ret = einit_boot_early;
                else if (strmatch(tcode[1], "load-kernel-extensions"))
                    ret = einit_boot_load_kernel_extensions;
                else if (strmatch(tcode[1], "devices-available"))
                    ret = einit_boot_devices_available;
                else if (strmatch(tcode[1], "root-device-ok"))
                    ret = einit_boot_root_device_ok;
                break;
            case einit_event_subsystem_hotplug:
                if (strmatch(tcode[1], "add"))
                    ret = einit_hotplug_add;
                else if (strmatch(tcode[1], "remove"))
                    ret = einit_hotplug_remove;
                else if (strmatch(tcode[1], "change"))
                    ret = einit_hotplug_change;
                else if (strmatch(tcode[1], "online"))
                    ret = einit_hotplug_online;
                else if (strmatch(tcode[1], "offline"))
                    ret = einit_hotplug_offline;
                else if (strmatch(tcode[1], "move"))
                    ret = einit_hotplug_move;
                else if (strmatch(tcode[1], "generic"))
                    ret = einit_hotplug_generic;
                break;
            case einit_event_subsystem_laptop:
                if (strmatch(tcode[1], "lid-open"))
                    ret = einit_laptop_lid_open;
                else if (strmatch(tcode[1], "lid-closed"))
                    ret = einit_laptop_lid_closed;
                break;
            }

        efree(tcode);
    }

    return ret;
}

time_t event_timer_register_timeout(time_t t)
{
    struct einit_event ev = evstaticinit(einit_timer_set);
    time_t tr = time(NULL) + t;

    ev.integer = tr;

    event_emit(&ev, 0);

    evstaticdestroy(ev);

    return tr;
}

void event_timer_cancel(time_t t)
{
    struct einit_event ev = evstaticinit(einit_timer_cancel);

    ev.integer = t;

    event_emit(&ev, 0);

    evstaticdestroy(ev);
}
