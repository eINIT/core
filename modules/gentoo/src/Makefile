include ../config.mk

all: einit-module

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.o *.so depend.mk

einit-module: compatibility-sysv-gentoo.so

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-gentoo.so ${DESTDIR}/${MODDIR}

compatibility-sysv-gentoo.so: compatibility-sysv-gentoo.c rc-misc.o librc.o librc-depend.o
	${CCL} -c compatibility-sysv-gentoo.c
	${LLD} compatibility-sysv-gentoo.o rc-misc.o librc.o librc-depend.o -o compatibility-sysv-gentoo.so
