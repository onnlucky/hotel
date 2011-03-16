CFLAGS=-Wall -Wno-unused-function -Werror -g -std=c99
CC=clang

tl: parser.c *.c *.h
	$(CC) $(CFLAGS) tl.c -o tl

parser.c: parser.g
	greg -o parser.c parser.g

all: tl

run: tl
	valgrind -q --track-origins=yes ./tl

clean:
	rm -rf tl parser.c *.o *.so tl.dSYM
