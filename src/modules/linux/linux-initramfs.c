/**
 *  linux-initramfs.c
 *  einit
 *
 *  Created by Ryan Hope on 02/15/2008.
 *  Copyright 2008 Ryan Hope. All rights reserved.
 *
 */

#include <alloca.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_initramfs_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_initramfs_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Initramfs Helper",
 .rid       = "linux-initramfs",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_initramfs_configure
};

module_register(linux_initramfs_self);

#endif

#ifndef TMPFS_MAGIC
# define TMPFS_MAGIC	0x01021994
#endif
#ifndef RAMFS_MAGIC
# define RAMFS_MAGIC	0x858458f6
#endif
#ifndef MS_MOVE
# define MS_MOVE	8192
#endif

const char *run_init(const char *realroot, const char *console);

static int nuke(const char *what);

static int nuke_dirent(int len, const char *dir, const char *name, dev_t me)
{
	int bytes = len + strlen(name) + 2;
	char path[bytes];
	int xlen;
	struct stat st;

	xlen = snprintf(path, bytes, "%s/%s", dir, name);
	assert(xlen < bytes);

	if (lstat(path, &st))
		return ENOENT;	/* Return 0 since already gone? */

	if (st.st_dev != me)
		return 0;	/* DO NOT recurse down mount points!!!!! */

	return nuke(path);
}

/* Wipe the contents of a directory, but not the directory itself */
static int nuke_dir(const char *what)
{
	int len = strlen(what);
	DIR *dir;
	struct dirent *d;
	int err = 0;
	struct stat st;

	if (lstat(what, &st))
		return errno;

	if (!S_ISDIR(st.st_mode))
		return ENOTDIR;

	if (!(dir = opendir(what))) {
		/* EACCES means we can't read it.  Might be empty and removable;
		   if not, the rmdir() in nuke() will trigger an error. */
		return (errno == EACCES) ? 0 : errno;
	}

	while ((d = readdir(dir))) {
		/* Skip . and .. */
		if (d->d_name[0] == '.' &&
		    (d->d_name[1] == '\0' ||
		     (d->d_name[1] == '.' && d->d_name[2] == '\0')))
			continue;

		err = nuke_dirent(len, what, d->d_name, st.st_dev);
		if (err) {
			closedir(dir);
			return err;
		}
	}

	closedir(dir);

	return 0;
}

static int nuke(const char *what)
{
	int rv;
	int err = 0;

	rv = unlink(what);
	if (rv < 0) {
		if (errno == EISDIR) {
			/* It's a directory. */
			err = nuke_dir(what);
			if (!err)
				err = rmdir(what) ? errno : err;
		} else {
			err = errno;
		}
	}

	if (err) {
		errno = err;
		return err;
	} else {
		return 0;
	}
}

const char *run_init(const char *realroot, const char *console)
{
	struct stat rst, cst, ist;
	struct statfs sfs;
	int confd;

	/* First, change to the new root directory */
	if (chdir(realroot))
		return "chdir to new root";

	/* This is a potentially highly destructive program.  Take some
	   extra precautions. */

	/* Make sure the current directory is not on the same filesystem
	   as the root directory */
	if (stat("/", &rst) || stat(".", &cst))
		return "stat";

	if (rst.st_dev == cst.st_dev)
		return "current directory on the same filesystem as the root";

	/* The initramfs should have /init */
	if (stat("/init", &ist) || !S_ISREG(ist.st_mode))
		return "can't find /init on initramfs";

	/* Make sure we're on a ramfs */
	if (statfs("/", &sfs))
		return "statfs /";
	if (sfs.f_type != RAMFS_MAGIC && sfs.f_type != TMPFS_MAGIC)
		return "rootfs not a ramfs or tmpfs";

	/* Okay, I think we should be safe... */

	/* Delete rootfs contents */
	if (nuke_dir("/"))
		return "nuking initramfs contents";

	/* Overmount the root */
	if (mount(".", "/", NULL, MS_MOVE, NULL))
		return "overmounting root";

	/* chroot, chdir */
	if (chroot(".") || chdir("/"))
		return "chroot";

	/* Open /dev/console */
	if ((confd = open(console, O_RDWR)) < 0)
		return "opening console";
	dup2(confd, 0);
	dup2(confd, 1);
	dup2(confd, 2);
	close(confd);
	return "ok";
}

void linux_initramfs_boot_postdev_handler (struct einit_event *ev) {
 if (strmatch(einit_argv[0], "/init")) {
  notice(1,"eINIT is running from within an initramfs!");
  char realroot [BUFFERSIZE];
  int i = 1;
  for (i; i<sizeof(einit_argv); i++) {
   if ( strmatch(strtok(einit_argv[i],"="),"root") ) {
	esprintf(realroot, BUFFERSIZE, "%s", strtok(NULL,"="));
   }
  }
  run_init(realroot, "/dev/console");
  mount ("proc", "/proc", "proc", 0, NULL);
  mount ("sys", "/sys", "sysfs", 0, NULL);
  struct einit_event eml = evstaticinit(einit_boot_initramfs);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
  evstaticdestroy(eml);
 } else {
  struct einit_event eml = evstaticinit(einit_boot_devices_available);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
  evstaticdestroy(eml);
 }
}

int linux_initramfs_cleanup (struct lmodule *pa) {
 event_ignore (einit_boot_postdev, linux_initramfs_boot_postdev_handler);

 return 0;
}

int linux_initramfs_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_initramfs_cleanup;

 event_listen (einit_boot_postdev, linux_initramfs_boot_postdev_handler);

 return 0;
}
