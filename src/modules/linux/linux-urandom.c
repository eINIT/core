/*
 *  linux-urandom.c
 *  einit
 *
 *  Created on 02/17/2008.
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
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_urandom_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * linux_urandom_provides[] = {"urandom", NULL};

/* no const here, we need to mofiy this on the fly */

struct smodule linux_urandom_self = {
		.eiversion = EINIT_VERSION,
		.eibuild   = BUILDNUMBER,
		.version   = 1,
		.mode      = einit_module_generic,
		.name      = "Urandom",
		.rid       = "linux-urandom",
		.si        = {
				.provides = linux_urandom_provides,
				.requires = NULL,
				.after    = NULL,
				.before   = NULL
		},
		.configure = linux_urandom_configure
};

module_register(linux_urandom_self);

#endif

int my_module_enable  (void *, struct einit_event *);
int my_module_disable (void *, struct einit_event *);

void linux_urandom_mini_dd(const char *from, const char *to, size_t s) {
	int from_fd = open(from, O_RDONLY);
	if (from_fd) {
		int to_fd = open(to, O_WRONLY | O_CREAT);
		if (to_fd) {
			char buffer[s];
			size_t len = read (from_fd, buffer, s);
			if (len > 0) {
				write (to_fd, buffer, len);
			}
			close (to_fd);
		}
		close (from_fd);
	}
}

int linux_urandom_get_poolsize(void) {
	char *poolsize_s = readfile("/proc/sys/kernel/random/poolsize");
	int poolsize = 512;
	if (poolsize_s) {
		poolsize = parse_integer(poolsize_s) / 8;
		efree (poolsize_s);
	}
	return poolsize;
}

int linux_urandom_save_seed (void) {
	int ret = status_failed;
	char *seedPath = cfg_getstring ("configuration-services-urandom/seed", NULL);
	if (seedPath) {
		linux_urandom_mini_dd ("/dev/urandom", seedPath, linux_urandom_get_poolsize());
		return status_ok;
	} else {
		notice(3,"Don't know where to save seed!");
	}
	return status_ok;
}

int linux_urandom_enable (void *param, struct einit_event *status) {
	fbprintf(status,"Initialising the Random Number Generator");
	char *seedPath = cfg_getstring ("configuration-services-urandom/seed", NULL);
	if (seedPath) {
		struct stat fileattrib;
		if (stat(seedPath, &fileattrib) == 0) {
			linux_urandom_mini_dd(seedPath, "/dev/urandom", linux_urandom_get_poolsize());
			if (remove(seedPath) == -1) {
				fbprintf(status,"Skipping %s initialization (ro root?)", seedPath);
				return status_ok;
			} else {
				return linux_urandom_save_seed();
			}
		}
	} else {
		return status_failed;
	}
}

int linux_urandom_disable (void *param, struct einit_event *status) {
	fbprintf(status,"Saving random seed");
	return linux_urandom_save_seed();
}

int linux_urandom_configure (struct lmodule *pa) {
	module_init (pa);
	pa->enable = linux_urandom_enable;
	pa->disable = linux_urandom_disable;    
	char *seedPath = cfg_getstring ("configuration-services-urandom/seed", NULL);
	if (seedPath) {
		char *files[2];
		files[0] = seedPath;
		files[1] = 0;
		char *after = after_string_from_files (files);
		if (after) {
			((struct smodule *)pa->module)->si.after = set_str_add_stable(NULL, after);
			efree (after);
		}
	}    
	return 0;
}
