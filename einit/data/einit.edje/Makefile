include ../../config.mk

all: einit.edj

install: all
	${INSTALL} ${IPARAM} -d ${ETCDIR}/edje
	${INSTALL} ${IPARAM} einit.edj ${ETCDIR}/edje

einit.edj: einit.edc
	edje_cc einit.edc
