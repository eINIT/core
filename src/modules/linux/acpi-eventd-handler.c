/*
 *  acpi-eventd-handler.c
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

#define ACPI_EVENT_PREFIX "configuration-services-acpi-eventd-event"

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int acpi_eventd_handler_configure(struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * acpi_eventd_handler_provides[] = {"acpi-eventd-handler", NULL};

/* no const here, we need to mofiy this on the fly */

struct smodule acpi_eventd_handler_self = {
	.eiversion = EINIT_VERSION,
	.eibuild = BUILDNUMBER,
	.version = 1,
	.mode = einit_module,
	.name = "ACPI Eventd Handler",
	.rid = "acpi-eventd-handler",
	.si = {
		.provides = acpi_eventd_handler_provides,
		.requires = NULL,
		.after = NULL,
		.before = NULL
	},
	.configure = acpi_eventd_handler_configure
};

module_register(acpi_eventd_handler_self);

#endif

void acpi_eventd_handler_generic_event_handler(struct einit_event *ev) {
	syslog(LOG_INFO, "%s\"%s\"[%d]",ev->rid,ev->string,ev->status);
	struct stree *st;
	st = cfg_prefix(ACPI_EVENT_PREFIX);
	puts("a");
	if (st) {
		puts("b");
		struct stree *cur = streelinear_prepare(st);
		while (cur) {
			puts("c");
			struct cfgnode *n = cur->value;
			if (n->arbattrs) {
				puts("d");
			}
			cur = streenext(cur);
		}
		streefree(st);
	}
}

int acpi_eventd_handler_configure(struct lmodule *pa) {
	module_init(pa);
	event_listen(einit_acpi_event_generic,
			acpi_eventd_handler_generic_event_handler);
	return 0;
}

