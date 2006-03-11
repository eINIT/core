include ../../config.mk
include depend.mk

all: dummy.so vis-text.so
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do cd "$$module" && ${MAKE} all; done; fi;

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} depend; done; fi;

clean:
	rm -f *.so depend.mk
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} clean; done; fi;

install: dummy.so vis-text.so
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
	${INSTALL} ${SOIPARAM} dummy.so ${MODDIR}
	${INSTALL} ${SOIPARAM} vis-text.so ${MODDIR}
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} install; done; fi;

dummy.so: dummy.c
	${CCL} dummy.c -o dummy.so

vis-text.so: vis-text.c
	${CCL} vis-text.c -o vis-text.so
