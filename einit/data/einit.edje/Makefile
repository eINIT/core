include ../../config.mk

all: einit-default-splash.edj

install: all
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${ETCDIR}/edje
	${INSTALL} ${IPARAM} einit-default-splash.edj ${DESTDIR}/${ETCDIR}/edje

einit-default-splash.edj: splash.edc
	edje_cc -id . -fd . splash.edc -o einit-default-splash.edj
