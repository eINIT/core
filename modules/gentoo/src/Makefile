include ../config.mk
include depend.mk

all: einit-module

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.o *.so depend.mk

einit-module: compatibility-sysv-gentoo.so

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-gentoo.so ${DESTDIR}/${MODDIR}

compatibility-sysv-gentoo.so: compatibility-sysv-gentoo.c
	${CCL} -c compatibility-sysv-gentoo.c
	${LLD} compatibility-sysv-gentoo.o -o compatibility-sysv-gentoo.so
