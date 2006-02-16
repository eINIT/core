include ../../config.mk

all: dummy.so

install: dummy.so
	${INSTALL} ${SOIPARAM} -d ${MODDIR}
	${INSTALL} ${SOIPARAM} dummy.so ${MODDIR}

dummy.so: dummy.c
	${CCL} dummy.c -o dummy.so
