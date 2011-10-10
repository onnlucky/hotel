CC:=clang
CFLAGS:=-std=c99 -Wall -O -Werror -Wno-unused-function -g $(CFLAGS)
TOOL=valgrind -q --track-origins=yes
TOOL=

all: tl

run: tl
	$(TOOL) ./tl run.tl

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

ev.o: ev/ev.c
	$(CC) $(CFLAGS) -c ev/ev.c -o ev.o

tl: lqueue.o lhashmap.o parser.o ev.o *.c *.h
	$(CC) $(CFLAGS) tl.c lqueue.o lhashmap.o parser.o ev.o -o tl -lm

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM

.PHONY: run test clean

