parser.tl: parser.tlg tlmeta.tl tlmeta-base.tl
	tl tlmeta.tl parser.tlg parser.tl

tokenizer.tl: tokenizer.tlg tlmeta.tl tlmeta-base.tl
	tl tlmeta.tl tokenizer.tlg tokenizer.tl

run: tokenizer.tl parser.tl
	tl tokenizer.tl
	#tl parser.tl

clean:
	rm -rf parser.tl tokenizer.tl

