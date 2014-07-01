_or = left, &right ->
    if not left: return right()
    return left
_and = left, &right ->
    if left: return right()
    return left

_install_global("or", _or)
_install_global("and", _and)

while = &cond ->
    block = args.block
    if not block: throw "while expects a block"
    loop:
        if not cond(): break
        block()

