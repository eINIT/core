/*
 *  posix-hald.c
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

int posix_hald_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * posix_hald_provides[] = {"hald", NULL};
char * posix_hald_requires[] = {"dbus", NULL};
char * posix_hald_after[] = {"acpid", "logger", NULL};

/* no const here, we need to mofiy this on the fly */

struct smodule posix_hald_self = {
		.eiversion = EINIT_VERSION,
		.eibuild   = BUILDNUMBER,
		.version   = 1,
		.mode      = einit_module,
		.name      = "Hardware Abstraction Layer daemon",
		.rid       = "posix-hald",
		.si        = {
				.provides = posix_hald_provides,
				.requires = posix_hald_requires,
				.after    = posix_hald_after,
				.before   = NULL
		},
		.configure = posix_hald_configure
};

module_register(posix_hald_self);

#endif

char * posix_hald_need_files[] = {"/usr/sbin/hald", NULL};

struct dexecinfo posix_hald_dexec = {
	.id = "daemon-hald",
	.command = "/usr/sbin/hald --daemon=yes --use-syslog",
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
	.pidfile = "/var/run/hald.pid",
	.need_files = posix_hald_need_files,
	.oattrs = NULL,

	.options = daemon_model_forking,

	.pidfiles_last_update = 0,

	.script = NULL,
	.script_actions = NULL
};

int posix_hald_enable (void *param, struct einit_event *status) {
	struct stat fileattrib;
	if (stat("/proc/acpi/event", &fileattrib) == 0) {
		gid_t g;
		lookupuidgid(NULL, &g, NULL, "haldaemon");
		chown ("/proc/acpi/event", 0, g);
		chmod ("/proc/acpi/event", 0440);
	}
	return 	startdaemon(&posix_hald_dexec, NULL);
}

int posix_hald_disable (void *param, struct einit_event *status) {
	return stopdaemon(&posix_hald_dexec, NULL);
}

int posix_hald_configure (struct lmodule *pa) {
	module_init (pa);
	pa->enable = posix_hald_enable;
	pa->disable = posix_hald_disable;    
	return 0;
}
