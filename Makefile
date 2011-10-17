CC:=clang
CFLAGS:=-std=c99 -Wall -O -Werror -Wno-unused-function -g $(CFLAGS)
TOOL=valgrind -q --track-origins=yes

# bit of a hack?
BOEHM:=$(shell grep "^.define.*HAVE_BOEHMGC" config.h)
ifneq ($(BOEHM),)
	LDFLAGS+= -lgc
endif

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

tl: parser.o ev.o *.c *.h llib/lqueue.* llib/lhashmap.* Makefile
	$(CC) $(CFLAGS) tl.c parser.o ev.o -o tl -lm $(LDFLAGS)

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM

.PHONY: run test clean

