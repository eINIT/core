include ../config.mk

documentation-html:
	mkdir -p ../build/documentation/html
	mkdir -p ../build/documentation/html/reference

documentation-man:
	mkdir -p ../build/documentation/man
	docbook2man --output ../build/documentation/man manual-tinyscheme.sgml

documentation-pdf:
	mkdir -p ../build/documentation

documentation-api:

all: documentation-html documentation-man documentation-pdf

install: all
