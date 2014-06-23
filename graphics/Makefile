UNAME := $(shell uname)

ifneq ($(UNAME),Darwin)
GTKFLAGS:=-ldl $(shell pkg-config --cflags --libs gtk+-3.0)
endif

CFLAGS:=-std=c99 -Wall -O0 -Werror -Wno-unused-function -g $(CFLAGS) -I.. $(GTKFLAGS)

run: gtl
	TL_MODULE_PATH=../modules ./gtl run.tl

ifeq ($(UNAME),Darwin)
gtl: mac.m graphics.o window.o app.o image.o ../libtl.a
	$(CC) $(CFLAGS) mac.m -o gtl graphics.o window.o image.o app.o ../libtl.a -framework Cocoa -lcairo -lfreetype -ljpeg -lssl -lcrypto -lz
else
gtl: gtk.c graphics.o window.o app.o image.o ../libtl.a
	$(CC) gtk.c ../libtl.a graphics.o image.o window.o app.o $(CFLAGS) -lcairo -ljpeg -lssl -lcrypto -lm -lz -o gtl
endif

graphics.o: graphics.h graphics.c
	$(CC) $(CFLAGS) -c graphics.c

window.o: window.h window.c
	$(CC) $(CFLAGS) -c window.c

image.o: image.h image.c
	$(CC) $(CFLAGS) -c image.c

app.o: app.h app.c
	$(CC) $(CFLAGS) -c app.c

../libtl.a: ../*.c ../*.h ../vm/*.c ../vm/*.h
	cd ..; $(MAKE) libtl.a

PREFIX?=/usr/local
BINDIR:=$(DESTDIR)$(PREFIX)/bin
install: gtl
	cp gtl $(BINDIR)/gtl

clean:
	rm -rf gtl
	rm -rf *.o

.PHONY: run clean
