/*
 *  readahead.c
 *  einit
 *
 *  Created by Ryan Hope on 3/30/2007.
 *
 *
 */

/*
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be
	  used to endorse or promote products derived from this software without
	  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _MODULE

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
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
/* okay, i think i found the proper file now */
#include <asm/ioctls.h>
#include <linux/vt.h>
#endif

#ifdef POSIXREGEX
#include <regex.h>
#include <dirent.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif


int _einit_readahead_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)
const struct smodule _einit_readahead_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "eINIT readahead",
 .rid       = "readahead",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _einit_readahead_configure
};

module_register(_einit_readahead_self);

#endif

void process_file(char *filename) {
	int fd;
	struct stat buf;
	
	if (!filename)
		return;

	if (stat(filename, &buf)<0)
		goto end;

	if(S_ISDIR(buf.st_mode) || S_ISCHR(buf.st_mode) || S_ISBLK(buf.st_mode) || S_ISFIFO(buf.st_mode) || S_ISSOCK(buf.st_mode))
		goto end;

	fd = open(filename,O_RDONLY);
	if (fd<0)
		goto end;

	{
		int readahead_errno;
		readahead(fd, (loff_t)0, (size_t)buf.st_size);
		readahead_errno = errno;
		switch(readahead_errno) {
			case 0: 
				break;
			case EINVAL:
				break;
		}
	}

	close(fd);

end:
	/* this could be bad, idk */
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
	
	if(strcmp(filename,"-") == 0)
		return;

	fd = open(filename,O_RDONLY);

	if (fd<0)
		return;
	
	if (fstat(fd, &statbuf)<0)
		return;

	/* map the whole file */
	file = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!file || file == MAP_FAILED) {
		return;
	}

	iter = file;
	while (iter) {
		/* find next newline */
		char* next = memchr(iter,'\n',file + statbuf.st_size - iter);
		if (next) {
			// if the length is positive, and shorter than MAXPATH
			// then we process it
			if((next - iter) >= MAXPATH) {
				fprintf(stderr,"%s:%s:%d:item in list too long!\n",__FILE__,__func__,__LINE__);
			} else if (next-iter > 1) {
				memcpy(buffer, iter, next-iter);
				// replace newline with string terminator
				buffer[next-iter]='\0';
				// we allow # as the first character in a line, to show comments
				if(buffer[0] != '#') {
					process_file(buffer);
				}
			}
			iter = next + 1;
		} else {
			iter = NULL;
		}
	}
}

void _einit_readahead_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && strmatch(ev->set[0], "examine") && strmatch(ev->set[1], "configuration")) {
  char *s;

  if (!(s = cfg_getstring("configuration-system-readahead", NULL))) {
   eputs (" * configuration variable \"configuration-system-readahead\" not found.\n", (FILE *)ev->para);
   /* ev->task++; */
  }

  ev->flag = 1;
 }
}

int _einit_readahead_cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_IPC, _einit_readahead_ipc_event_handler);

 return 0;
}

int _einit_readahead_enable (void *pa, struct einit_event *status) {
 char *name;
 if ((list = cfg_getstring ("configuration-system-readahead-list", NULL))) {
  status->string = "performing file readahead into page cache";
  status_update (status);
  if (process_files (list) {
   status->string = strerror(errno);
   errno = 0;
   status->flag++;
   status_update (status);
  }
 } else {
  status->string = "there was some error";
  status->flag++;
  status_update (status);
 }

 return STATUS_OK;
}

int _einit_readahead_disable (void *pa, struct einit_event *status) {
 return STATUS_OK;
}

int _einit_readahead_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->cleanup = _einit_readahead_cleanup;
 thismodule->enable = _einit_readahead_enable;
 thismodule->disable = _einit_readahead_disable;

 event_listen (EVENT_SUBSYSTEM_IPC, _einit_readahead_ipc_event_handler);

 return 0;
}

