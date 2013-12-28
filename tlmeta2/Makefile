main.c: tlmeta.tl in.tlg
	tl tlmeta.tl in.tlg main.c

tlmeta.tl: tlmeta.tlg
	tlmeta tlmeta.tlg tlmeta.tl

main: main.c tlmeta.c
	gcc -std=c99 -I../vm -I../ -L../ -L/usr/local/lib -Wall -Wno-unused-function main.c -o main -ltl -lm -lpthread -ljack -lportaudio -lssl -lcrypto

xrun: main.c
	tail -n40 main.c

run: main
	./main example.tl

clean:
	rm -rf tlmeta.tl main.c main
