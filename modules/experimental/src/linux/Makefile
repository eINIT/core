include ../../config.mk
include depend.mk

all: linux-network.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so *.o depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} linux-network.so ${DESTDIR}/${MODDIR}

linux-network.so: linux-network.c
	${CCL} -c linux-network.c
	${LLD} linux-network.o -o linux-network.so
