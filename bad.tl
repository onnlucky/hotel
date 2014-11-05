_disasm()

Test = { ->
    var $x = 10
    set = n -> $x = n
}
Test().set(40)
