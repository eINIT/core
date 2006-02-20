include ../../config.mk
include depend.mk

all: dummy.so mod-sysv.so

depend:
	${CC} -M *.c > depend.mk

clean:
	echo > depend.mk
	rm *.so

install: dummy.so mod-sysv.so
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
	${INSTALL} ${SOIPARAM} dummy.so ${MODDIR}
	${INSTALL} ${SOIPARAM} mod-sysv.so ${MODDIR}

dummy.so: dummy.c
	${CCL} dummy.c -o dummy.so

mod-sysv.so: mod-sysv.c
	${CCL} mod-sysv.c -o mod-sysv.so
