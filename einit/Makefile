include config.mk

einit: einit-xml
	cp -p src/einit-xml einit

install: einit
	cd src && ${MAKE} install

einit-xml:
	cd src && ${MAKE} einit-xml

clean:
	rm einit src/einit-xml src/*.o

all: einit
