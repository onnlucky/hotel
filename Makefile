CFLAGS:=-std=c99 -Wall -O0 -Werror -Wno-unused-function -g $(CFLAGS)
ifeq ($(VALGRIND),1)
TOOL=valgrind -q --track-origins=yes
endif

all: tl

run: tl
	$(TOOL) ./tl run.tl

test: tl
	cd test && ./run.sh

greg/greg:
	git clone http://github.com/onnlucky/greg
	cd greg && git checkout 8bc002c0f640c2d93bb9d9dc965d61df8caf4cf4 && make

parser.c: parser.g greg/greg
	greg/greg -o parser.c parser.g

parser.o: parser.c tl.h config.h Makefile
	$(CC) $(subst -Wall,,$(CFLAGS)) -c parser.c -o parser.o

ev.o: ev/*.c ev/*.h config.h Makefile
	$(CC) $(subst -Werror,,$(CFLAGS)) -c ev/ev.c -o ev.o

BOEHM:=$(shell grep "^.define.*HAVE_BOEHMGC" config.h)
ifneq ($(BOEHM),)
LIBGC:=libgc.a
$(LIBGC): libgc.sh
	./libgc.sh
endif

tl: $(LIBGC) parser.o ev.o *.c *.h llib/lqueue.* llib/lhashmap.* Makefile
	$(CC) $(CFLAGS) tl.c parser.o ev.o -o tl -lm $(LIBGC)

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM
distclean: clean
	rm -rf libgc.a libgc greg
dist-clean: distclean

.PHONY: run test clean distclean

