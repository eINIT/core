include ../../config.mk
include depend.mk

all: dummy.so
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do cd "$$module" && ${MAKE} all; done; fi;

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} depend; done; fi;

clean:
	rm -f *.so depend.mk
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} clean; done; fi;

install: dummy.so
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
	${INSTALL} ${SOIPARAM} dummy.so ${MODDIR}
	if test -n "${PMODULES}"; then for module in "${PMODULES}"; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} install; done; fi;

dummy.so: dummy.c
	${CCL} dummy.c -o dummy.so
