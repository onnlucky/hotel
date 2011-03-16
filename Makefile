CFLAGS=-Wall -Wno-unused-function -Werror -g -std=c99
CC=clang

tl: grammar.c *.c
	$(CC) $(CFLAGS) tl.c -o tl

grammar.c: grammar.g
	greg -o grammar.c grammar.g

run: tl
	valgrind --track-origins=yes ./tl

