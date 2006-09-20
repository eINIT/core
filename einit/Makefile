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

documentation-api:
	rm -rf build/documentation/hacking-einit
	doxygen Doxyfile

documentation: documentation-html documentation-api

install: all
	cd src && ${MAKE} install
	cd data && ${MAKE} install

depend:
	touch src/depend.mk
	cd src && ${MAKE} depend

clean:
	rm -f einit
	cd src && ${MAKE} clean
