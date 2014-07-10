assert _json_parse("42") == 42
assert _json_parse("{hello:42,world:142}").world == 142
assert _json_parse(" {hello:42, 'test % more':\"no\"\n world:142, } ").world == 142
