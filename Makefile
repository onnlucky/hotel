CFLAGS:=-std=c99 -Wall -O -Werror -Wno-unused-function -g -Ivm/ -I. $(CFLAGS)
ifeq ($(VALGRIND),1)
TOOL=valgrind -q --track-origins=yes
endif
ifeq ($(GDB),1)
TOOL=gdb --args
endif

BOEHM:=$(shell grep "^.define.*HAVE_BOEHMGC" config.h)
LIBGC:=libgc/objs/libgc.a

TLG_MODULES=modules/html.tl modules/sizzle.tl

all: tl $(TLG_MODULES)

run: tl
	TL_MODULE_PATH=./modules $(TOOL) ./tl run.tl

runnoboot: tl
	TL_MODULE_PATH=./modules $(TOOL) ./tl --noboot run.tl

test: tl
	cd test/noboot/ && ./run.sh
	cd test/ && TL_MODULE_PATH=../modules $(TOOL) ../tl tester.tl

$(LIBGC):
	./libgc.sh

boot_tl.h: boot/boot.tl
	cd boot && xxd -i boot.tl ../boot_tl.h

greg/greg:
	git clone git://github.com/onnlucky/greg.git
	cd greg && git checkout 8bc002c0f640c2d93bb9d9dc965d61df8caf4cf4 && make

parser.c: vm/parser.g greg/greg
	greg/greg -o parser.c vm/parser.g

parser.o: parser.c vm/tl.h config.h $(LIBGC)
	$(CC) $(subst -Wall,,$(CFLAGS)) -c parser.c -o parser.o

ev.o: ev/*.c ev/*.h config.h $(LIBGC)
	$(CC) $(subst -Werror,,$(CFLAGS)) -c ev/ev.c -o ev.o

libtl.a: $(LIBGC) parser.o ev.o vm.o
	rm -f libtl.a
	ar -q libtl.a parser.o ev.o vm.o
ifneq ($(BOEHM),)
	ar -q libtl.a libgc/objs/*.o
endif
	ar -s libtl.a

vm.o: vm/*.c vm/*.h llib/lqueue.* llib/lhashmap.* boot_tl.h $(LIBGC)
	$(CC) $(CFLAGS) -Ilibgc/libatomic_ops/src -c vm/vm.c -o vm.o

tl: libtl.a vm/tl.c
	$(CC) $(CFLAGS) vm/tl.c -o tl libtl.a -lm -lpthread -lportaudio -lssl -lcrypto

# meta parser and modules depending on it
tlmeta.tl: tlmeta.tlg tl tlmeta-base.tl
	TL_MODULE_PATH=./modules ./tl boot-tlmeta.tl tlmeta.tlg tlmeta.tl

modules/html.tl: modules/html.tlg tl tlmeta.tl
	TL_MODULE_PATH=./modules ./tl tlmeta.tl modules/html.tlg modules/html.tl

modules/sizzle.tl: modules/sizzle.tlg tl tlmeta.tl
	TL_MODULE_PATH=./modules ./tl tlmeta.tl modules/sizzle.tlg modules/sizzle.tl

clean:
	rm -rf tl parser.c *.o *.a *.so *.dylib tl.dSYM boot_tl.h test/noboot/*.log
	$(MAKE) -C graphics clean
distclean: clean
	rm -rf greg/ libgc/
dist-clean: distclean

PREFIX?=/usr/local
BINDIR:=$(DESTDIR)$(PREFIX)/bin
LIBDIR:=$(DESTDIR)$(PREFIX)/lib
INCDIR:=$(DESTDIR)$(PREFIX)/include
MODDIR:=$(DESTDIR)$(PREFIX)/lib/tl
install: libtl.a tl $(TLG_MODULES)
	mkdir -p $(BINDIR)
	mkdir -p $(LIBDIR)
	mkdir -p $(INCDIR)
	rm -rf $(MODDIR)
	mkdir -p $(MODDIR)
	cp tl $(BINDIR)/
	cp libtl.a $(LIBDIR)/
	cp -r modules/*.tl $(MODDIR)/
uninstall:
	rm -rf $(BINDIR)/tl
	rm -rf $(LIBDIR)/libtl.a
	rm -rf $(MODDIR)

.PHONY: run test clean distclean install uninstall

