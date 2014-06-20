assert openssl.md5("").toHex == "d41d8cd98f00b204e9800998ecf8427e"
assert openssl.md5("The quick brown fox jumps over the lazy dog.").toHex == "e4d909c290d0fb1ca068ffaddf22cbd0"
assert openssl.sha1("").toHex == "da39a3ee5e6b4b0d3255bfef95601890afd80709"
assert openssl.sha1("The quick brown fox jumps over the lazy dog").toHex == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"

