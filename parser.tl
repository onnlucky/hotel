parserParser = parser {
    name: ls=alpha+   -> ls.toChar
  escape: "\\\""      -> "\""
        | "\\n"       -> "\n"
        | "\\r"       -> "\r"
        | "\\t"       -> "\t"
        | "\\\\"      -> "\\"
        | !"\"" l=_   -> l.toChar
    text: "\"" ts=escape* "\""         -> ts.join

     act: "->". ts=(!"\n" _)*          -> ts.toChar

    term: "(". t=or .")"               -> { type: #Sub, term: t }
        | "."                          -> { type: #Ws }
        | "_"                          -> { type: #Any }
        | n=name !":"                  -> { type: #Call, rule: n }
        | t=text                       -> { type: #Text, text: t }
    post: "!" t=term                   -> { type: #Not, term: t }
        | t=term "*"                   -> { type: #Star, term: t }
        | t=term "+"                   -> { type: #Plus, term: t }
        | t=term "?"                   -> { type: #Opt, term: t }
        | term
   named: n=name "=" t=post            -> { type: #Bind, name: n, term: t }
        | post
     and: h=named rs=(.named)* .a=act? -> { type: #And, terms: [h].cat(rs), act: a }
      or: h=and rs=(."|". and)*        -> { type: #Or, terms: [h].cat(rs) }
    rule: .n=name .":". t=or           -> { type: #Rule, name: n, term: t }
  parser: n=name ."="."parser"."{"
          rs=rule*
          ."}"                         -> { n: n, rs: rs }
   start: b=(!(name ."="."parser"."{") _)*
          p=parser
          a=_*                         -> { before: b.toChar, after: a.toChar, rules: p.rs, name: p.n }
}

// emitting
emit = (r, buf, ind, name ->
    _match(r.type == #Or): goto emitOr r, buf, ind
    _match(r.type == #And): goto emitAnd r, buf, ind
    _match(r.type == #Sub): goto emitSub r, buf, ind
    _match(r.type == #Not): goto emitNot r, buf, ind
    _match(r.type == #Opt): goto emitOpt r, buf, ind
    _match(r.type == #Star): goto emitStar r, buf, ind
    _match(r.type == #Plus): goto emitPlus r, buf, ind

    _match(r.type == #Call): buf.write("rule_$(r.rule)(_in)\n")
    _match(r.type == #Bind): emit(r.term, buf, ind); buf.write("$(ind)  $(r.name) = $name\n")
    _match(r.type == #Text): buf.write("pText(_in, \"$(r.text.escape)\")\n")
    _match(r.type == #Ws):   buf.write("rule_ws(_in)\n")
    _match(r.type == #Any):  buf.write("pAny(_in)\n")
    _match(true): _throw "InternalError emit unknown: $(r.type)"
    _nomatch
)
emitNot = (r, buf, ind ->
    buf.write("$("")pNot(_in, (_in ->\n$(ind)    ")
    emit r.term, buf, ind.cat("  ")
    buf.write("$(ind)  ))\n")
)
emitOpt = (r, buf, ind ->
    buf.write("$("")pOpt(_in, (_in ->\n$(ind)    ")
    emit r.term, buf, ind.cat("  ")
    buf.write("$(ind)  ))\n")
)
emitStar = (r, buf, ind ->
    buf.write("$("")pStar(_in, (_in ->\n$(ind)    ")
    emit r.term, buf, ind.cat("  ")
    buf.write("$(ind)  ))\n")
)
emitPlus = (r, buf, ind ->
    buf.write("$("")pPlus(_in, (_in ->\n$(ind)    ")
    emit r.term, buf, ind.cat("  ")
    buf.write("$(ind)  ))\n")
)
emitSub = (r, buf, ind ->
    buf.write("(_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$(ind)  )(_in)\n")
)
emitOr = (r, buf, ind ->
    if r.terms.size == 0: return
    if r.terms.size == 1: return emit(r.terms.get(0), buf, ind)
    var $n = 0
    r.terms.each: (r ->
        alt = "_a$($n += 1)"
        buf.write("$(ind)  $alt = (_in ->\n")
        emit r, buf, ind.cat("  ")
        buf.write("$(ind)  )\n")
    )

    buf.write("$(ind)  pOr(_in")
    $n = 0
    r.terms.each: (r ->
        alt = "_a$($n += 1)"
        buf.write(", $alt")
    )
    buf.write(")\n")
)
emitAnd = (r, buf, ind ->
    var $n = 0
    r.terms.each: (r ->
        name = "_v$($n += 1)"
        buf.write("$(ind)  _in, $(name) = ")
        emit r, buf, ind, name
    )
    if r.act: (
        name = "_v$($n += 1)"
        buf.write("$(ind)  $name = ($(r.act))\n")
    )
    buf.write("$(ind)  return _in, _v$($n)\n")
)

emitRule = (r, buf ->
    name = r.name
    buf.write("rule_$name = (_in ->\n")
    buf.write("  trace(_in, \"rule_$name\")\n")
    emit r.term, buf, ""
    buf.write(")\n")
)

if not args.get(1): _throw "Usage: <input.tl> <output.tl>"

ast = parserParser(io.File(args.get(0)).read)

buf = io.Buffer.new
buf.write(ast.before)
buf.write("\n// --- parserbase.tl ---\n")
buf.write(io.File("parserbase.tl").read)
buf.write("\n// --- parser ---\n")
var $hasstart = false
ast.rules.each: (r ->
    if r.name == "start": $hasstart = true
    emitRule r, buf
)
start = (if $hasstart: "start") or ast.rules.get(0).name
buf.write("$(ast.name)$("") = text -> pStart(rule_$start, text)\n")
buf.write("\n// ---\n")
buf.write(ast.after)

io.File(args.get(1)).write(buf)
print "written '$(args.get(1))'"

