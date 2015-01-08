CLANGUNWARN:=$(shell if cc 2>&1 | grep clang >/dev/null; then echo "-Qunused-arguments"; fi)
CFLAGS:=-std=c99 -Wall -Werror -Wno-unused-function $(CLANGUNWARN) $(CFLAGS) -I../vm/

UNAME := $(shell uname)
ifneq ($(UNAME),Darwin)
LINK=-shared
CFLAGS:=$(CFLAGS) -fPIC
else
LINK=-dynamiclib -flat_namespace -undefined suppress
endif

ifeq ($(BUILD),release)
	CFLAGS+=-O3
else
	CFLAGS+=-g -O
endif
# doesn't work with coverage tool
ifeq ($(GCOV),1)
	#CFLAGS+=-fprofile-arcs -ftest-coverage
	#LINK+=-fprofile-arcs -ftest-coverage
endif

all: openssl.mod zlib.mod audio.mod

openssl.mod: openssl.c Makefile ../vm/tl.h
	$(CC) $(CFLAGS) -c openssl.c
	$(CC) $(LINK) openssl.o -o openssl.mod -lcrypto -lssl

zlib.mod: zlib.c Makefile ../vm/tl.h
	$(CC) $(CFLAGS) -c zlib.c
	$(CC) $(LINK) zlib.o -o zlib.mod -lz

LJACK:=$(shell if ls /usr/lib*/libjack.* /usr/lib/*/libjack.* 2>/dev/null ; then echo "-ljack"; fi)
LAUDIO:=$(LJACK) -lportaudio
audio.mod: audio.c Makefile ../vm/tl.h
	$(CC) $(CFLAGS) -c audio.c
	$(CC) $(LINK) audio.o -o audio.mod $(LAUDIO)

clean:
	rm -rf *.o *.mod
	rm -rf *.gcda *.gcno

