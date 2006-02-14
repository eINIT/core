include ../config.mk

einit-xml: main.o module.o bitch.o config-xml-expat.o default-modules
	$(CCC) main.o module.o bitch.o config-xml-expat.o -o einit-xml -ldl -lexpat

install: default-modules
	cd modules && ${MAKE} install

default-modules:
	cd modules && ${MAKE} all

main.o: main.c
	$(CCC) -c main.c

module.o: module.c
	$(CCC) -rdynamic -c module.c

bitch.o: bitch.c
	$(CCC) -c bitch.c

config-xml-expat.o: config-xml-expat.c
	$(CCC) -c config-xml-expat.c
