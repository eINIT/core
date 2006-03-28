include ../../../config.mk
include depend.mk

all: linux-sysconf.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so depend.mk

install: linux-sysconf.so
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
	${INSTALL} ${SOIPARAM} linux-sysconf.so ${MODDIR}

linux-sysconf.so: linux-sysconf.c
	${CCL} linux-sysconf.c -o linux-sysconf.so
