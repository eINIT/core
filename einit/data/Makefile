include ../config.mk

all:

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/local
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/modules
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-xml
	${INSTALL} ${IPARAM} -m 0644 einit.xml ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -m 0644 modules-xml/*.xml ${DESTDIR}/${LIBDIR}/modules-xml
	if test -e ${ETCDIR}/local.xml; then ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}/local.xml.example; else ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}; fi
