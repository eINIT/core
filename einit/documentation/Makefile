include ../config.mk

all: documentation

documentation-html:
	mkdir -p ../build/documentation/html
	docbook2html einit.sgml --output ../build/documentation/html

documentation-man:
	mkdir -p ../build/documentation/man
	docbook2man einit.sgml --output ../build/documentation/man
