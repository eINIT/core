include ../config.mk
include depend.mk

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	touch modules/depend.mk
	cd modules && ${MAKE} depend

clean:
	rm -f *.o einit-* einit depend.mk
	cd modules && ${MAKE} clean	

einit-xml: main.o module.o bitch.o config-xml-expat.o utility.o
	$(CCC) main.o module.o bitch.o config-xml-expat.o utility.o $(XDYNAMIC) -o einit-xml -ldl -lexpat $(LPA)
	ln -sf einit-xml einit
	$(MAKE) default-modules

install:
	cd modules && ${MAKE} install

default-modules:
	cd modules && ${MAKE} all

main.o: main.c
	$(CCC) -c main.c

module.o: module.c
	$(CCC) -c module.c

bitch.o: bitch.c
	$(CCC) -c bitch.c

config-xml-expat.o: config-xml-expat.c
	$(CCC) -c config-xml-expat.c

utility.o: utility.c
	$(CCC) -c utility.c
