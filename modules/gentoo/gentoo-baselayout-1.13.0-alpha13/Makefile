# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

CC ?= gcc

CFLAGS ?= -Wall -O2 -pipe
CFLAGS += -pedantic -std=c99 \
    -Wall -Wunused -Wimplicit -Wshadow -Wformat=2 \
    -Wmissing-declarations -Wno-missing-prototypes -Wwrite-strings \
    -Wbad-function-cast -Wnested-externs -Wcomment -Winline \
    -Wchar-subscripts -Wcast-align -Wno-format-nonliteral

# Early GCC versions don't support these flags, so you may need to comment
# this line out
CFLAGS += -Wsequence-point -Wextra -Wdeclaration-after-statement

# For debugging
#CFLAGS += -ggdb

DESTDIR =
LIB = lib

LIBRCSOVER = 0
LIBRCSO = librc.so.$(LIBRCSOVER)
LIBRCOBJS= rc-misc.o librc.o librc-depend.o

LIB_TARGETS = $(LIBRCSO)
BIN_TARGETS =
SBIN_TARGETS = consoletype rc-depend start-stop-daemon runscript
SYS_WHITELIST = env_whitelist

TARGET = $(LIBRCOBJS)

ifeq ($(OS),)
OS=$(shell uname -s)
ifneq ($(OS),Linux)
OS=BSD
endif
endif
ifeq ($(OS),Linux)
LDLIBS_RS  = -ldl
endif
ifeq ($(OS),BSD)
LDLIBS_SSD = -lkvm
endif

HAVE_PAM = 
ifdef HAVE_PAM
CFLAGS_SSD = -DHAVE_PAM -lpam
endif

# We also define _BSD_SOURCE so both Linux and the BSDs get a few
# handy functions which makes our lives a lot easier
override CFLAGS += -DLIBDIR=\"$(LIB)\" -D_BSD_SOURCE

# IMPORTANT!!!
# Remove this when releasing as it's a security risk
# However, this does save us using libtool
override LDFLAGS += -Wl,-rpath .

all: $(TARGET)

runscript: LDLIBS += $(LDLIBS_RS)
runscript: $(LIBRCSO) rc-misc.o runscript.o

start-stop-daemon: CFLAGS += $(CFLAGS_SSD)
start-stop-daemon: LDLIBS += $(LDLIBS_SSD)
start-stop-daemon: start-stop-daemon.o

$(LIBRCOBJS): strlist.h
$(LIBRCOBJS): CFLAGS += -fPIC

$(LIBRCSO): $(LIBRCOBJS) 
	$(CC) -shared -Wl,-soname,$(LIBRCSO) -o $(LIBRCSO) $(LIBRCOBJS)
	ln -sf $(LIBRCSO) librc.so

rc-depend: $(LIBRCSO) strlist.h
rc-depend: LDLIBS += -L. -lrc
rc-depend: rc-depend.o

install: $(TARGET)
	install -m 0755 -d $(DESTDIR)/$(LIB)
	install -m 0755 $(LIB_TARGETS) $(DESTDIR)/$(LIB)
	ln -sf $(LIBRCSO) $(DESTDIR)/$(LIB)/librc.so
	install -m 0755 -d $(DESTDIR)/usr/include
	install -m 0644 rc.h $(DESTDIR)/usr/include 
	install -m 0755 -d $(DESTDIR)/sbin
	install -m 0755 $(SBIN_TARGETS) $(DESTDIR)/sbin
	install -m 0755 -d $(DESTDIR)/$(LIB)/rcscripts/conf.d
	install -m 0644 $(SYS_WHITELIST) $(DESTDIR)/$(LIB)/rcscripts/conf.d
	if test "$(HAVE_PAM)" != "" ; then \
		install -m 0755 -d $(DESTDIR)/etc/pam.d ; \
		install -m 0644 start-stop-daemon.pam $(DESTDIR)/etc/pam.d/start-stop-daemon ; \
	fi

clean:
	rm -f $(TARGET)
	rm -f *.o *~ *.core *.so
