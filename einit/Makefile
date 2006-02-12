include config.mk

einit: einit-xml
	cp -a src/einit-xml einit

einit-xml:
	cd src && ${MAKE} einit-xml

clean:
	rm einit src/einit-xml src/*.o

all: einit
