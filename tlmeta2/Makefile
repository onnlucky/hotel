main.c: tlmeta.tl in.tlg
	tl tlmeta.tl in.tlg main.c

tlmeta.tl: tlmeta.tlg
	tlmeta tlmeta.tlg tlmeta.tl

main: main.c tlmeta.c
	clang -I../vm -I../ -L../ -L/usr/local/lib -Wall main.c -o main -ltl -lm -lpthread -lportaudio -lssl -lcrypto -lgc

xrun: main.c
	tail -n40 main.c

run: main
	./main "1 + 2"

clean:
	rm -rf tlmeta.tl main.c main
