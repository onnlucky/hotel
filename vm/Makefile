CLANGUNWARN:=$(shell if cc 2>&1 | grep clang >/dev/null; then echo "-Qunused-arguments"; fi)
CFLAGS:=-g -O -std=c99 -Wall -Werror -Wno-unused-function $(CLANGUNWARN) $(CFLAGS)

SOURCES:=$(shell echo *.c | sed -E 's/[a-z]+_test.c//g')
OBJECTS:=$(SOURCES:.c=.o)
TEST_SOURCES:=$(shell echo *_test.c)
TESTS:=$(TEST_SOURCES:.c=.o)

run: args_test number_test hashmap_test
	#./args_test
	#./number_test
	./hashmap_test

args_test: args_test.c ../libtl.a Makefile *.h
	$(CC) args_test.c ../libtl.a $(CFLAGS) -lcairo -ljpeg -lssl -lcrypto -lm -lz -o args_test

number_test: number_test.c ../libtl.a Makefile *.h
	$(CC) number_test.c ../libtl.a $(CFLAGS) -lcairo -ljpeg -lssl -lcrypto -lm -lz -o number_test

hashmap_test: hashmap_test.c ../libtl.a Makefile *.h
	$(CC) $< ../libtl.a $(CFLAGS) -lcairo -ljpeg -lssl -lcrypto -lm -lz -o $@

../libtl.a: $(SOURCES) *.h ../*.h
	cd .. && make libtl.a

clean:
	rm -rf *.o *.mod

