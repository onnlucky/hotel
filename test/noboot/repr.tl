// only one key, as their order is undefined
out _to_repr({foo=[123,456,7.89, "hello world !! \n\n", "world"]}).readString, "\n"
out _to_repr(_from_repr(_to_repr({foo=[123,456,7.89, "hello world !! \n\n", "world"]}))).readString, "\n"

