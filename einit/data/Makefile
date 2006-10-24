include ../config.mk

all:
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} all; fi

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} einit.xml ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} local.xml ${DESTDIR}/${ETCDIR}
	if test -n "`echo ${OPTIONS}|grep sandbox`"; then ${INSTALL} ${IPARAM} sandbox.xml ${DESTDIR}/${ETCDIR}; fi
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} install; fi
