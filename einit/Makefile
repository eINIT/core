include config.mk

all:
	cd src && ${MAKE} all

documentation-html:
	cd documentation && ${MAKE} documentation-html

documentation-man:
	cd documentation && ${MAKE} documentation-man

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
