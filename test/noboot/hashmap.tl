map = HashMap.new

map.set("hello", "world")
print map.get("hello")
print map.has("hello")
print map.size
print map.del("hello")
print map.has("hello")
print map.size

map.set("hello", 42)
map.set("hello world!", "NO")
map.set("HELLO WORLD", 1001)
map.set("hello".cat(" world!"), "yes")

print map.get("hello")
print map.get("hello")
print map.get("hello world!")

