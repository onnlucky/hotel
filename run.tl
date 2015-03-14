print "hello world!"

###
oop = _Mutable_new({x=42})
testcompiler.load("{-> var this.x = 10; this.y = 10}", _env_current()).disasm
