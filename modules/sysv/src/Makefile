include ../config.mk
include depend.mk

all:
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} all && cd ..; done; fi;

depend:
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && touch depend.mk && ${MAKE} depend && cd ..; done; fi;
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} clean && cd ..; done; fi;
	rm -f *.o *.so depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} install && cd ..; done; fi;
