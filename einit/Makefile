include config.mk

all:
	cd src && ${MAKE} all
	cd data && ${MAKE} all

documentation-html:
	cd documentation && ${MAKE} documentation-html

documentation-man:
	cd documentation && ${MAKE} documentation-man

documentation-pdf:
	cd documentation && ${MAKE} documentation-pdf

install: all
	cd src && ${MAKE} install
	cd data && ${MAKE} install

documentation_install: documentation-html

depend:
	touch src/depend.mk
	cd src && ${MAKE} depend

clean:
	rm -f einit
	cd src && ${MAKE} clean
