include ../config.mk

all: documentation

clean:
	rm -rf ../build/documentation

documentation-dirs:
	mkdir -p ../build/documentation
	mkdir -p ../build/documentation/html
	mkdir -p ../build/documentation/html/reference
	mkdir -p ../build/documentation/html/images
	mkdir -p ../build/documentation/html/css
	mkdir -p ../build/documentation/man

einit-0.24.4-html: documentation-dirs einit-0.24.4.xml
	rm -f ../build/documentation/html/*.htm*
	cp -rf images/* ../build/documentation/html/images
	cp -rf css/* ../build/documentation/html/css
	docbook2html -w no-sgmldecl einit-0.24.4.xml -d "einit.dsl#html" --output ../build/documentation/html
	for i in ../build/documentation/html/*.htm*; do tidy -i -asxhtml -utf8 -m $${i}; done; true

manual-einit-html: documentation-dirs manual-einit.sgml
	rm -f ../build/documentation/html/reference/manual-einit.htm*
	docbook2html manual-einit.sgml -d "einit.dsl#html" --output ../build/documentation/html/reference --nochunks
	tidy -i -asxhtml -utf8 -m ../build/documentation/html/reference/manual-einit.htm*; true

manual-einit-control-html: documentation-dirs manual-einit-control.sgml
	rm -f ../build/documentation/html/reference/manual-einit-control.htm*
	docbook2html manual-einit-control.sgml -d "einit.dsl#html" --output ../build/documentation/html/reference --nochunks
	tidy -i -asxhtml -utf8 -m ../build/documentation/html/reference/manual-einit-control.htm*; true

einit-0.24.4-html-berlios: documentation-dirs einit-0.24.4.xml
	rm -f ../build/documentation/html/*.htm*
	cp -rf images/* ../build/documentation/html/images
	cp -rf css/* ../build/documentation/html/css
	docbook2html -w no-sgmldecl einit-0.24.4.xml -d "einit-berlios.dsl#html" --output ../build/documentation/html
	for i in ../build/documentation/html/*.htm*; do tidy -i -asxhtml -utf8 -m $${i}; done; true

manual-einit-html-berlios: documentation-dirs manual-einit.sgml
	rm -f ../build/documentation/html/reference/manual-einit.htm*
	docbook2html manual-einit.sgml -d "einit-berlios.dsl#html" --output ../build/documentation/html/reference --nochunks
	tidy -i -asxhtml -utf8 -m ../build/documentation/html/reference/manual-einit.htm*; true

manual-einit-control-html-berlios: documentation-dirs manual-einit-control.sgml
	rm -f ../build/documentation/html/reference/manual-einit-control.htm*
	docbook2html manual-einit-control.sgml -d "einit-berlios.dsl#html" --output ../build/documentation/html/reference --nochunks
	tidy -i -asxhtml -utf8 -m ../build/documentation/html/reference/manual-einit-control.htm*; true

manual-einit-man: documentation-dirs manual-einit.sgml
	docbook2man --output ../build/documentation/man manual-einit.sgml

manual-einit-control-man: documentation-dirs manual-einit-control.sgml
	docbook2man --output ../build/documentation/man manual-einit-control.sgml

einit-0.24.4-pdf: documentation-dirs einit-0.24.4.xml
	docbook2pdf einit-0.24.4.xml --output ../build/documentation

documentation-pdf: einit-0.24.4-pdf
documentation-man: manual-einit-man manual-einit-control-man
documentation-html: einit-0.24.4-html manual-einit-html manual-einit-control-html
documentation-html-berlios: einit-0.24.4-html-berlios manual-einit-html-berlios manual-einit-control-html-berlios

documentation: documentation-pdf documentation-man documentation-html
