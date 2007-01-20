include ../config.mk
include depend.mk

all: einit einit-control default-modules

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk
	touch modules/depend.mk
	cd modules && ${MAKE} depend

clean:
	rm -f *.o einit-xml einit-control einit depend.mk
	cd modules && ${MAKE} clean

einit-xml: einit.o module.o bitch.o config-xml-expat.o utility.o scheduler.o event.o module-logic.o config.o module-configuration.o tree.o set.o
	$(CLD) einit.o module.o bitch.o config-xml-expat.o utility.o scheduler.o event.o module-logic.o config.o module-configuration.o tree.o set.o $(XDYNAMIC) -o einit-xml $(STATIC) -ldl -lexpat $(LPA)

einit: einit-xml
	ln -sf einit-xml einit

einit-control: einit-control.o bitch.o utility.o event.o tree.o set.o
	$(CLD) einit-control.o bitch.o utility.o event.o tree.o set.o $(XDYNAMIC) -o einit-control $(STATIC) -ldl $(LPA)

install: all
	${INSTALL} ${BINIPARAM} -d ${DESTDIR}/${SBINDIR}
	${INSTALL} ${BINIPARAM} einit ${DESTDIR}/${SBINDIR}
	${INSTALL} ${BINIPARAM} einit-control ${DESTDIR}/${SBINDIR}
	ln -sf einit-control ${DESTDIR}/${SBINDIR}/power
	ln -sf einit-control ${DESTDIR}/${SBINDIR}/erc
	cd modules && ${MAKE} install

default-modules: einit
	cd modules && ${MAKE} all

einit-control.o: einit-control.c
	$(CCC) -c einit-control.c

einit.o: einit.c
	$(CCC) -c einit.c

config.o: config.c
	$(CCC) -c config.c

tree.o: tree-linear.c tree-bst-splay.c
	$(CCC) -c tree-${TREETYPE}.c -o tree.o

set.o: set-lowmem.c
	$(CCC) -c set-${SETTYPE}.c -o set.o

event.o: event.c
	$(CCC) -c event.c

scheduler.o: scheduler.c
	$(CCC) -c scheduler.c

module.o: module.c
	$(CCC) -c module.c

module-logic.o: module-logic.c
	$(CCC) -c module-logic.c

module-configuration.o: module-configuration.c
	$(CCC) -c module-configuration.c

bitch.o: bitch.c
	$(CCC) -c bitch.c

config-xml-expat.o: config-xml-expat.c
	$(CCC) -c config-xml-expat.c

utility.o: utility.c
	$(CCC) -c utility.c
