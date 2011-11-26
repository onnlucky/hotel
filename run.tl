o1 = _Object_new {
    setFoo: (v-> _this_set(this, #foo, v))
}
print o1.setFoo 42
print o1.foo

