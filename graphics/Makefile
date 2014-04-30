CFLAGS:=-std=c99 -Wall -O0 -Werror -Wno-unused-function -g $(CFLAGS) -I..

run: mac
	TL_MODULE_PATH=../modules ./mac

mac: mac.m graphics.o window.o image.o ../libtl.a
	CC $(CFLAGS) mac.m -o mac graphics.o window.o image.o ../libtl.a -framework Cocoa -lcairo -lfreetype -ljpeg -lssl -lcrypto

graphics.o: graphics.h graphics.c
	CC $(CFLAGS) -c graphics.c

window.o: window.h window.c
	CC $(CFLAGS) -c window.c

image.o: image.h image.c
	CC $(CFLAGS) -c image.c

../libtl.a: ../*.c ../*.h ../vm/*.c ../vm/*.h
	cd ..; $(MAKE) libtl.a

clean:
	rm -rf mac
	rm -rf *.o

.PHONY: run clean
