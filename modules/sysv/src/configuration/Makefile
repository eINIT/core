include ../../config.mk

all: configuration-secondary-sh-style.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so *.o depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} configuration-secondary-sh-style.so ${DESTDIR}/${MODDIR}

configuration-secondary-sh-style.so: configuration-secondary-sh-style.c
	${CCL} -c configuration-secondary-sh-style.c
	${LLD} configuration-secondary-sh-style.o -o configuration-secondary-sh-style.so
