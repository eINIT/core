include ../config.mk

all:

install: all
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/conf.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/subsystems.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-scheme
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/conf.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/subsystems.d
	${INSTALL} ${IPARAM} -m 0644 modules-scheme/*.scheme ${DESTDIR}/${LIBDIR}/modules-scheme
	${INSTALL} ${IPARAM} -m 0644 modules-scheme/*.scm ${DESTDIR}/${LIBDIR}/modules-scheme
	for i in conf.d/*; do ${LIBDIR}/scripts/install-config "$$i" conf.d "${CONFIGINSTALLMETHOD}"; done
	for i in subsystems.d/*; do ${LIBDIR}/scripts/install-config "$$i" subsystems.d "${CONFIGINSTALLMETHOD}"; done
