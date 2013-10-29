run:
	clang -I../vm -I../ -L../ -L/usr/local/lib -Wall -Werror tokens.c -o tokens \
		-ltl -lm -lpthread -lportaudio -lssl -lcrypto -lgc
	./tokens test.tl
