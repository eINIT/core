include ../config.mk

all:
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} all; fi

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} default.xml ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} daemon.xml ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} shell.xml ${DESTDIR}/${ETCDIR}
	if test -n "`echo ${OPTIONS}|grep sandbox`"; then ${INSTALL} ${IPARAM} sandbox.xml ${DESTDIR}/${ETCDIR}; fi
	${INSTALL} ${IPARAM} rc.xml ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} mount.xml ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} filesystems.xml ${DESTDIR}/${ETCDIR}
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} install; fi
