include ../config.mk

einit-xml: main.o module.o config-xml-expat.o modules
	$(CCC) main.o module.o config-xml-expat.o -o einit-xml -lexpat

modules:
	cd modules && ${MAKE}

main.o: main.c
	$(CCC) -c main.c

module.o: module.c
	$(CCC) -c module.c

config-xml-expat.o: config-xml-expat.c
	$(CCC) -c config-xml-expat.c
