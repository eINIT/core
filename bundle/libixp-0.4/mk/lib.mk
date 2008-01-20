PTARG = ${ROOT}/lib/${TARG}
LIB = ${PTARG}.a
OFILES = ${OBJ:=.o}

all: ${HFILES} ${LIB} 

install: ${PTARG}.install
uninstall: ${PTARG}.uninstall
clean: libclean
depend: ${OBJ:=.depend}

libclean:
	for i in ${LIB} ${OFILES}; do \
		rm -f $$i; \
	done 2>/dev/null || true

printinstall:
	echo 'Install directories:'
	echo '	Lib: ${LIBDIR}'

${LIB}: ${OFILES}
	echo AR $$($(ROOT)/util/cleanname $(BASE)/$@)
	mkdir ${ROOT}/lib 2>/dev/null || true
	${AR} $@ ${OFILES}

include ${ROOT}/mk/common.mk
