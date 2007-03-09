include ../config.mk
include depend.mk

all: einit-library

depend:
	${CC} ${INCLUDE} -M *.cc > depend.mk

clean:
	rm -f *.o *.so *.la depend.mk

einit-library: libeinit++.a

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${ULIBDIR}
	${INSTALL} ${SOIPARAM} libeinit++.a ${DESTDIR}/${ULIBDIR}
	${INSTALL} ${IPARAM} -d ${DESTDIR}/${INCLUDEDIR}/einit++
	${INSTALL} ${IPARAM} -m 0644 include/einit++/*.h ${DESTDIR}/${INCLUDEDIR}/einit++

libeinit++.a: einit-module.o
	ln -f einit-module.o libeinit++.a

einit-module.o: einit-module.cc
	$(XXL) -c einit-module.cc
