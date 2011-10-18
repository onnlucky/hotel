CC:=clang
CFLAGS:=-std=c99 -Wall -O -Werror -Wno-unused-function -g $(CFLAGS)
TOOL=valgrind -q --track-origins=yes

all: tl

run: tl
	$(TOOL) ./tl run.tl

test: tl
	cd test && ./run.sh

parser.c: parser.g
	greg -o parser.c parser.g

parser.o: parser.c tl.h config.h Makefile
	$(CC) $(subst -Wall,,$(CFLAGS)) -c parser.c -o parser.o

ev.o: ev/*.c ev/*.h Makefile
	$(CC) $(CFLAGS) -c ev/ev.c -o ev.o

BOEHM:=$(shell grep "^.define.*HAVE_BOEHMGC" config.h)
ifneq ($(BOEHM),)
LIBGC:=libgc.a
$(LIBGC): libgc.sh
	./libgc.sh
endif

tl: $(LIBGC) parser.o ev.o *.c *.h llib/lqueue.* llib/lhashmap.* Makefile
	$(CC) $(CFLAGS) $(LIBGC) tl.c parser.o ev.o -o tl -lm

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM
	rm -rf libgc.a bdwgc

.PHONY: run test clean

