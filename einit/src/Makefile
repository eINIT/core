include ../config.mk
include depend.mk

all: einit einit-control default-modules

depend:
	ln -sf module-logic-${MODULELOGICTYPE}.h include/einit/module-logic.h
	ln -sf set-${SETTYPE}.h include/einit/set.h
	ln -sf tree-${TREETYPE}.h include/einit/tree.h
	${CC} ${INCLUDE} -M *.c > depend.mk
	touch modules/depend.mk
	cd modules && ${MAKE} depend

clean:
	rm -f *.o einit-xml einit-control einit depend.mk
	cd modules && ${MAKE} clean

einit-xml: $(CORELINK)
	$(CLD) $(CORELINK) $(XDYNAMIC) -o einit-xml $(STATIC) -ldl $(CORELIBS) $(LPA)

einit: einit-xml
	ln -sf einit-xml einit

einit-control: einit-control.o bitch.o utility.o event.o tree.o set.o config.o
	$(CLD) einit-control.o bitch.o utility.o event.o tree.o set.o config.o $(XDYNAMIC) -o einit-control $(STATIC) -ldl $(LPA)

install: all
	${INSTALL} ${BINIPARAM} -d ${DESTDIR}/${SBINDIR}
	${INSTALL} ${BINIPARAM} einit ${DESTDIR}/${SBINDIR}
	${INSTALL} ${BINIPARAM} einit-control ${DESTDIR}/${SBINDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${INCLUDEDIR}/einit
	${INSTALL} ${IPARAM} -m 0644 include/einit/*.h ${DESTDIR}/${INCLUDEDIR}/einit
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${INCLUDEDIR}/einit-modules
	${INSTALL} ${IPARAM} -m 0644 include/einit-modules/*.h ${DESTDIR}/${INCLUDEDIR}/einit-modules
	if test -n "${EXTERNALMODULES}"; then ${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}; ${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${BOOTSTRAPDIR}; fi
	if test -n "`echo ${EXTERNALMODULES}|grep einit-module-logic-v3`"; then ${INSTALL} ${SOIPARAM} einit-module-logic-v3.so ${DESTDIR}/${MODDIR}; fi
	if test -n "`echo ${EXTERNALMODULES}|grep einit-config-xml-expat`"; then ${INSTALL} ${SOIPARAM} einit-configuration-xml-expat.so ${DESTDIR}/${BOOTSTRAPDIR}; fi
	ln -sf einit-control ${DESTDIR}/${SBINDIR}/power
	ln -sf einit-control ${DESTDIR}/${SBINDIR}/erc
	cd modules && ${MAKE} install

default-modules: einit $(EXTERNALMODULES)
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

module-logic.o: module-logic-v2.c module-logic-v3.c
	$(CCC) -c module-logic-${MODULELOGICTYPE}.c -o module-logic.o

einit-module-logic-v3: einit-module-logic-v3.so

einit-module-logic-v3.so: module-logic-v3.c
	${CCL} -c module-logic-v3.c
	${LLD} module-logic-v3.o -o einit-module-logic-v3.so;

einit-config-xml-expat: einit-configuration-xml-expat.so

einit-configuration-xml-expat.so: config-xml-expat.c
	${CCL} -c config-xml-expat.c
	${LLD} config-xml-expat.o -o einit-configuration-xml-expat.so -lexpat

module-configuration.o: module-configuration.c
	$(CCC) -c module-configuration.c

bitch.o: bitch.c
	$(CCC) -c bitch.c

config-xml-expat.o: config-xml-expat.c
	$(CCC) -c config-xml-expat.c

utility.o: utility.c
	$(CCC) -c utility.c
