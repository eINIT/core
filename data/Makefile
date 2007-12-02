include ../config.mk

all:

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/local
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/modules
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/init.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/subsystems.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/rules.edev
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/hotplug.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/conf.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/../dbus-1
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/../dbus-1/system.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/schemata
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/conf.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/subsystems.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/defaults/rules.edev
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-scheme
#	${INSTALL} ${IPARAM} -m 0644 einit.xml ${DESTDIR}/${LIBDIR}
	sed -e "s/\/lib/\/${ELIBDIR}/" < einit.xml >einit.xml.n
	${INSTALL} ${IPARAM} -m 0644 einit.xml.n ${DESTDIR}/${LIBDIR}/einit.xml
	${INSTALL} ${IPARAM} -m 0755 scripts/* ${DESTDIR}/${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -m 0644 schema/*.rn? ${DESTDIR}/${LIBDIR}/schemata
	${INSTALL} ${IPARAM} -m 0644 einit.conf ${DESTDIR}/${ETCDIR}/../dbus-1/system.d
	
	for i in subsystems.d/*.xml; do ${DESTDIR}/${LIBDIR}/scripts/install-config "$$i" subsystems.d "${CONFIGINSTALLMETHOD}"; done
	for i in rules.edev/*.xml; do ${DESTDIR}/${LIBDIR}/scripts/install-config "$$i" rules.edev "${CONFIGINSTALLMETHOD}"; done
	for i in local.xml fstab.xml modes.xml network.xml; do ${DESTDIR}/${LIBDIR}/scripts/install-config "$$i" "" "${CONFIGINSTALLMETHOD}"; done
	
	${INSTALL} ${IPARAM} -m 0644 modules-scheme/*.scm ${DESTDIR}/${LIBDIR}/modules-scheme
	ln -sfn ../../../sbin/einit ${DESTDIR}/${ETCDIR}/init.d/einit

	for i in conf.d/*; do ${DESTDIR}/${LIBDIR}/scripts/install-config "$$i" conf.d "${CONFIGINSTALLMETHOD}"; done
