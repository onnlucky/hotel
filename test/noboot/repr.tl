print _to_repr({hello=42,foo=[123,456,7.89, "hello world !! \n\n", "world"]}).readString
print _to_repr(_from_repr(_to_repr({hello=42,foo=[123,456,7.89, "hello world !! \n\n", "world"]}))).readString

