include ../config.mk

all:

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/local
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/modules
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCXDIR}/einit.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCXDIR}/econf.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-xml
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-xml/templates-net
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -m 0644 einit.xml ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -m 0644 modules-xml/*.xml ${DESTDIR}/${LIBDIR}/modules-xml
	${INSTALL} ${IPARAM} -m 0644 modules-xml/templates-net/*.xml ${DESTDIR}/${LIBDIR}/modules-xml/templates-net
	${INSTALL} ${IPARAM} -m 0755 scripts/* ${DESTDIR}/${LIBDIR}/scripts
	
	if test -e ${ETCDIR}/local.xml; then ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}/local.xml.example; else ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}; fi
	if test -e ${ETCDIR}/fstab.xml; then ${INSTALL} ${IPARAM} -m 0644 fstab.xml.example ${DESTDIR}/${ETCDIR}/fstab.xml.example; else ${INSTALL} ${IPARAM} -m 0644 fstab.xml.example ${DESTDIR}/${ETCDIR}/fstab.xml.example; ${INSTALL} ${IPARAM} -m 0644 fstab.xml ${DESTDIR}/${ETCDIR}; fi
	for module in `ls modules-xml`; do mod=`echo $${module} | sed 's/.xml//g'`; ln -sf ../../lib/einit/scripts/einit.d_erc.sh ${DESTDIR}/${ETCXDIR}/einit.d/$${mod}; done
