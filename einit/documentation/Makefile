include ../config.mk

all: documentation

documentation-html:
	mkdir -p ../build/documentation/html
	mkdir -p ../build/documentation/html/reference
	rm -f ../build/documentation/html/*.htm* ../build/documentation/html/reference/*.htm*
	docbook2html einit-0.24.4.sgml -d "einit.dsl#html" --output ../build/documentation/html
	docbook2html manual-einit.sgml -d "einit.dsl#html" --output ../build/documentation/html/reference --nochunks
	docbook2html manual-einit-control.sgml -d "einit.dsl#html" --output ../build/documentation/html/reference --nochunks

documentation-man:
	mkdir -p ../build/documentation/man
	docbook2man --output ../build/documentation/man manual-einit.sgml
	docbook2man --output ../build/documentation/man manual-einit-control.sgml

documentation-pdf:
	mkdir -p ../build/documentation
	docbook2pdf einit.sgml --output ../build/documentation
