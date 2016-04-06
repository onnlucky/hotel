CLANGUNWARN:=$(shell if cc 2>&1 | grep clang >/dev/null; then echo "-Qunused-arguments"; fi)
CFLAGS:=-std=c99 -Wall -Werror -Wno-unused-function $(CLANGUNWARN) $(CFLAGS) -I../include/

UNAME:=$(shell uname)
ifneq ($(UNAME),Darwin)
LINK=-shared
CFLAGS:=$(CFLAGS) -fPIC
SDL_CFLAGS:=$(shell pkg-config --cflags sdl2)
SDL_LDFLAGS:=$(shell pkg-config --libs sdl2)
else
LINK=-dynamiclib -flat_namespace -undefined suppress
SDL_CFLAGS:=
SDL_LDFLAGS:=-lsdl2
OPENSSL_CFLAGS:=-I/usr/local/opt/openssl/include
OPENSSL_LDFLAGS:=-L/usr/local/opt/openssl/lib
endif

ifeq ($(BUILD),release)
	CFLAGS+=-O3
else
	CFLAGS+=-ggdb -O0
endif
# doesn't work with coverage tool
ifeq ($(GCOV),1)
	#CFLAGS+=-fprofile-arcs -ftest-coverage
	#LINK+=-fprofile-arcs -ftest-coverage
endif

ifndef NO_OPENSSL
all: openssl.mod
endif
ifndef NO_PORTAUDIO
all: audio.mod
endif
ifndef NO_CAIRO
all: Image.mod
endif
ifndef NO_SDL
all: sdl.mod
endif
all: zlib.mod

openssl.mod: openssl.c Makefile ../include/tl.h
	$(CC) $(CFLAGS) $(OPENSSL_CFLAGS) -c openssl.c
	$(CC) $(LINK) $(OPENSSL_LDFLAGS) openssl.o -o openssl.mod -lcrypto -lssl

zlib.mod: zlib.c Makefile ../include/tl.h
	$(CC) $(CFLAGS) -c zlib.c
	$(CC) $(LINK) zlib.o -o zlib.mod -lz

LJACK:=$(shell if ls /usr/lib*/libjack.* /usr/lib/*/libjack.* 2>/dev/null ; then echo "-ljack"; fi)
LAUDIO:=$(LJACK) -lportaudio
audio.mod: audio.c Makefile ../include/tl.h
	$(CC) $(CFLAGS) -c audio.c
	$(CC) $(LINK) audio.o -o audio.mod $(LAUDIO)

LIMAGE:=-lcairo -lfreetype -lfontconfig -ljpeg
Image.mod: image.c image.h graphics.c graphics.h Makefile ../include/tl.h
	$(CC) $(CFLAGS) -c graphics.c
	$(CC) $(CFLAGS) -c image.c
	$(CC) $(LINK) graphics.o image.o -o Image.mod $(LIMAGE)

sdl.mod: sdl.c Makefile ../include/tl.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c sdl.c
	$(CC) $(LINK) sdl.o -o sdl.mod $(SDL_LDFLAGS)

clean:
	rm -rf *.o *.mod
	rm -rf *.gcda *.gcno

