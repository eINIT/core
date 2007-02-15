include config.mk

all: librc-objects
	cd src && ${MAKE} all
	cd data && ${MAKE} all
	cd scripts && ${MAKE} all

documentation-html:
	cd documentation && ${MAKE} documentation-html

documentation-man:
	cd documentation && ${MAKE} documentation-man

documentation-pdf:
	cd documentation && ${MAKE} documentation-pdf

documentation-api:
#	rm -Rf build/documentation/hacking-einit
#	doxygen Doxyfile

documentation: documentation-man documentation-html documentation-api

install: all
	cd src && ${MAKE} install
	cd data && ${MAKE} install
	cd scripts && ${MAKE} install

depend:
	touch src/depend.mk
	cd src && ${MAKE} depend

clean:
	rm -f einit
	cd src && ${MAKE} clean

librc-objects:
	cd gentoo-baselayout-1.13.0-alpha13 && ${MAKE}
	cp -a gentoo-baselayout-1.13.0-alpha13/*.o src/
