parser.tl: parser.tlg
	tlmeta parser.tlg parser.tl

test.tl: test.tlg
	tlmeta test.tlg test.tl

run: parser.tl
	cd .. && make tl
	tl process.tl test-input.tl
	hexdump -C test-input.tlb
	../tl

clean:
	rm -rf parser.tl test.tl

