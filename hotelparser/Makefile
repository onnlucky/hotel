all: main

xmlparser.c: tlmeta.tl abstract.tl xml.tlg
	tl tlmeta.tl xml.tlg xmlparser.c

jsonparser.c: tlmeta.tl abstract.tl json.tlg
	tl tlmeta.tl json.tlg jsonparser.c

hotelparser.c: tlmeta.tl abstract.tl hotel.tlg
	tl tlmeta.tl hotel.tlg hotelparser.c

tlmeta.tl: tlmeta.tlg
	tlmeta tlmeta.tlg tlmeta.tl

main: tlmeta.c xmlparser.c jsonparser.c hotelparser.c main.c
	cc -std=c99 -g -O2 -I../include/ -L../ -L/usr/local/lib -Wall -Wno-unused-function main.c -o main -ltl -lm -lpthread -ldl -lgc

run: main
	./main example.tl

clean:
	rm -rf tlmeta.tl hotelparser.c jsonparser.c xmlparser.c main

boot: hotelparser.c jsonparser.c xmlparser.c tlmeta.c
	cat warning.c tlmeta.c > ../boot/tlmeta.c
	cat warning.c hotelparser.c > ../boot/hotelparser.c
	cat warning.c jsonparser.c > ../boot/jsonparser.c
	cat warning.c xmlparser.c > ../boot/xmlparser.c
