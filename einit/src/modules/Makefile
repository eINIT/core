include ../../config.mk
include depend.mk

all: dummy.so

depend:
	${CC} -M *.c > depend.mk

clean:
	rm *.so depend.mk

install: dummy.so
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
	${INSTALL} ${SOIPARAM} dummy.so ${MODDIR}

dummy.so: dummy.c
	${CCL} dummy.c -o dummy.so
