CLANGUNWARN:=$(shell if cc 2>&1 | grep clang >/dev/null; then echo "-Qunused-arguments"; fi)
CFLAGS:=-rdynamic -std=c99 -Wall -Werror -Wno-unused-function -Ilibmp/ -Ilibatomic_ops/src/ $(CLANGUNWARN) $(CFLAGS)
LDFLAGS:=-lm -lpthread -ldl -lgc $(LDFLAGS)

ifeq ($(BUILD),release)
	CFLAGS+=-O3
else
	CFLAGS+=-ggdb -O0
endif
ifeq ($(GCOV),1)
	CFLAGS+=-fprofile-arcs -ftest-coverage
	LDFLAGS+=-fprofile-arcs -ftest-coverage
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
ifeq ($(DDD),1)
	TOOL=ddd --args
endif

LIBMP:=libmp/libtommath.a
LIBATOMIC:=libatomic_ops/src/atomic_ops.h

TLG_MODULES=modules/sizzle.tl
MODULES:=$(shell echo modules/*.tl) $(TLG_MODULES)
BIN_MODULES:=$(MODULES:.tl=.tlb)
C_MODULES=cmodules/zlib.mod cmodules/openssl.mod cmodules/audio.mod

all: tl $(C_MODULES) $(TLG_MODULES) $(BIN_MODULES)

# these targets require a working language
boot: boot/init.tlb.h boot/compiler.tlb.h boot/hotelparser.o boot/jsonparser.o boot/xmlparser.o

# so we check it and either copy pre generated (perhaps older) versions, or build them
BOOT?=$(shell tl --version | grep hotel)
ifneq ($(BOOT),)
boot/init.tlb boot/init.tlb.h: modules/init.tl tlcompiler
	./tlcompiler modules/init.tl -c
	mv modules/init.tlb boot/init.tlb
	mv modules/init.tlb.h boot/init.tlb.h
boot/compiler.tlb boot/compiler.tlb.h: modules/compiler.tl tlcompiler
	./tlcompiler modules/compiler.tl -c
	mv modules/compiler.tlb boot/compiler.tlb
	mv modules/compiler.tlb.h boot/compiler.tlb.h
boot/tlmeta.c boot/hotelparser.c boot/jsonparser.c boot/xmlparser.c:
	$(MAKE) -C hotelparser boot
else
boot/init.tlb.h boot/compiler.tlb.h boot/tlmeta.c boot/hotelparser.c boot/jsonparser.c boot/xmlparser.c: boot/pregen/*
	cp boot/pregen/* boot/
endif

boot/hotelparser.o: Makefile vm/tl.h config.h boot/hotelparser.c boot/tlmeta.c
	$(CC) $(subst -Wall,,$(CFLAGS)) -c boot/hotelparser.c -o boot/hotelparser.o
boot/jsonparser.o: Makefile vm/tl.h config.h boot/jsonparser.c boot/tlmeta.c
	$(CC) $(subst -Wall,,$(CFLAGS)) -c boot/jsonparser.c -o boot/jsonparser.o
boot/xmlparser.o: Makefile vm/tl.h config.h boot/xmlparser.c boot/tlmeta.c
	$(CC) $(subst -Wall,,$(CFLAGS)) -c boot/xmlparser.c -o boot/xmlparser.o

run: tl
	echo $(BOOT)
	TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl run.tl
	#./tlcompiler run.tl
	#TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl --init run.tlb
	#TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl

unit-test: ev.o boot
	$(MAKE) -C vm test
test-noboot: tl
	cd test/noboot/ && ./run.sh
test: unit-test test-noboot $(TLG_MODULES) $(C_MODULES)
	TL_MODULE_PATH=./modules:./cmodules $(TOOL) ./tl runspec.tl
	cd test/ && TL_MODULE_PATH=../modules:../cmodules $(TOOL) ../tl tester.tl

$(LIBMP):
	cd libmp && make

$(LIBATOMIC):
	git submodule update --init

ev.o: ev/*.c ev/*.h config.h Makefile
	echo $(CC) $(subst -Wall,,$(CFLAGS)) -Wno-comment -c ev/ev.c -o ev.o
	$(CC) $(CFLAGS) -Wno-extern-initializer -Wno-bitwise-op-parentheses -Wno-unused -Wno-comment -c ev/ev.c -o ev.o

tl: boot ev.o $(LIBMP) $(LIBATOMIC) llib/*.h llib/*.c vm/*.c vm/*.h Makefile
	$(MAKE) -C vm tl
	cp vm/tl tl

$(C_MODULES): cmodules/*.c
	$(MAKE) -C cmodules

# meta parser and modules depending on it
tlmeta: tlmeta.tlg tl tlmeta-base.tl boot-tlmeta.tl
	TL_MODULE_PATH=./modules ./tl boot-tlmeta.tl tlmeta.tlg tlmeta
	chmod 755 tlmeta

modules/sizzle.tl: modules/sizzle.tlg tl tlmeta
	TL_MODULE_PATH=./modules ./tl tlmeta modules/sizzle.tlg modules/sizzle.tl

modules/sizzle.tlb: modules/sizzle.tl
modules/%.tlb: modules/%.tl
	TL_MODULE_PATH=modules:cmodules ./tl ./tlcompiler $^ $@

clean:
	rm -rf tl *.o *.a *.so *.dylib tl.dSYM test/noboot/*.log
	rm -f modules/sizzle.tl
	rm -f modules/*.tlb
	rm -f tlmeta
	rm -rf gcov*html *.gcda *.gcno vm/*.gcda vm/*.gcno boot/*.gcda boot/*.gcno
	$(MAKE) -C vm clean
	$(MAKE) -C cmodules clean
ifndef NO_GRAPHICS
	$(MAKE) -C graphics clean
endif
	rm -rf boot/*.tlb boot/*.tl boot/*.o
distclean: clean
	rm -f $(LIBMP) libmp/*.o
	rm -rf libatomic_ops/
dist-clean: distclean

docgen.tl: docgen.tlg
	TL_MODULE_PATH=./modules ./tl tlmeta docgen.tlg docgen.tl
doc: all docgen.tl
	TL_MODULE_PATH=./modules ./tl docgen.tl boot/boot.tl vm/array.c vm/bin.c vm/buffer.c vm/list.c vm/map.c vm/regex.c vm/string.c vm/time.c vm/vm.c modules/io.tl

PREFIX?=/usr/local
BINDIR:=$(DESTDIR)$(PREFIX)/bin
LIBDIR:=$(DESTDIR)$(PREFIX)/lib
INCDIR:=$(DESTDIR)$(PREFIX)/include
MODDIR:=$(DESTDIR)$(PREFIX)/lib/tl
install: libtl.a tl tlmeta $(TLG_MODULES) $(C_MODULES) $(BIN_MODULES)
	mkdir -p $(BINDIR)
	mkdir -p $(LIBDIR)
	mkdir -p $(INCDIR)
	rm -rf $(MODDIR)
	mkdir -p $(MODDIR)
	cp tl $(BINDIR)/
	cp libtl.a $(LIBDIR)/
	cp vm/tl.h $(INCDIR)/tl.h
	cp modules/*.tl $(MODDIR)/
	cp modules/*.tlb $(MODDIR)/
	cp cmodules/*.mod $(MODDIR)/
	cp tlmeta $(BINDIR)/
	cp tlmeta-base.tl $(MODDIR)/
ifndef NO_GRAPHICS
	$(MAKE) -C graphics install
endif
uninstall:
	rm -rf $(BINDIR)/tl
	rm -rf $(BINDIR)/tlmeta
	rm -rf $(LIBDIR)/libtl.a
	rm -rf $(INCDIR)/tl.h
	rm -rf $(MODDIR)
ifndef NO_GRAPHICS
	$(MAKE) -C graphics uninstall
endif

.PHONY: run test clean distclean install uninstall

