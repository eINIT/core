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

einit-xml: einit.o module.o bitch.o config-xml-expat.o utility.o scheduler.o event.o
	$(CCC) einit.o module.o bitch.o config-xml-expat.o utility.o scheduler.o event.o $(XDYNAMIC) -o einit-xml -ldl -lexpat $(LPA)

einit: einit-xml
	ln -sf einit-xml einit

einit-control: einit-control.o bitch.o utility.o
	$(CCC) einit-control.o bitch.o utility.o $(XDYNAMIC) -o einit-control -ldl

install: all
	${INSTALL} ${BINIPARAM} -d ${SBINDIR}
	${INSTALL} ${BINIPARAM} einit ${SBINDIR}
	${INSTALL} ${BINIPARAM} einit-control ${SBINDIR}
	ln -sf einit-control ${SBINDIR}/power
	ln -sf einit-control ${SBINDIR}/erc
	cd modules && ${MAKE} install

default-modules: einit
	cd modules && ${MAKE} all

einit-control.o: einit-control.c
	$(CCC) -c einit-control.c

einit.o: einit.c
	$(CCC) -c einit.c

event.o: event.c
	$(CCC) -c event.c

scheduler.o: scheduler.c
	$(CCC) -c scheduler.c

module.o: module.c
	$(CCC) -c module.c

bitch.o: bitch.c
	$(CCC) -c bitch.c

config-xml-expat.o: config-xml-expat.c
	$(CCC) -c config-xml-expat.c

utility.o: utility.c
	$(CCC) -c utility.c
