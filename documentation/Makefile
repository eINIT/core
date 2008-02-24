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

einit-0.40.0-html: documentation-dirs einit-0.40.0.xml
	rm -f ../build/documentation/html/*.htm*
	cp -rf images/* ../build/documentation/html/images
	cp -rf css/* ../build/documentation/html/css
	docbook2html -w no-sgmldecl einit-0.40.0.xml -d "einit.dsl#html" --output ../build/documentation/html
	for i in ../build/documentation/html/*.htm*; do tidy -i -asxhtml -utf8 -m $${i}; done; true

manual-einit-html: documentation-dirs manual-einit.xml
	rm -f ../build/documentation/html/reference/manual-einit.htm*
	docbook2html manual-einit.xml -d "einit.dsl#html" --output ../build/documentation/html/reference --nochunks
	tidy -i -asxhtml -utf8 -m ../build/documentation/html/reference/manual-einit.htm*; true

manual-einit-man: documentation-dirs manual-einit.xml
	docbook2man --output ../build/documentation/man manual-einit.xml

einit-0.40.0-pdf: documentation-dirs einit-0.40.0.xml
	docbook2pdf einit-0.40.0.xml --output ../build/documentation

documentation-pdf: einit-0.40.0-pdf
documentation-man: manual-einit-man
documentation-html: einit-0.40.0-html manual-einit-html

documentation: documentation-man documentation-html
