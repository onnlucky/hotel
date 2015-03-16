CLANGUNWARN:=$(shell if cc 2>&1 | grep clang >/dev/null; then echo "-Qunused-arguments"; fi)
CFLAGS:=-rdynamic -std=c99 -Wall -Werror -Wno-unused-function -I../libmp/ -I../libatomic_ops/src/ $(CLANGUNWARN) $(CFLAGS)
LDFLAGS:=-lm -lpthread -ldl -lgc $(LDFLAGS)

ifeq ($(BUILD),release)
	CFLAGS+=-ggdb -O3
else
	CFLAGS+=-ggdb -O0
endif
ifeq ($(GCOV),1)
	CFLAGS+=-fprofile-arcs -ftest-coverage
	LDFLAGS+=-fprofile-arcs -ftest-coverage
endif

SOURCES:=$(filter-out tl.c,$(shell echo *.c | sed -E 's/[a-z]+_test.c//g'))
OBJECTS:=$(SOURCES:.c=.o)
TEST_SOURCES:=$(filter-out number_test.c,$(shell echo *_test.c))
TESTS:=$(TEST_SOURCES:.c=)

run: test

test: $(TESTS)
	./env_test
	./args_test
	#./number_test
	./hashmap_test
	./idset_test
	./weakmap_test
	./pmap_test

%.o: %.c *.h Makefile
	$(CC) -c $< $(CFLAGS)

libtl.a: $(OBJECTS) ../ev.o ../boot/*.o
	rm -f libtl.a
	ar -q libtl.a $(OBJECTS) ../ev.o ../boot/*.o
	ar -s libtl.a

tl: tl.c libtl.a *.h Makefile ../libmp/libtommath.a
	$(CC) $< $(CFLAGS) $(LDFLAGS) -o $@ libtl.a ../libmp/libtommath.a

%_test: %_test.c libtl.a *.h Makefile ../libmp/libtommath.a
	$(CC) $< $(CFLAGS) $(LDFLAGS) -o $@ libtl.a ../libmp/libtommath.a

../libtl.a: $(SOURCES) *.h ../*.h
	cd .. && make libtl.a

clean:
	rm -rf *.o
	rm -f *_test

