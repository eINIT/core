/*
 *  readahead.c
 *  einit
 *
 *  Created by Ryan Hope on 3/30/2007.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/module-logic.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <einit/bitch.h>
#include <pthread.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef LINUX
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <linux/vt.h>
#endif

#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_readahead_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * einit_readahead_provides[] = {"readahead", NULL};
char * einit_readahead_requires[] = {"mount-critical", NULL};
const struct smodule einit_readahead_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT readahead",
 .rid       = "readahead",
 .si        = {
  .provides = einit_readahead_provides,
  .requires = einit_readahead_requires,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_readahead_configure
};

module_register(einit_readahead_self);

#endif

void process_file(char *filename) {
	int fd;
	struct stat buf;

	if (!filename)
		return;
	
	if (stat(filename, &buf)<0) {
		int stat_errno;
		stat_errno = errno;
		switch(stat_errno) {
			case EBADF:
			case ENOENT:
			case ENOTDIR:
			case ENAMETOOLONG:
			case ELOOP:
			case EACCES:
				break;
			default:
				break;
		}
	goto end;
	}

	/* Don't bother reading directories, devices (char or block), FIFOs or named sockets */
	if(S_ISDIR(buf.st_mode) || S_ISCHR(buf.st_mode) || 
			S_ISBLK(buf.st_mode) || S_ISFIFO(buf.st_mode) ||
			S_ISSOCK(buf.st_mode))
		goto end;

	fd = open(filename,O_RDONLY);
	if (fd<0) {
		goto end;
	}

	{
		int readahead_errno;
		readahead(fd, (loff_t)0, (size_t)buf.st_size);
		readahead_errno = errno;
		switch(readahead_errno) {
			case 0: 
				break;
			case EBADF:
			case EINVAL:
				break;
		}
	}

	close(fd);

end:
	/* be nice to other processes now */
	sched_yield();
}

#define MAXPATH 2048
void process_files(char* filename) {
	int fd;
	char* file = NULL;
	struct stat statbuf;
	char buffer[MAXPATH+1];
	char* iter = NULL;

	if (!filename)
		return;
	
	if(strcmp(filename,"-") == 0) {
		return;
	}

	fd = open(filename,O_RDONLY);

	if (fd<0) {
		return;
	}
	
	if (fstat(fd, &statbuf)<0) {
		return;
	}

	/* map the whole file */
	file = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!file || file == MAP_FAILED) {
		return;
	}

	iter = file;
	while (iter) {
		char* next = memchr(iter,'\n',file + statbuf.st_size - iter);
		if (next) {
			if(!(next - iter) >= MAXPATH) {
				if (next-iter > 1) {
					memcpy(buffer, iter, next-iter);
					buffer[next-iter]='\0';
					if(buffer[0] != '#')
						process_file(buffer);
				}
			}
			iter = next + 1;
		} else {
			iter = NULL;
		}
	}
}

void einit_readahead_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "examine") && strmatch(ev->argv[1], "configuration")) {
  char *s;

  if (!(s = cfg_getstring("configuration-system-readahead", NULL))) {
   eputs (" * configuration variable \"configuration-system-readahead\" not found.\n", ev->output);
   ev->ipc_return++;
  }

  ev->implemented = 1;
 }
}

int einit_readahead_cleanup (struct lmodule *this) {
 event_ignore (einit_event_subsystem_ipc, einit_readahead_ipc_event_handler);

 return 0;
}

int einit_readahead_enable (void *pa, struct einit_event *status) {
 char *list;
 list = cfg_getstring ("configuration-system-readahead", NULL);
 if (list) {
  status->string = "Performing file readahead";
  status_update (status);
  process_files (list);
 } else {
  status->string = "Could not perform file readahead";
  status->flag++;
  status_update (status);
 }

 return status_ok;
}

int einit_readahead_disable (void *pa, struct einit_event *status) {
 return status_ok;
}

int einit_readahead_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->cleanup = einit_readahead_cleanup;
 thismodule->enable = einit_readahead_enable;
 thismodule->disable = einit_readahead_disable;

 event_listen (einit_event_subsystem_ipc, einit_readahead_ipc_event_handler);

 return 0;
}

