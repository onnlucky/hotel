greeter = (&postpone =>
    print("before")
    postpone()
    print("after")
)
greeter(print("hello"))

