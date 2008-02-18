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

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_urandom_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * linux_urandom_provides[] = {"urandom", NULL};

const struct smodule linux_urandom_self = {
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

int linux_urandom_cleanup (struct lmodule *pa) {
 return 0;
}

int save_seed(void) {
	int ret = EXIT_FAILURE;
	char *seedPath = cfg_getstring ("configuration-services-urandom/seed", NULL);
	if (seedPath) {
		//FILE *ps = fopen("/proc/sys/kernel/random/poolsize","r");
		//char *buffer;
		int poolsize = 512;
		/*if (ps) {
			fprintf(stdout,"proc poolsize found\n");
			fgets(buffer, 1, ps);
			fprintf(stdout, "%i", atoi(buffer));
			poolsize = atoi(buffer) / 4096 * 512;
			fclose(ps);;
		}*/
		FILE *urandom = fopen("/dev/urandom", "r");
		if (urandom) {
			FILE *seedFile = fopen(seedPath, "w");
			if (seedFile) {
				char seed[poolsize+1];
				int i = 0;
				for (i;i<poolsize;i++) {
					seed[i] = fgetc(urandom);
				}
				seed[poolsize] = '\0';
				fprintf(seedFile, "%s", seed);
				fclose(seedFile);
				ret = EXIT_SUCCESS;
			}
			fclose(urandom);
		}
	} else {
		fprintf(stdout,"Don't know where to save seed!");
	}
	return ret;
}

int linux_urandom_enable (void *param, struct einit_event *status) {
	int ret;
	FILE *urandom = fopen("/dev/urandom", "r");
	char *seedPath = cfg_getstring ("configuration-services-urandom/seed", NULL);
	if (urandom) {
		if (seedPath) {
			FILE *seedFile = fopen(seedPath, "w+");
			if (seedFile) {
				char seed;
				do {
					seed = getc(seedFile);
					} while (seed != EOF);
				fprintf(urandom, "%s", seed);
				fclose(seedFile);
			}
		}
		fclose(urandom);
	}
	if ( remove(seedPath) == -1 ) {
		fprintf(stdout,"URANDOM: Skipping %s initialization\n",seedPath);
		ret = EXIT_SUCCESS;
	} else {
		fprintf(stdout,"URANDOM: Initializing random number generator\n");
		int ret = save_seed();
		if (ret==EXIT_FAILURE) {
			fprintf(stdout,"URANDOM: Error initializing random number generator\n");
		}
	}
	return ret;
}

int linux_urandom_disable (void *param, struct einit_event *status) {
	fprintf(stdout,"URANDOM: Saving random seed\n");
	int ret = save_seed();
	if (ret==EXIT_FAILURE) {
		fprintf(stdout,"URANDOM: Failed to save random seed\n");
	}
	return ret;
}

int linux_urandom_configure (struct lmodule *pa) {
 module_init (pa);

 pa->enable = linux_urandom_enable;
 pa->disable = linux_urandom_disable;
 pa->cleanup = linux_urandom_cleanup;

 return 0;
}
