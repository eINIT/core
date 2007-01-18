include ../../config.mk
include depend.mk

all: einit-feedback-visual-textual.so einit-mod-exec.so einit-tty.so einit-mod-daemon.so einit-ipc.so einit-mount.so einit-hostname.so einit-feedback-aural.so einit-network.so einit-process.so einit-exec.so einit-external.so einit-parse-sh.so einit-feedback-visual-fbsplash.so
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
	${INSTALL} ${SOIPARAM} einit-feedback-visual-fbsplash.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-feedback-aural.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-mod-exec.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-tty.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-mod-daemon.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-ipc.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-hostname.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-mount.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-process.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-network.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-exec.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-external.so ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-parse-sh.so ${DESTDIR}/${MODDIR}
	if test -n "${PMODULES}"; then for module in ${PMODULES}; do touch "$$module/depend.mk"; cd "$$module" && ${MAKE} install && cd ..; done; fi;

einit-feedback-visual-textual.so: einit-feedback-visual-textual.c
	${CCL} -c einit-feedback-visual-textual.c
	${LLD} einit-feedback-visual-textual.o -o einit-feedback-visual-textual.so

einit-feedback-visual-fbsplash.so: einit-feedback-visual-fbsplash.c
	${CCL} -c einit-feedback-visual-fbsplash.c
	${LLD} einit-feedback-visual-fbsplash.o -o einit-feedback-visual-fbsplash.so

einit-feedback-aural.so: einit-feedback-aural.c
	${CCL} -c einit-feedback-aural.c
	${LLD} einit-feedback-aural.o -o einit-feedback-aural.so

einit-mount.so: einit-mount.c
	${CCL} -c einit-mount.c
	${LLD} einit-mount.o -o einit-mount.so

einit-mod-exec.so: einit-mod-exec.c
	${CCL} -c einit-mod-exec.c
	${LLD} einit-mod-exec.o -o einit-mod-exec.so

einit-mod-daemon.so: einit-mod-daemon.c
	${CCL} -c einit-mod-daemon.c
	${LLD} einit-mod-daemon.o -o einit-mod-daemon.so

einit-tty.so: einit-tty.c
	${CCL} -c einit-tty.c
	${LLD} einit-tty.o -o einit-tty.so

einit-external.so: einit-external.c
	${CCL} -c einit-external.c
	${LLD} einit-external.o -o einit-external.so

einit-network.so: einit-network.c
	${CCL} -c einit-network.c
	${LLD} einit-network.o -o einit-network.so

einit-process.so: einit-process.c
	${CCL} -c einit-process.c
	${LLD} einit-process.o -o einit-process.so

einit-ipc.so: einit-ipc.c
	${CCL} -c einit-ipc.c
	${LLD} einit-ipc.o -o einit-ipc.so

einit-exec.so: einit-exec.c
	${CCL} -c einit-exec.c
	${LLD} einit-exec.o -o einit-exec.so

einit-hostname.so: einit-hostname.c
	${CCL} -c einit-hostname.c
	${LLD} einit-hostname.o -o einit-hostname.so

einit-parse-sh.so: einit-parse-sh.c
	${CCL} -c einit-parse-sh.c
	${LLD} einit-parse-sh.o -o einit-parse-sh.so
