include ../config.mk

all:
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} all; fi

install:
	${INSTALL} ${IPARAM} -d ${ETCDIR}
	${INSTALL} ${IPARAM} default.xml ${ETCDIR}
	${INSTALL} ${IPARAM} daemon.xml ${ETCDIR}
	${INSTALL} ${IPARAM} shell.xml ${ETCDIR}
	${INSTALL} ${IPARAM} sandbox.xml ${ETCDIR}
	${INSTALL} ${IPARAM} rc.xml ${ETCDIR}
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} install; fi
