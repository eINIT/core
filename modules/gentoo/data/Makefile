include ../config.mk

all:

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/modules
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/subsystems.d
	${INSTALL} ${IPARAM} -m 0644 gentoo.xml ${DESTDIR}/${ETCDIR}/subsystems.d
	${INSTALL} ${IPARAM} -m 0644 gentoo.xml ${DESTDIR}/${LIBDIR}/defaults/subsystems.d
