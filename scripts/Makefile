include ../config.mk

all:
	if test -e /etc/gentoo-release; then cd gentoo && ${MAKE} all; fi

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}${SBINDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -m 0755 install-config ${DESTDIR}${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -m 0755 import-fstab.sh ${DESTDIR}${LIBDIR}/scripts
#	if [ "$$(readlink /bin/sh)" != "dash" ]; then echo -e "PREFIX=\"${PREFIX}\"\nLIBDIR=\"${LIBDIR}\"\nETCDIR=\"${ETCDIR}\"" > ${DESTDIR}${LIBDIR}/scripts/configuration; else echo "PREFIX=\"${PREFIX}\"\nLIBDIR=\"${LIBDIR}\"\nETCDIR=\"${ETCDIR}\"" > ${DESTDIR}${LIBDIR}/scripts/configuration; fi
	${INSTALL} ${IPARAM} -m 0755 ../config.sh ${DESTDIR}${LIBDIR}/scripts/configuration
	if test -n "$$(echo '${CFLAGS}'|grep DEBUG)"; then ${INSTALL} ${IPARAM} -m 0755 debug/einit.vg ${DESTDIR}${SBINDIR}; fi;
	if test -e /etc/gentoo-release; then cd gentoo && ${MAKE} install; fi
