out("abc".startsWith("ab"), "\n")      # true
out("abc".startsWith("abcd"), "\n")    # false: prefix longer than string
out("abc".startsWith(""), "\n")        # true
out("abc".startsWith("b", 2), "\n")    # true:  "bc" starts with "b"
out("abc".startsWith("bc", 2), "\n")   # true:  "bc" starts with "bc"
out("abc".startsWith("bcd", 2), "\n")  # false: only 2 chars remain, need 3
out("ab".startsWith("bcd", 2), "\n")   # false: only 1 char remains, need 3
