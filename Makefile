CLANGUNWARN:=$(shell if cc 2>&1 | grep clang >/dev/null; then echo "-Qunused-arguments"; fi)
UNAME := $(shell uname)
CFLAGS:=-rdynamic -std=c99 -Wall -O -Werror -Wno-unused-function -g -Ivm/ -I. $(CLANGUNWARN) $(CFLAGS)

LDFLAGS:=-lm -lpthread
ifneq ($(UNAME),Darwin)
LDFLAGS:=$(LDFLAGS) -ldl
endif

ifeq ($(VALGRIND),1)
TOOL=valgrind --leak-check=full --track-origins=yes --suppressions=libgc.supp
TOOL=valgrind -q --track-origins=yes --suppressions=libgc.supp
endif
ifeq ($(GDB),1)
ifneq ($(shell which gdb),)
TOOL=gdb --args
else
TOOL=lldb --
endif
endif

BOEHM:=$(shell grep "^.define.*HAVE_BOEHMGC" config.h)
LIBGC:=libgc/.libs/objs/libgc.a
LIBMP:=libmp/libtommath.a

TLG_MODULES=modules/sizzle.tl
C_MODULES=cmodules/zlib.mod cmodules/openssl.mod cmodules/audio.mod

all: tl $(TLG_MODULES) $(C_MODULES)

run: tl
	TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl run.tl
	#TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl --noboot run.tl
	#TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl

test-noboot: tl
	cd test/noboot/ && ./run.sh
test: test-noboot $(TLG_MODULES) $(C_MODULES)
	TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl runspec.tl
	cd test/ && TL_MODULE_PATH=../modules:../cmodules $(TOOL) ../tl tester.tl

$(LIBGC):
	./libgc.sh

$(LIBMP):
	cd libmp && make

boot_tl.h: boot/boot.tl
	cd boot && xxd -i boot.tl ../boot_tl.h

greg/greg:
	cd greg && make

parser.c: vm/parser.g greg/greg
	greg/greg -o parser.c vm/parser.g

parser.o: parser.c vm/tl.h config.h $(LIBGC)
	$(CC) $(subst -Wall,,$(CFLAGS)) -c parser.c -o parser.o

hotelparser.o: vm/tl.h config.h vm/hotelparser.c vm/tlmeta.c
	$(CC) $(subst -Wall,,$(CFLAGS)) -c vm/hotelparser.c -o hotelparser.o

jsonparser.o: vm/tl.h config.h vm/jsonparser.c vm/tlmeta.c
	$(CC) $(subst -Wall,,$(CFLAGS)) -c vm/jsonparser.c -o jsonparser.o

xmlparser.o: vm/tl.h config.h vm/xmlparser.c vm/tlmeta.c
	$(CC) $(subst -Wall,,$(CFLAGS)) -c vm/xmlparser.c -o xmlparser.o

ev.o: ev/*.c ev/*.h config.h $(LIBGC)
	$(CC) $(subst -Werror,,$(CFLAGS)) -c ev/ev.c -o ev.o

libtl.a: $(LIBGC) $(LIBMP) parser.o ev.o vm.o hotelparser.o jsonparser.o xmlparser.o
	rm -f libtl.a
	ar -q libtl.a parser.o ev.o vm.o hotelparser.o jsonparser.o xmlparser.o
	ar -q libtl.a libmp/*.o
ifneq ($(BOEHM),)
	ar -q libtl.a libgc/.libs/objs/*.o
endif
	ar -s libtl.a

vm.o: vm/*.c vm/*.h llib/lqueue.* llib/lhashmap.* boot_tl.h $(LIBGC) $(LIBMP)
	$(CC) $(CFLAGS) -Ilibmp -Ilibgc/libatomic_ops/src -c vm/vm.c -o vm.o

tl: libtl.a vm/tl.c
	$(CC) $(CFLAGS) vm/tl.c -o tl libtl.a $(LDFLAGS)

$(C_MODULES): cmodules/*.c
	$(MAKE) -C cmodules

# meta parser and modules depending on it
tlmeta: tlmeta.tlg tl tlmeta-base.tl boot-tlmeta.tl
	TL_MODULE_PATH=./modules ./tl boot-tlmeta.tl tlmeta.tlg tlmeta
	chmod 755 tlmeta

modules/sizzle.tl: modules/sizzle.tlg tl tlmeta
	TL_MODULE_PATH=./modules ./tl tlmeta modules/sizzle.tlg modules/sizzle.tl

clean:
	rm -rf tl parser.c *.o *.a *.so *.dylib tl.dSYM boot_tl.h test/noboot/*.log
	rm -f modules/sizzle.tl
	rm -f tlmeta
	$(MAKE) -C graphics clean
	$(MAKE) -C cmodules clean
distclean: clean
	rm -f $(LIBMP) libmp/*.o
	rm -rf greg/ libgc/ libatomic_ops/
dist-clean: distclean

docgen.tl: docgen.tlg
	TL_MODULE_PATH=./modules ./tl tlmeta docgen.tlg docgen.tl
doc: all docgen.tl
	TL_MODULE_PATH=./modules ./tl docgen.tl vm/text.c #boot/*.tl modules/*.tl

PREFIX?=/usr/local
BINDIR:=$(DESTDIR)$(PREFIX)/bin
LIBDIR:=$(DESTDIR)$(PREFIX)/lib
INCDIR:=$(DESTDIR)$(PREFIX)/include
MODDIR:=$(DESTDIR)$(PREFIX)/lib/tl
install: libtl.a tl tlmeta $(TLG_MODULES) $(C_MODULES)
	mkdir -p $(BINDIR)
	mkdir -p $(LIBDIR)
	mkdir -p $(INCDIR)
	rm -rf $(MODDIR)
	mkdir -p $(MODDIR)
	cp tl $(BINDIR)/
	cp libtl.a $(LIBDIR)/
	cp -r modules/*.tl $(MODDIR)/
	cp -r cmodules/*.mod $(MODDIR)/
	cp tlmeta $(BINDIR)/
	cp tlmeta-base.tl $(MODDIR)/
uninstall:
	rm -rf $(BINDIR)/tl
	rm -rf $(LIBDIR)/libtl.a
	rm -rf $(MODDIR)

.PHONY: run test clean distclean install uninstall

