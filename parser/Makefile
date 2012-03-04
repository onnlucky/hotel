parser.tl: parser.tlg tlmeta.tl tlmeta-base.tl
	tl tlmeta.tl parser.tlg parser.tl

test.tl: test.tlg tlmeta.tl tlmeta-base.tl
	tl tlmeta.tl test.tlg test.tl

run: parser.tl test.tl
	tl parser.tl

clean:
	rm -rf parser.tl test.tl

