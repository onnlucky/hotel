CC=gcc
CFLAGS=-std=c99 -Wall -Wno-unused-function -g

all: tl

run: parser-test tl
	#valgrind -q --track-origins=yes ./parser-test
	valgrind -q --track-origins=yes ./tl

parser.c: parser.g tl.h Makefile
	greg -o parser.c parser.g

parser.o: parser.c
	$(CC) $(CFLAGS) -c parser.c -o parser.o

parser-test: lhashmap.o parser.o *.c *.h
	$(CC) $(CFLAGS) parser-test.c lhashmap.o parser.o -o parser-test

lhashmap.o: llib/lhashmap.h llib/lhashmap.c
	$(CC) $(CFLAGS) -c llib/lhashmap.c -o lhashmap.o

lqueue.o: llib/lqueue.h llib/lqueue.c
	$(CC) $(CFLAGS) -c llib/lqueue.c -o lqueue.o

tl: lhashmap.o parser.o *.c *.h
	$(CC) $(CFLAGS) tl.c lhashmap.o parser.o -o tl

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM

