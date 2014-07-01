env = Env.current
fase1 = io.File("boot/fase1.tl").readString
fase2 = io.File("boot/fase2.tl").readString
fase3 = io.File("boot/fase3.tl").readString

bytes1 = compiler.compile(fase1)
bytes2 = compiler.compile(fase2)
bytes3 = compiler.compile(fase3)

m = compiler.load(bytes1)
m.links.each: link -> assert env.get(link)

m = compiler.load(bytes2)
m.links.each: link -> assert env.get(link) or link == "while"

m = compiler.load(bytes3)
m.links.each: link -> print link, env.get(link)

io.File("boot/fase1.tlb").write(bytes1)
io.File("boot/fase2.tlb").write(bytes2)
io.File("boot/fase3.tlb").write(bytes3)

/*
c = Array.new
c.add "unsigned char boot_tlb[] = {"
bytes.each: b, n ->
    if n % 16 == 1: c.add("\n  ")
    { n == bytes.size  }: c.add "$b\n"
    {                  }: c.add "$b,"
c.add "};\n"
io.File("boot/boot.tlb.h").write(c.join)
*/

