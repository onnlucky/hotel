fn:   { names, defaults, map }
call: { values, names, map }

args = Args.new
var first = -1
call.values.each (value, at)->(
    name: call.names[at]
    if name ->
        default: fn.defaults[fn.map[name]]
        if default.lazy? -> args[name] = Thunk(value)
        else -> args[name] = eval(value)
        continue

    repeat ->
        first++
        name: fn.names[first]
        if call.map[name] -> again
        default: fn.defaults[first]
        if default.lazy? -> args[at] = Thunk(value)
        else -> args[at] = eval(value)
)

while first < fn.names.length ->(
    first++
    name: fn.names[first]
    if call.map[name] -> again
    default: fn.defaults[first]
    args[first] = default
)

args.each (value, at)->(
    env[fn.names[at]] = value
)

