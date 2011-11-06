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
	$(CC) $(subst -Wall,,$(CFLAGS)) -c parser.c

ev.o: ev/*.c ev/*.h config.h Makefile
	$(CC) $(subst -Werror,,$(CFLAGS)) -c ev/ev.c -o ev.o

BOEHM:=$(shell grep "^.define.*HAVE_BOEHMGC" config.h)
ifneq ($(BOEHM),)
LIBGC:=libgc/objs/libgc.a
$(LIBGC): libgc.sh
	./libgc.sh
endif

libtl.a: $(LIBGC) parser.o ev.o vm.o
	rm -f libtl.a
	ar -q libtl.a parser.o ev.o vm.o
ifneq ($(BOEHM),)
	ar -q libtl.a libgc/objs/*.o
endif
	ar -s libtl.a

vm.o: *.c *.h llib/lqueue.* llib/lhashmap.* Makefile
	$(CC) $(CFLAGS) vm.c -c

tl: libtl.a tl.c
	$(CC) $(CFLAGS) tl.c -o tl libtl.a -lm

clean:
	rm -rf tl parser.c *.o *.a *.so *.dylib tl.dSYM
	$(MAKE) -C graphics clean
distclean: clean
	rm -rf libgc greg
dist-clean: distclean

.PHONY: run test clean distclean

