/*
 *  linux-event-client-acpi.c
 *  einit
 *
 *  Created on 03/09/2008.
 *  Copyright 2008 Ryan Hope. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <einit/tree.h>
#include <errno.h>
#include <syslog.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_event_client_acpi_configure(struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * linux_event_client_acpi_provides[] = {"acpi-eventd-handler", NULL};

/* no const here, we need to mofiy this on the fly */

struct smodule linux_event_client_acpi_self = {
	.eiversion = EINIT_VERSION,
	.eibuild = BUILDNUMBER,
	.version = 1,
	.mode = einit_module,
	.name = "ACPI Event Client",
	.rid = "event-client-acpi",
	.si = {
		.provides = linux_event_client_acpi_provides,
		.requires = NULL,
		.after = NULL,
		.before = NULL
	},
	.configure = linux_event_client_acpi_configure
};

module_register(linux_event_client_acpi_self);

#endif

void linux_event_client_acpi_power_source_battery(struct einit_event *ev) {
}

void linux_event_client_acpi_power_source_ac(struct einit_event *ev) {
}

int linux_event_client_acpi_configure(struct lmodule *pa) {
	module_init(pa);
	event_listen(einit_power_source_battery,
			linux_event_client_acpi_power_source_battery);
	event_listen(einit_power_source_ac,
			linux_event_client_acpi_power_source_ac);
	return 0;
}

