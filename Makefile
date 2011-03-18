CFLAGS=-Wall -Wno-unused-function -g -std=c99

all: tl

lhashmap.o: llib/lhashmap.h llib/lhashmap.c
	$(CC) $(CFLAGS) -c llib/lhashmap.c -o lhashmap.o

lqueue.o: llib/lqueue.h llib/lqueue.c
	$(CC) $(CFLAGS) -c llib/lqueue.c -o lqueue.o

tl: lhashmap.o lqueue.o parser.c *.c *.h
	$(CC) $(CFLAGS) tl.c lhashmap.o lqueue.o -o tl

parser.c: parser.g
	greg -o parser.c parser.g

run: tl run.tl
	valgrind -q --track-origins=yes ./tl

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM

