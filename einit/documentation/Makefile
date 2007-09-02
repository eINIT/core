include ../config.mk

all: documentation

documentation-html:
	mkdir -p ../build/documentation
	mkdir -p ../build/documentation/html
	mkdir -p ../build/documentation/html/reference
	mkdir -p ../build/documentation/html/images
	mkdir -p ../build/documentation/html/css
	rm -f ../build/documentation/html/*.htm* ../build/documentation/html/reference/*.htm*
	docbook2html -w no-sgmldecl einit-0.24.4.xml -d "einit.dsl#html" --output ../build/documentation/html
	for i in ../build/documentation/html/*.htm*; do tidy -i -asxhtml -utf8 -m $${i}; done; true
	cp -rf images/* ../build/documentation/html/images
	cp -rf css/* ../build/documentation/html/css
	docbook2html manual-einit.sgml -d "einit.dsl#html" --output ../build/documentation/html/reference --nochunks
	docbook2html manual-einit-control.sgml -d "einit.dsl#html" --output ../build/documentation/html/reference --nochunks

documentation-man:
	mkdir -p ../build/documentation/man
	docbook2man --output ../build/documentation/man manual-einit.sgml
	docbook2man --output ../build/documentation/man manual-einit-control.sgml

documentation-pdf:
	mkdir -p ../build/documentation
	docbook2pdf einit-0.24.4.xml --output ../build/documentation
