map = HashMap.new

map.set("hello", "world")
out map.get("hello"), "\n"
out map.has("hello"), "\n"
out map.size, "\n"
out map.del("hello"), "\n"
out map.has("hello"), "\n"
out map.size, "\n"

map.set("hello", 42)
map.set("hello world!", "NO")
map.set("HELLO WORLD", 1001)
map.set("hello".cat(" world!"), "yes")

out map.get("hello"), "\n"
out map.get("hello"), "\n"
out map.get("hello world!"), "\n"

