include ../config.mk

all:

install:
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/local
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/modules
	${INSTALL} ${IPARAM} -d ${DESTDIR}/etc/einit.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/etc/econf.d
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/modules-xml
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${LIBDIR}/scripts
	${INSTALL} ${IPARAM} -m 0644 einit.xml ${DESTDIR}/${LIBDIR}
	${INSTALL} ${IPARAM} -m 0644 modules-xml/*.xml ${DESTDIR}/${LIBDIR}/modules-xml
	${INSTALL} ${IPARAM} -m 0644 einit.d_erc.sh ${DESTDIR}/${LIBDIR}/scripts
	
	if test -e ${ETCDIR}/local.xml; then ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}/local.xml.example; else ${INSTALL} ${IPARAM} -m 0644 local.xml ${DESTDIR}/${ETCDIR}; fi
	modules=`ls modules-xml`; for module in ${modules}; do mod=`echo ${module} | sed 's/.xml//g'`; echo ${mod}; ln -s ${DESTDIR}/${LIBDIR}/scripts/einit.d_erc.sh ${DESTDIR}/etc/einit.d/${mod}; done
