include ../../config.mk
include depend.mk

all: vis-text.so mod-exec.so tty.so
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} all && cd ..; done; fi;

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} depend && cd ..; done; fi;

clean:
	rm -f *.so depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} clean && cd ..; done; fi;

install: vis-text.so mod-exec.so tty.so
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
#	${INSTALL} ${SOIPARAM} dummy.so ${MODDIR}
	${INSTALL} ${SOIPARAM} vis-text.so ${MODDIR}
	${INSTALL} ${SOIPARAM} mod-exec.so ${MODDIR}
	${INSTALL} ${SOIPARAM} tty.so ${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} install && cd ..; done; fi;

#dummy.so: dummy.c
#	${CCL} dummy.c -o dummy.so

vis-text.so: vis-text.c
	${CCL} vis-text.c -o vis-text.so

mod-exec.so: mod-exec.c
	${CCL} mod-exec.c -o mod-exec.so

tty.so: tty.c
	${CCL} tty.c -o tty.so
