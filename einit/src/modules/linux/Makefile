include ../../../config.mk
include depend.mk

all: linux-sysconf.so linux-mount.so linux-network.so linux-process.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so *.o depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-sysconf.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-mount.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-network.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-process.so ${DESTDIR}/${MODDIR}

linux-sysconf.so: linux-sysconf.c
	${CCL} -c linux-sysconf.c
	${LLD} linux-sysconf.o -o linux-sysconf.so

linux-mount.so: linux-mount.c
	${CCL} -c linux-mount.c
	${LLD} linux-mount.o -o linux-mount.so

linux-process.so: linux-process.c
	${CCL} -c linux-process.c
	${LLD} linux-process.o -o linux-process.so
