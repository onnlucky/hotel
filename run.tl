o1 = _Object_new {
    foo: 0
    getFoo: -> @foo
    setFoo: v -> @foo = v
}
print o1.foo
print o1.setFoo 42
print o1.foo

