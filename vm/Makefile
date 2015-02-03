CLANGUNWARN:=$(shell if cc 2>&1 | grep clang >/dev/null; then echo "-Qunused-arguments"; fi)
CFLAGS:=-rdynamic -std=c99 -Wall -Werror -Wno-unused-function -Ivm/ -I. $(CLANGUNWARN) $(CFLAGS)
LDFLAGS:=-lm -lpthread -ldl $(LDFLAGS)

ifeq ($(BUILD),release)
	CFLAGS+=-O3
else
	CFLAGS+=-ggdb -O0
endif
ifeq ($(GCOV),1)
	CFLAGS+=-fprofile-arcs -ftest-coverage
	LDFLAGS+=-fprofile-arcs -ftest-coverage
endif

SOURCES:=$(shell echo *.c | sed -E 's/[a-z]+_test.c//g')
OBJECTS:=$(SOURCES:.c=.o)
TEST_SOURCES:=$(shell echo *_test.c)
TESTS:=$(TEST_SOURCES:.c=)

run: test

test: $(TESTS)
	./env_test
	./args_test
	./number_test
	./hashmap_test
	./idset_test
	./weakmap_test

%_test: %_test.c ../libtl.a Makefile *.h
	$(CC) $< ../libtl.a $(CFLAGS) $(LDFLAGS) -o $@

../libtl.a: $(SOURCES) *.h ../*.h
	cd .. && make libtl.a

clean:
	rm -rf *.o *.mod

