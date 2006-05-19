include ../../config.mk
include depend.mk

all: vis-text.so mod-exec.so tty.so mod-daemon.so einit-ipc.so einit-utmp-forger.so
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} all && cd ..; done; fi;

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} depend && cd ..; done; fi;

clean:
	rm -f *.so *.o depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} clean && cd ..; done; fi;

install: all
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
#	${INSTALL} ${SOIPARAM} dummy.so ${MODDIR}
	${INSTALL} ${SOIPARAM} vis-text.so ${MODDIR}
	${INSTALL} ${SOIPARAM} mod-exec.so ${MODDIR}
	${INSTALL} ${SOIPARAM} tty.so ${MODDIR}
	${INSTALL} ${SOIPARAM} mod-daemon.so ${MODDIR}
	${INSTALL} ${SOIPARAM} einit-ipc.so ${MODDIR}
	${INSTALL} ${SOIPARAM} einit-utmp-forger.so ${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} install && cd ..; done; fi;

#dummy.so: dummy.c
#	${CCL} dummy.c -o dummy.so

vis-text.so: vis-text.c
	${CCL} vis-text.c -o vis-text.so

pexec.o: pexec.c
	${CCC} -c pexec.c

mod-exec.so: mod-exec.c pexec.o
	${CCC} -c mod-exec.c
	${CCL} mod-exec.o pexec.o -o mod-exec.so

mod-daemon.so: mod-daemon.c pexec.o
	${CCC} -c mod-daemon.c
	${CCL} mod-daemon.o pexec.o -o mod-daemon.so

tty.so: tty.c
	${CCL} tty.c -o tty.so

einit-ipc.so: einit-ipc.c
	${CCL} einit-ipc.c -o einit-ipc.so

einit-utmp-forger.so: einit-utmp-forger.c
	${CCL} einit-utmp-forger.c -o einit-utmp-forger.so
