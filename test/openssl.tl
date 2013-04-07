assert SSL.md5("").toString == "d41d8cd98f00b204e9800998ecf8427e"
assert SSL.md5("The quick brown fox jumps over the lazy dog.").toString == "e4d909c290d0fb1ca068ffaddf22cbd0"
assert SSL.sha1("").toString == "da39a3ee5e6b4b0d3255bfef95601890afd80709"
assert SSL.sha1("The quick brown fox jumps over the lazy dog").toString == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"

