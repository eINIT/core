include ../config.mk

all:

install: all
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/conf.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-xml
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-xml/network
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/conf.d
	${INSTALL} ${IPARAM} -m 0644 modules-xml/*.xml ${DESTDIR}/${LIBDIR}/modules-xml
	${INSTALL} ${IPARAM} -m 0644 modules-xml/network/*.xml ${DESTDIR}/${LIBDIR}/modules-xml/network
	for i in conf.d/*; do ${LIBDIR}/scripts/install-config "$$i" conf.d "${CONFIGINSTALLMETHOD}"; done
