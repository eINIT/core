include ../config.mk
include depend.mk

all: einit-module
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} all && cd ..; done; fi;

depend:
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && touch depend.mk && ${MAKE} depend && cd ..; done; fi;
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} clean && cd ..; done; fi;
	rm -f *.o *.so depend.mk

einit-module: einit-network.so einit-feedback-visual-fbsplash.so

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-network.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-feedback-visual-fbsplash.so ${DESTDIR}/${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} install && cd ..; done; fi;

einit-network.so: einit-network.c
	${CCL} -c einit-network.c
	${LLD} einit-network.o -o einit-network.so

einit-feedback-visual-fbsplash.so: einit-feedback-visual-fbsplash.c
	${CCL} -c einit-feedback-visual-fbsplash.c
	${LLD} einit-feedback-visual-fbsplash.o -o einit-feedback-visual-fbsplash.so
