CFLAGS:=-std=c99 -Wall -O0 -Werror -Wno-unused-function -g $(CFLAGS)

run: mac
	./mac

mac: mac.m graphics.o
	CC mac.m graphics.o ../ev.o ../parser.o -o mac -framework Cocoa -lcairo -lfreetype

graphics.o: graphics.h graphics.c ../*.h ../*.c
	CC $(CFLAGS) -c graphics.c
