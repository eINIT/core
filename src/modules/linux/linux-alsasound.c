/*
 *  linux-alsasound.c
 *  einit
 *
 *  Created on 02/26/2008.
 *  Copyright 2008 Ryan Hope. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <einit-modules/exec.h>
#include <errno.h>
#include <sched.h>

#include <sys/stat.h>
#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_alsasound_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * linux_alsasound_provides[] = {"alsasound", NULL};
char * linux_alsasound_requires[] = {"mount-critical", NULL};

/* no const here, we need to mofiy this on the fly */

struct smodule linux_alsasound_self = {
		.eiversion = EINIT_VERSION,
		.eibuild   = BUILDNUMBER,
		.version   = 1,
		.mode      = einit_module_generic,
		.name      = "ALSA Sound",
		.rid       = "linux-alsasound",
		.si        = {
				.provides = linux_alsasound_provides,
				.requires = linux_alsasound_requires,
				.after    = NULL,
				.before   = NULL
		},
		.configure = linux_alsasound_configure
};

module_register(linux_alsasound_self);

#endif

// someday this module will load the alsa kernel modules first
int linux_alsasound_load_modules(void *param, struct einit_event *status) {
	int ret = status_failed;
	return ret;
}

// someday this module kill sound apps
int linux_alsasound_terminate(void *param, struct einit_event *status) {
	int ret = status_failed;
	return ret;
}

int linux_alsasound_enable (void *param, struct einit_event *status) {
	int ret = status_ok;
	char *statefile = cfg_getstring ("configuration-services-alsasound/statefile", NULL);
	if (statefile) {
		struct stat fileattrib;
		if (stat(statefile, &fileattrib) == 0) {
			char cmd[BUFFERSIZE];
			snprintf(cmd,BUFFERSIZE,"alsactl -f %s restore",statefile);
			qexec(cmd);
		} else {
			char msg[BUFFERSIZE];
			snprintf(msg,BUFFERSIZE,"Could not find %s, unmute mixer manually.",statefile);
			notice(2,msg);
			ret = status_failed;
		}
	}
	return ret;
}

int linux_alsasound_disable (void *param, struct einit_event *status) {
	int ret = status_ok;
	char *statefile = cfg_getstring ("configuration-services-alsasound/statefile", NULL);
	if (statefile) {
		char cmd[BUFFERSIZE];
		snprintf(cmd,BUFFERSIZE,"alsactl -f %s store",statefile);
		qexec(cmd);
	} else {
		char msg[BUFFERSIZE];
		snprintf(msg,BUFFERSIZE,"Could not find %s, dont know where to store.",statefile);
		notice(2,msg);
		ret = status_failed;
	}
	return ret;
}

int linux_alsasound_configure (struct lmodule *pa) {
	module_init (pa);
	pa->enable = linux_alsasound_enable;
	pa->disable = linux_alsasound_disable;    
	char *statefile = cfg_getstring ("configuration-services-alsasound/statefile", NULL);
	if (statefile) {
		char *files[2];
		files[0] = statefile;
		files[1] = 0;
		char *after = after_string_from_files (files);
		if (after) {
			((struct smodule *)pa->module)->si.after = set_str_add_stable(NULL, after);
			efree (after);
		}
	}
	return 0;
}
