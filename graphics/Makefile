CFLAGS:=-std=c99 -Wall -O0 -Werror -Wno-unused-function -g $(CFLAGS)

run: mac
	./mac

mac: mac.m graphics.o ../libtl.a
	CC $(CFLAGS) mac.m -o mac graphics.o ../libtl.a -framework Cocoa -lcairo -lfreetype

graphics.o: graphics.h graphics.c
	CC $(CFLAGS) -c graphics.c

../libtl.a: ../*.c ../*.h
	cd ..; $(MAKE) libtl.a

clean:
	rm -rf mac
	rm -rf *.o

.PHONY: run clean
