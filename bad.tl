Test = { ->
    test = -> print foo
}
foo = 42
// TODO here we should close Test, so that it binds the correct foo
Test().test
foo = 100
