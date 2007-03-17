include ../../config.mk

all: compatibility-mod-sysv-init-d.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so *.o depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} compatibility-mod-sysv-init-d.so ${DESTDIR}/${MODDIR}

linux-network.so: linux-network.c
	${CCL} -c linux-network.c
	${LLD} linux-network.o -o linux-network.so

compatibility-mod-sysv-init-d.so: compatibility-mod-sysv-init-d.c
	${CCL} -c compatibility-mod-sysv-init-d.c
	${LLD} compatibility-mod-sysv-init-d.o -o compatibility-mod-sysv-init-d.so
