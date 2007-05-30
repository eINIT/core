include ../config.mk

all:

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/local
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/modules
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/init.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/subsystems.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-lisp
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/subsystems.d
	${INSTALL} ${IPARAM} -m 0644 einit.xml ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -m 0644 modules-lisp/*lisp ${DESTDIR}/${LIBDIR}/modules-lisp
	${INSTALL} ${IPARAM} -m 0755 scripts/* ${DESTDIR}/${LIBDIR}/scripts
	
	for i in subsystems.d/*; do ${DESTDIR}/${LIBDIR}/scripts/install-config "$$i" subsystems.d "${CONFIGINSTALLMETHOD}"; done
	for i in local.xml fstab.xml modes.xml network.xml; do ${DESTDIR}/${LIBDIR}/scripts/install-config "$$i" "" "${CONFIGINSTALLMETHOD}"; done
	
	for module in `ls modules-xml`; do mod=`echo $${module} | sed 's/.xml//g'`; ln -sf ../../lib/einit/scripts/einit.d_erc.sh ${DESTDIR}/${ETCDIR}/init.d/$${mod}; done
	ln -sfn ../../lib/einit/defaults ${DESTDIR}/${ETCDIR}/defaults
