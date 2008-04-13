/*
 *  event.h
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EINIT_EVENT_H
#define EINIT_EVENT_H

#include <stdio.h>
#include <inttypes.h>
#include <einit/tree.h>

#define EVENT_SUBSYSTEM_MASK           0xfffff000
#define EVENT_CODE_MASK                0x00000fff

    enum einit_event_emit_flags {
        einit_event_flag_remote = 0x10
            /*
             * !< spawn the event in a remote core... only use this if you 
             * know you need it 
             */
    };

    enum einit_event_subsystems {
        einit_event_subsystem_core = 0x00001000,
        einit_event_subsystem_mount = 0x00003000,
        /*
         * !< update mount status 
         */
        einit_event_subsystem_feedback = 0x00004000,
        einit_event_subsystem_power = 0x00005000,
        /*
         * !< notify others that the power is failing, has been restored
         * or similar 
         */
        einit_event_subsystem_timer = 0x00006000,
        /*
         * !< set/receive timer ticks 
         */
        einit_event_subsystem_network = 0x00007000,
        einit_event_subsystem_process = 0x00008000,
        einit_event_subsystem_boot = 0x00009000,
        einit_event_subsystem_hotplug = 0x0000a000,
        einit_event_subsystem_ipc_v2 = 0x0000b000,
        einit_event_subsystem_laptop = 0x0000c000,

        einit_event_subsystem_any = 0xffffe000,
        /*
         * !< match any subsystem... mostly intended to be used for
         * rebroadcasting, e.g. via D-Bus 
         */
        einit_event_subsystem_custom = 0xfffff000
            /*
             * !< custom events; not yet implemented 
             */
    };

    enum einit_event_code {
        /*
         * einit_event_subsystem_core: 
         */
        einit_core_panic = einit_event_subsystem_core | 0x001,
        /*
         * !< put everyone in the cast range into a state of panic/calm
         * everyone down; status contains a reason 
         */
        einit_core_service_update = einit_event_subsystem_core | 0x003,
        /*
         * !< Service availability changing; use the task and status
         * fields to find out what happened 
         */
        einit_core_configuration_update =
            einit_event_subsystem_core | 0x004,
        /*
         * !< notification of configuration update 
         */
        einit_core_module_list_update = einit_event_subsystem_core | 0x006,
        /*
         * !< notification of module-list updates 
         */
        einit_core_module_list_update_complete =
            einit_event_subsystem_core | 0x007,

        einit_core_update_configuration =
            einit_event_subsystem_core | 0x101,
        /*
         * !< update the configuration 
         */
        einit_core_change_service_status =
            einit_event_subsystem_core | 0x102,
        /*
         * !< change status of a service 
         */
        einit_core_switch_mode = einit_event_subsystem_core | 0x103,
        /*
         * !< switch to a different mode 
         */
        einit_core_update_modules = einit_event_subsystem_core | 0x104,
        /*
         * !< update the modules 
         */
        einit_core_update_module = einit_event_subsystem_core | 0x105,
        /*
         * !< update this module (in ->para) 
         */
        einit_core_manipulate_services =
            einit_event_subsystem_core | 0x106,

        einit_core_mode_switching = einit_event_subsystem_core | 0x201,
        einit_core_mode_switch_done = einit_event_subsystem_core | 0x202,
        einit_core_switching = einit_event_subsystem_core | 0x203,
        einit_core_done_switching = einit_event_subsystem_core | 0x204,

        einit_core_service_enabling = einit_event_subsystem_core | 0x501,
        einit_core_service_enabled = einit_event_subsystem_core | 0x502,
        einit_core_service_disabling = einit_event_subsystem_core | 0x503,
        einit_core_service_disabled = einit_event_subsystem_core | 0x504,

        einit_core_module_action_execute =
            einit_event_subsystem_core | 0x601,
        einit_core_module_action_complete =
            einit_event_subsystem_core | 0x602,

        einit_core_forked_subprocess = einit_event_subsystem_core | 0x701,

        /*
         * einit_event_subsystem_mount: 
         */
        einit_mount_do_update = einit_event_subsystem_mount | 0x001,
        einit_mount_node_mounted = einit_event_subsystem_mount | 0x011,
        einit_mount_node_unmounted = einit_event_subsystem_mount | 0x012,
        einit_mount_new_mount_level = einit_event_subsystem_mount | 0x021,

        /*
         * einit_event_subsystem_feedback: 
         */
        einit_feedback_module_status =
            einit_event_subsystem_feedback | 0x001,
        /*
         * !< the para field specifies a module that caused the feedback 
         */
        einit_feedback_notice = einit_event_subsystem_feedback | 0x003,

        einit_feedback_broken_services =
            einit_event_subsystem_feedback | 0x021,
        einit_feedback_unresolved_services =
            einit_event_subsystem_feedback | 0x022,
        einit_feedback_switch_progress =
            einit_event_subsystem_feedback | 0x030,

        /*
         * einit_event_subsystem_power: 
         */
        einit_power_down_scheduled = einit_event_subsystem_power | 0x001,
        /*
         * !< shutdown scheduled 
         */
        einit_power_down_imminent = einit_event_subsystem_power | 0x002,
        /*
         * !< shutdown going to happen after this event 
         */
        einit_power_reset_scheduled = einit_event_subsystem_power | 0x011,
        /*
         * !< reboot scheduled 
         */
        einit_power_reset_imminent = einit_event_subsystem_power | 0x012,
        /*
         * !< reboot going to happen after this event 
         */

        einit_power_failing = einit_event_subsystem_power | 0x021,
        /*
         * !< power is failing 
         */
        einit_power_failure_imminent = einit_event_subsystem_power | 0x022,
        /*
         * !< power is failing NOW 
         */
        einit_power_restored = einit_event_subsystem_power | 0x023,
        /*
         * !< power was restored 
         */

        einit_power_source_ac = einit_event_subsystem_power | 0x030,
        einit_power_source_battery = einit_event_subsystem_power | 0x031,
        einit_power_button_power = einit_event_subsystem_power | 0x032,
        einit_power_button_sleep = einit_event_subsystem_power | 0x033,

        einit_power_down_requested = einit_event_subsystem_power | 0x040,
        einit_power_reset_requested = einit_event_subsystem_power | 0x041,
        einit_power_sleep_requested = einit_event_subsystem_power | 0x042,
        einit_power_hibernation_requested =
            einit_event_subsystem_power | 0x043,

        einit_timer_tick = einit_event_subsystem_timer | 0x001,
        /*
         * !< tick.tick.tick. 
         */
        einit_timer_set = einit_event_subsystem_timer | 0x002,
        einit_timer_cancel = einit_event_subsystem_timer | 0x003,

        /*
         * einit_event_subsystem_network: 
         */
        einit_network_interface_construct =
            einit_event_subsystem_network | 0x001,
        einit_network_interface_configure =
            einit_event_subsystem_network | 0x002,
        einit_network_interface_update =
            einit_event_subsystem_network | 0x003,

        einit_network_interface_prepare =
            einit_event_subsystem_network | 0x011,
        einit_network_verify_carrier =
            einit_event_subsystem_network | 0x012,
        einit_network_kill_carrier = einit_event_subsystem_network | 0x013,
        einit_network_address_automatic =
            einit_event_subsystem_network | 0x014,
        einit_network_address_static =
            einit_event_subsystem_network | 0x015,
        einit_network_interface_done =
            einit_event_subsystem_network | 0x016,

        einit_network_interface_cancel =
            einit_event_subsystem_network | 0x020,

        /*
         * einit_event_subsystem_process: 
         */
        einit_process_died = einit_event_subsystem_process | 0x001,

        /*
         * einit_event_subsystem_boot: 
         */
        einit_boot_early = einit_event_subsystem_boot | 0x004,
        einit_boot_load_kernel_extensions =
            einit_event_subsystem_boot | 0x005,
        einit_boot_devices_available = einit_event_subsystem_boot | 0x006,
        einit_boot_root_device_ok = einit_event_subsystem_boot | 0x007,

        einit_boot_dev_writable = einit_event_subsystem_boot | 0x010,

        /*
         * einit_event_subsystem_devices: 
         */
        /*
         * the naming for those is currently exactly the same as the
         * naming for linux hotplug netlink events 
         */
        einit_hotplug_add = einit_event_subsystem_hotplug | 0x001,
        einit_hotplug_remove = einit_event_subsystem_hotplug | 0x002,
        einit_hotplug_change = einit_event_subsystem_hotplug | 0x003,
        einit_hotplug_online = einit_event_subsystem_hotplug | 0x004,
        einit_hotplug_offline = einit_event_subsystem_hotplug | 0x005,
        einit_hotplug_move = einit_event_subsystem_hotplug | 0x006,
        einit_hotplug_generic = einit_event_subsystem_hotplug | 0xfff,

        /*
         * einit_event_subsystem_ipc_v2: 
         */
        einit_ipc_enabling = einit_event_subsystem_ipc_v2 | 0x010,
        einit_ipc_disabling = einit_event_subsystem_ipc_v2 | 0x011,
        einit_ipc_disable = einit_event_subsystem_ipc_v2 | 0x012,

        /*
         * einit_event_subsystem_laptop: 
         */
        einit_laptop_lid_open = einit_event_subsystem_laptop | 0x001,
        einit_laptop_lid_closed = einit_event_subsystem_laptop | 0x002
    };

#define evstaticinit(ttype) { ttype, 0, NULL, NULL, 0, 0, 0, 0, NULL, { NULL }, NULL }
#define evstaticdestroy(ev) { }

    struct einit_event {
        enum einit_event_code type;     /* !< the event or subsystem to
                                         * watch */
        enum einit_event_code chain_type;       /* !< the event to be
                                                 * called right after this 
                                                 * * * * * * * * * one */

        void **set;             /* !< a set that should make sense in
                                 * combination with the event type */
        char *string;           /* !< a string */
        int32_t integer,        /* !< generic integer */
         status,                /* !< generic integer */
         task;                  /* !< generic integer */
        unsigned char flag;     /* !< flags */

        char **stringset;       /* !< a (string-)set that should make
                                 * sense in combination with the event
                                 * type */

        /*
         * ! additional parameters 
         */
        union {
            struct cfgnode *node;
            void *para;
        };

        char *rid;
    };

    enum function_type {
        function_type_specific,
        function_type_generic
    };

    struct exported_function {
        char *name;
        enum function_type type;
        uint32_t version;       /* !< API version (for internal use) */
        void const *function;   /* !< pointer to the function */
        struct lmodule *module;
    };

    enum einit_timer_options {
        einit_timer_once = 0x0001,
        einit_timer_until_cancelled = 0x0002
    };

    struct event_function {
        void (*handler) (struct einit_event *);
    };

    void *event_emit(struct einit_event *, enum einit_event_emit_flags);
    void event_listen(enum einit_event_subsystems,
                      void (*)(struct einit_event *));
    void event_ignore(enum einit_event_subsystems,
                      void (*)(struct einit_event *));

    void function_register_type(const char *, uint32_t, void const *,
                                enum function_type, struct lmodule *);
    void function_unregister_type(const char *, uint32_t, void const *,
                                  enum function_type, struct lmodule *);

#define function_register(name,version,function) function_register_type (name, version, function, function_type_specific, thismodule)
#define function_unregister(name,version,function) function_unregister_type (name, version, function, function_type_specific, thismodule)

    void **function_find(const char *, const uint32_t, const char **);
    void *function_find_one(const char *, const uint32_t, const char **);

    struct exported_function **function_look_up(const char *,
                                                const uint32_t,
                                                const char **);
    struct exported_function *function_look_up_one(const char *,
                                                   const uint32_t,
                                                   const char **);

#define function_call(rv,data,...)\
 ((rv)(((data) != NULL) && ((data)->function != NULL) ?\
  (((data)->type == function_type_generic) ? \
  (((rv (*)(char *, ...))(data)->function) ((data)->name, __VA_ARGS__)) :\
  (((rv (*)())(data)->function) (__VA_ARGS__))) :\
  0))

#define function_call_wfailrv(rv,data,failrv,...)\
 ((rv)(((data) != NULL) && ((data)->function != NULL) ?\
  (((data)->type == function_type_generic) ? \
  (((rv (*)(char *, ...))(data)->function) ((data)->name, __VA_ARGS__)) :\
  (((rv (*)())(data)->function) (__VA_ARGS__))) :\
  failrv))

    extern struct exported_function *einit_function_macro_data;

#define function_call_by_name(rv,name,version,...)\
 ((einit_function_macro_data = (void *)function_look_up_one(name, version, NULL)),\
  function_call(rv, einit_function_macro_data, __VA_ARGS__))

#define function_call_by_name_use_data(rv,name,version,data,failrv,...)\
 (data || (data = function_look_up_one(name, version, NULL)) ? \
  function_call_wfailrv(rv, data, failrv, __VA_ARGS__) : failrv)

#define function_call_by_name_multi(rv,name,version,sub,...)\
 ((einit_function_macro_data = (void *)function_look_up_one(name, version, sub)),\
  function_call(rv, einit_function_macro_data, __VA_ARGS__))

    struct stree *exported_functions;

    char *event_code_to_string(const uint32_t);
    uint32_t event_string_to_code(const char *);

    time_t event_timer_register_timeout(time_t);
    void event_timer_cancel(time_t);

#endif

#ifdef __cplusplus
}
#endif
