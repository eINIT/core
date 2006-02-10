CC = gcc

einit: src/einit-xml
	cp -a src/einit-xml einit

src/einit-xml:
	cd src; make einit-xml

clean:
	rm einit src/einit-xml src/*.o

all: einit
