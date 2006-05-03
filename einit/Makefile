include config.mk

all:
	cd src && ${MAKE} all

documentation:

install: all
	${INSTALL} ${IPARAM} -d ${ETCDIR}
	${INSTALL} ${IPARAM} data/default.xml ${ETCDIR}
	cd src && ${MAKE} install

depend:
	touch src/depend.mk
	cd src && ${MAKE} depend

clean:
	rm -f einit
	cd src && ${MAKE} clean
