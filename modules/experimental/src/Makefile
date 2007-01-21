include ../config.mk
include depend.mk

all: einit-module
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} all && cd ..; done; fi;

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.o *.so depend.mk

einit-module: einit-network.so

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-sysv-gentoo.so ${DESTDIR}/${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} install && cd ..; done; fi;

einit-network.so: einit-network.c
	${CCL} -c einit-network.c
	${LLD} einit-network.o -o einit-network.so
