jsonparser.c: tlmeta.tl json.tlg
	tl tlmeta.tl json.tlg jsonparser.c

hotelparser.c: tlmeta.tl hotel.tlg
	tl tlmeta.tl hotel.tlg hotelparser.c

tlmeta.tl: tlmeta.tlg
	tlmeta tlmeta.tlg tlmeta.tl

main: tlmeta.c jsonparser.c hotelparser.c main.c
	cc -std=c99 -g -I../vm -I../ -L../ -L/usr/local/lib -Wall -Wno-unused-function main.c -o main -ltl -lm -lpthread

run: main
	./main example.tl

clean:
	rm -rf tlmeta.tl hotelparser.c jsonparser.c main

boot: hotelparser.c jsonparser.c tlmeta.c
	cat warning.c tlmeta.c > ../vm/tlmeta.c
	cat warning.c hotelparser.c > ../vm/hotelparser.c
	#cat warning.c jsonparser.c > ../vm/jsonparser.c
