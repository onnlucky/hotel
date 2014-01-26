parser.c: tlmeta.tl in.tlg
	tl tlmeta.tl in.tlg parser.c

tlmeta.tl: tlmeta.tlg
	tlmeta tlmeta.tlg tlmeta.tl

main: tlmeta.c parser.c main.c
	cc -std=c99 -g -I../vm -I../ -L../ -L/usr/local/lib -Wall -Wno-unused-function main.c -o main -ltl -lm -lpthread -lssl -lcrypto

run: parser.c
	#./main example.tl

clean:
	rm -rf tlmeta.tl parser.c main
