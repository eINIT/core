include ../config.mk

all:
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} all; fi

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} --mode 0644 einit.xml ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} --mode 0644  local.xml ${DESTDIR}/${ETCDIR}
	if test -n "`echo ${PMODULES}|grep efl`"; then cd einit.edje && ${MAKE} install; fi
