include config.mk

einit: einit-xml
	cp -p src/einit-xml einit

install: einit
	${INSTALL} ${BINIPARAM} -d ${SBINDIR}
	${INSTALL} ${BINIPARAM} einit ${SBINDIR}
	${INSTALL} ${IPARAM} -d ${ETCDIR}
	${INSTALL} ${IPARAM} data/default.xml ${ETCDIR}
	cd src && ${MAKE} install

einit-xml:
	cd src && ${MAKE} einit-xml

depend:
	touch src/depend.mk
	cd src && ${MAKE} depend

clean:
	rm -f einit
	cd src && ${MAKE} clean

all: einit
