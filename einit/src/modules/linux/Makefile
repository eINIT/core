include ../../../config.mk
include depend.mk

all: linux-sysconf.so linux-mount.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so depend.mk

install: linux-sysconf.so linux-mount.so
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-sysconf.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-mount.so ${DESTDIR}/${MODDIR}

linux-sysconf.so: linux-sysconf.c
	${CCL} linux-sysconf.c -o linux-sysconf.so

linux-mount.so: linux-mount.c
	${CCL} linux-mount.c -o linux-mount.so
