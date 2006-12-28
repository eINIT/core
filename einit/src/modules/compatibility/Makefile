include ../../../config.mk
include depend.mk

all: compatibility-sysv-gentoo.so compatibility-sysv-utmp.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-gentoo.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-utmp.so ${DESTDIR}/${MODDIR}

compatibility-sysv-gentoo.so: compatibility-sysv-gentoo.c
	${CCL} compatibility-sysv-gentoo.c -o compatibility-sysv-gentoo.so

compatibility-sysv-utmp.so: compatibility-sysv-utmp.c
	${CCL} compatibility-sysv-utmp.c -o compatibility-sysv-utmp.so
