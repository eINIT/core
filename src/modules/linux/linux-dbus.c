/*
 *  linux-dbus.c
 *  einit
 *
 *  Created on 03/5/2008.
 *  Copyright 2008 Ryan Hope. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <einit-modules/exec.h>
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_dbus_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * linux_dbus_provides[] = {"dbus", NULL};

/* no const here, we need to mofiy this on the fly */

struct smodule linux_dbus_self = {
		.eiversion = EINIT_VERSION,
		.eibuild   = BUILDNUMBER,
		.version   = 1,
		.mode      = einit_module,
		.name      = "D-BUS system messagebus",
		.rid       = "linux-dbus",
		.si        = {
				.provides = linux_dbus_provides,
				.requires = NULL,
				.after    = NULL,
				.before   = NULL
		},
		.configure = linux_dbus_configure
};

module_register(linux_dbus_self);

#endif

char * linux_dbus_need_files[] = {"/usr/bin/dbus-daemon", NULL};

struct dexecinfo linux_dbus_dexec = {
	.id = "daemon-dbus",
	.command = "pexec-options dont-close-stdin; dbus-daemon --system --fork",
	.prepare = NULL,
	.cleanup = NULL,
	.is_up = NULL,
	.is_down = NULL,
	.variables = NULL,
	.uid = 0,
	.gid = 0,
	.user = NULL, .group = NULL,
	.restart = 1,
	.cb = NULL,
	.environment = NULL,
	.pidfile = "/var/run/dbus.pid",
	.need_files = linux_dbus_need_files,
	.oattrs = NULL,

	.options = daemon_model_forking,

	.pidfiles_last_update = 0,

	.script = NULL,
	.script_actions = NULL
};

int linux_dbus_enable (void *param, struct einit_event *status) {
	qexec("/usr/bin/dbus-uuidgen --ensure");
	remove("/var/run/dbus");
	mkdir("/var/run/dbus",0775);
	return 	startdaemon(&linux_dbus_dexec, NULL);
}

int linux_dbus_disable (void *param, struct einit_event *status) {
	int ret = status_failed;
	ret = stopdaemon(&linux_dbus_dexec, NULL);
	if (ret==1) remove("/var/run/dbus/system_bus_socket");
	return status_ok;
}

int linux_dbus_configure (struct lmodule *pa) {
	module_init (pa);
	pa->enable = linux_dbus_enable;
	pa->disable = linux_dbus_disable;    
	return 0;
}
