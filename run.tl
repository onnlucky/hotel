list = Array.new
26.times: i -> list.add "$(String('a' - 1 + i))=$i"
26.times: i -> list.add "$(String('A' - 1 + i))=$i"
code = "print repr {"+ list.join(",") +"}"

bytes = compiler.compile(code)
mod = compiler.load(bytes)
mod.run
print bytes.size, bytes

