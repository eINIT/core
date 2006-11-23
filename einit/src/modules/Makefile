include ../../config.mk
include depend.mk

all: einit-feedback-visual-textual.so einit-mod-exec.so einit-tty.so einit-mod-daemon.so einit-ipc.so einit-utmp-forger.so einit-mount.so pexec.o dexec.o einit-hostname.so einit-feedback-aural.so einit-network.so einit-process.so einit-exec.so
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do cd "$$module" && ${MAKE} all && cd ..; done; fi;

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} depend && cd ..; done; fi;

clean:
	rm -f *.so *.o depend.mk
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} clean && cd ..; done; fi;

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-feedback-visual-textual.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-feedback-aural.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-mod-exec.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-tty.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-mod-daemon.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-ipc.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-hostname.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-utmp-forger.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-mount.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-process.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-network.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-exec.so ${DESTDIR}/${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} install && cd ..; done; fi;

einit-feedback-visual-textual.so: einit-feedback-visual-textual.c
	${CCL} einit-feedback-visual-textual.c -o einit-feedback-visual-textual.so

einit-feedback-aural.so: einit-feedback-aural.c pexec.o
	${CCC} -fPIC -c einit-feedback-aural.c
	${CCL} einit-feedback-aural.o pexec.o -o einit-feedback-aural.so

pexec.o: pexec.c
	${CCC} -fPIC -c pexec.c

dexec.o: dexec.c
	${CCC} -fPIC -c dexec.c

einit-mount.so: einit-mount.c pexec.o dexec.o
	${CCC} -fPIC -c einit-mount.c
	${CCL} einit-mount.o pexec.o dexec.o -o einit-mount.so

einit-mod-exec.so: einit-mod-exec.c pexec.o
	${CCC} -fPIC -c einit-mod-exec.c
	${CCL} einit-mod-exec.o pexec.o -o einit-mod-exec.so

einit-mod-daemon.so: einit-mod-daemon.c pexec.o dexec.o
	${CCC} -fPIC -c einit-mod-daemon.c
	${CCL} einit-mod-daemon.o pexec.o dexec.o -o einit-mod-daemon.so

einit-tty.so: einit-tty.c
	${CCL} einit-tty.c -o einit-tty.so

einit-network.so: einit-network.c
	${CCL} einit-network.c -o einit-network.so

einit-process.so: einit-process.c
	${CCL} einit-process.c -o einit-process.so

einit-ipc.so: einit-ipc.c
	${CCL} einit-ipc.c -o einit-ipc.so

einit-exec.so: einit-exec.c
	${CCL} einit-exec.c -o einit-exec.so

einit-hostname.so: einit-hostname.c
	${CCL} einit-hostname.c -o einit-hostname.so

einit-utmp-forger.so: einit-utmp-forger.c
	${CCL} einit-utmp-forger.c -o einit-utmp-forger.so
