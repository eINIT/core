include ../../../config.mk
include depend.mk

all: einit-efl-feedback-evas-edje.so

depend:
	${CC} ${INCLUDE} -M *.c > depend.mk

clean:
	rm -f *.so depend.mk

install: all
	${INSTALL} ${SOIPARAM} -d ${DESTDIR}/${MODDIR}
	${INSTALL} ${SOIPARAM} einit-efl-feedback-evas-edje.so ${DESTDIR}/${MODDIR}

einit-efl-feedback-evas-edje.so: einit-efl-feedback-evas-edje.c
	${CCL} einit-efl-feedback-evas-edje.c -o einit-efl-feedback-evas-edje.so\
      `ecore-config --cflags --libs` `evas-config --cflags --libs` `edje-config --cflags --libs`
