include ../../config.mk
include depend.mk

all: vis-text.so mod-exec.so tty.so mod-daemon.so einit-ipc.so einit-utmp-forger.so common-mount.so pexec.o dexec.o einit-hostname.so
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} all && cd ..; done; fi;

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} depend && cd ..; done; fi;

clean:
	rm -f *.so *.o depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} clean && cd ..; done; fi;

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} vis-text.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} mod-exec.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} tty.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} mod-daemon.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-ipc.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-hostname.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-utmp-forger.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} common-mount.so ${DESTDIR}/${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} install && cd ..; done; fi;

vis-text.so: vis-text.c
	${CCL} vis-text.c -o vis-text.so

pexec.o: pexec.c
	${CCC} -fPIC -c pexec.c

dexec.o: dexec.c
	${CCC} -fPIC -c dexec.c

common-mount.so: common-mount.c pexec.o dexec.o
	${CCC} -fPIC -c common-mount.c
	${CCL} common-mount.o pexec.o dexec.o -o common-mount.so

mod-exec.so: mod-exec.c pexec.o
	${CCC} -fPIC -c mod-exec.c
	${CCL} mod-exec.o pexec.o -o mod-exec.so

mod-daemon.so: mod-daemon.c pexec.o dexec.o
	${CCC} -fPIC -c mod-daemon.c
	${CCL} mod-daemon.o pexec.o dexec.o -o mod-daemon.so

tty.so: tty.c
	${CCL} tty.c -o tty.so

einit-ipc.so: einit-ipc.c
	${CCL} einit-ipc.c -o einit-ipc.so

einit-hostname.so: einit-hostname.c
	${CCL} einit-hostname.c -o einit-hostname.so

einit-utmp-forger.so: einit-utmp-forger.c
	${CCL} einit-utmp-forger.c -o einit-utmp-forger.so
