CFLAGS=-std=c99 -g -Wall -Werror

run: lhashmap.h lhashmap.c lhashmap-test.c
	$(CC) $(CFLAGS) -c lhashmap.c -o lhashmap.o
	$(CC) $(CFLAGS) lhashmap-test.c -o lhashmap-test
	./lhashmap-test

	$(CC) $(CFLAGS) -c lqueue.c -o lqueue.o
	$(CC) $(CFLAGS) lqueue-test.c lqueue.o -o lqueue-test
	./lqueue-test

clean:
	rm -rf *.o *.lo *.a *.la *.dSYM
	rm -f lhashmap-test lqueue-test
