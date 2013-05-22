parser.tl: parser.tlg
	tlmeta parser.tlg parser.tl

test.tl: test.tlg
	tlmeta test.tlg test.tl

run: parser.tl
	tl parser.tl test-input.tl

clean:
	rm -rf parser.tl test.tl

