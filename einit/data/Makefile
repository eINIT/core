include ../config.mk

all:
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} all; fi

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/local
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/modules
	${INSTALL} ${IPARAM} -m 0644 einit.xml ${DESTDIR}/${ETCDIR}
	if test -e ${ETCDIR}/local.xml; then ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}/local.xml.example; else ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}; fi
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} install; fi
