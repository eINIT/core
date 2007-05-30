include ../config.mk

all:

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -m 0755 install-config ${DESTDIR}/${LIBDIR}/scripts
	echo -e "PREFIX=\"${PREFIX}\"\nLIBDIR=\"${LIBDIR}\"\nETCDIR=\"${ETCDIR}\"" > ${DESTDIR}/${LIBDIR}/scripts/configuration