include ../config.mk
include depend.mk

all: einit power default-modules

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	touch modules/depend.mk
	cd modules && ${MAKE} depend

clean:
	rm -f *.o einit-* einit power depend.mk
	cd modules && ${MAKE} clean	

einit-xml: einit.o module.o bitch.o config-xml-expat.o utility.o
	$(CCC) einit.o module.o bitch.o config-xml-expat.o utility.o $(XDYNAMIC) -o einit-xml -ldl -lexpat $(LPA)

einit: einit-xml
	ln -sf einit-xml einit

power: power.o bitch.o utility.o
	$(CCC) power.o bitch.o utility.o $(XDYNAMIC) -o power

install:
	${INSTALL} ${BINIPARAM} -d ${SBINDIR}
	${INSTALL} ${BINIPARAM} einit ${SBINDIR}
	${INSTALL} ${BINIPARAM} power ${SBINDIR}
	cd modules && ${MAKE} install

default-modules: einit
	cd modules && ${MAKE} all

power.o: power.c
	$(CCC) -c power.c

einit.o: einit.c
	$(CCC) -c einit.c

module.o: module.c
	$(CCC) -c module.c

bitch.o: bitch.c
	$(CCC) -c bitch.c

config-xml-expat.o: config-xml-expat.c
	$(CCC) -c config-xml-expat.c

utility.o: utility.c
	$(CCC) -c utility.c
