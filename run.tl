if = (c, block){ bool(c, block, {})() }

dir = _Dir_open("test")
dir.each: (p){
    if (p.endsWith(".tl")): print p
}

