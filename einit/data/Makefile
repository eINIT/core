include ../config.mk

all:
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} all; fi

install:
	${INSTALL} ${IPARAM} -d ${ETCDIR}
	${INSTALL} ${IPARAM} default.xml ${ETCDIR}
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} install; fi
