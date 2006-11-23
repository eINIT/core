include ../../../config.mk
include depend.mk

all: linux-sysconf.so linux-mount.so linux-network.so linux-process.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-sysconf.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-mount.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-network.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-process.so ${DESTDIR}/${MODDIR}

linux-sysconf.so: linux-sysconf.c
	${CCL} linux-sysconf.c -o linux-sysconf.so

linux-mount.so: linux-mount.c
	${CCL} linux-mount.c -o linux-mount.so

linux-network.so: linux-network.c
	${CCL} linux-network.c -o linux-network.so

linux-process.so: linux-process.c
	${CCL} linux-process.c -o linux-process.so
