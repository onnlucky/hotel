code = "{->\nx=42\nf=->print(x)}().f"
code = "print 42; print \"hello world!\""
bytes = compiler.compile(code)
mod = compiler.load(bytes)
mod.run
print bytes.size, bytes

