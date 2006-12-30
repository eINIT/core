include ../../../config.mk
include depend.mk

all: compatibility-sysv-gentoo.so compatibility-sysv-utmp.so compatibility-mod-sysv-init-d.so compatibility-sysv-initctl.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so *.o depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-gentoo.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-utmp.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-mod-sysv-init-d.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-initctl.so ${DESTDIR}/${MODDIR}

compatibility-sysv-gentoo.so: compatibility-sysv-gentoo.c
	${CCL} -c compatibility-sysv-gentoo.c
	${LLD} compatibility-sysv-gentoo.o -o compatibility-sysv-gentoo.so

compatibility-sysv-utmp.so: compatibility-sysv-utmp.c
	${CCL} -c compatibility-sysv-utmp.c
	${LLD} compatibility-sysv-utmp.o -o compatibility-sysv-utmp.so

compatibility-mod-sysv-init-d.so: compatibility-mod-sysv-init-d.c
	${CCL} -c compatibility-mod-sysv-init-d.c
	${LLD} compatibility-mod-sysv-init-d.o -o compatibility-mod-sysv-init-d.so

compatibility-sysv-initctl.so: compatibility-sysv-initctl.c
	${CCL} -c compatibility-sysv-initctl.c
	${LLD} compatibility-sysv-initctl.o -o compatibility-sysv-initctl.so
