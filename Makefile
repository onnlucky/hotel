CC=gcc
CFLAGS=-std=c99 -Wall -Werror -Wno-unused-function -g

all: tl

run: tl
	valgrind -q --track-origins=yes ./tl run.tl

test: tl
	cd test && ./run.sh

parser.c: parser.g tl.h Makefile
	greg -o parser.c parser.g

parser.o: parser.c
	$(CC) $(subst -Wall,,$(CFLAGS)) -c parser.c -o parser.o

lhashmap.o: llib/lhashmap.h llib/lhashmap.c
	$(CC) $(CFLAGS) -c llib/lhashmap.c -o lhashmap.o

lqueue.o: llib/lqueue.h llib/lqueue.c
	$(CC) $(CFLAGS) -c llib/lqueue.c -o lqueue.o

tl: lqueue.o lhashmap.o parser.o *.c *.h
	$(CC) $(CFLAGS) tl.c lqueue.o lhashmap.o parser.o -o tl

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM

.PHONY: run test clean

