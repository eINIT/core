include config.mk

all:
	cd scripts && ${MAKE} all
	cd build && ${MAKE} all
	cd data && ${MAKE} all

test:
	cd build && ${MAKE} test

documentation-html:
	cd documentation && ${MAKE} documentation-html

documentation-man:
	cd documentation && ${MAKE} documentation-man

documentation-pdf:
	cd documentation && ${MAKE} documentation-pdf

documentation-api:
	rm -Rf build/documentation/hacking-einit
	doxygen Doxyfile

documentation: documentation-man documentation-html documentation-api

install: all
	cd scripts && ${MAKE} install
	cd build && ${MAKE} install
	cd data && ${MAKE} install

depend:

clean:
	rm -Rf build
