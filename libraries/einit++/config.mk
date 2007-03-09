PREFIX = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox
ETCDIR = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox/etc/einit
LIBDIR = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox/lib/einit
MODDIR = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox/lib/einit/modules
BINDIR = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox/bin
SBINDIR = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox/sbin
INCLUDEDIR = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox/include
ULIBDIR = /home/mdeininger/projects/einit/modules/gentoo/../../einit/sandbox/lib

INSTALL = install
IPARAM =
SOIPARAM = ${IPARAM}
BINIPARAM = ${IPARAM}

PMODULES = compatibility configuration linux
OPTIONS = 
INCLUDE = 

XDYNAMIC = -Wl,-export-dynamic
BUILDNUMBER = 
ISSVN = 0

CC = x86_64-pc-linux-gnu-gcc
CFLAGS = -O2 -pipe -march=athlon64 -DPOSIXREGEX -I/home/mdeininger/projects/einit/modules/gentoo/../../einit/src/include/ -DPOSIX -DLINUX
LDFLAGS = 
CCC = ${CC} ${INCLUDE} ${CFLAGS} -DISSVN=${ISSVN}
CCL = ${CC} ${INCLUDE} ${CFLAGS} -fPIC -DISSVN=${ISSVN}
CLD = ${CC} ${LDFLAGS}
LLD = ${CC} ${LDFLAGS} -shared
XLLD = -shared
STATIC = 
LPA = -lpthread
