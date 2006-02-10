einit-xml: main.o module.o config-xml-expat.o
	$(CC) main.o module.o config-xml-expat.o -o einit-xml -lexpat

main.o: main.c
	$(CC) -c main.c

module.o: module.c
	$(CC) -c module.c

config-xml-expat.o: config-xml-expat.c
	$(CC) -c config-xml-expat.c
