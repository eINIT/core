include ../config.mk
include depend.mk

all: einit-library

depend:
	${CC} ${INCLUDE} -M *.cpp > depend.mk

clean:
	rm -f *.o *.so *.la depend.mk

einit-library: libeinit++.la

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${ULIBDIR}
	${INSTALL} ${SOIPARAM} libeinit++.la ${DESTDIR}/${ULIBDIR}

libeinit++.la:
	