include ../../config.mk

all: einit.edj

install: all
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/edje
	${INSTALL} ${IPARAM} einit.edj ${DESTDIR}/${ETCDIR}/edje

einit.edj: einit.edc
	edje_cc einit.edc
