
// --- tlmeta-base.tl ---
DEBUG=false
trace = (in, &msg -> if DEBUG: print in.pos, in.text.slice(in.pos, in.pos + 10).escape, "--", msg())

WS = " \t\n\r"
LOWER = "abcdefghijklmnopqrstuvwxyz"
UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ALPHA = LOWER.cat(UPPER)
NUMERIC = "0123456789"
ALPHANUMERIC = ALPHA.cat(NUMERIC)

pAny = (in ->
    trace(in, "pAny")
    c = in.text.get(in.pos)
    if not c: _throw { t: #p, in: in }
    return { text: in.text, pos: in.pos + 1 }, c
)
pText = (in, text ->
    trace(in, "pText: $text")
    if in.text.startsWith(text, in.pos): return { text: in.text, pos: in.pos + text.size }, text
    _throw { t: #p, in: in, msg: text }
)
pChar = (in, text ->
    trace(in, "pChar: $text")
    c = in.text.get(in.pos)
    if not c: _throw { t: #p, in: in }
    if text.search(c.toChar): return { text: in.text, pos: in.pos + 1 }, c
    _throw { t: #p, in: in }
)

pNot = (in, parser ->
    trace(in, "pNot")
    in = (
        _catch: (e ->
            if e?t == #p: (
                if e?noreturn: _throw e
                return in, null
            )
            _throw e
        )
        parser(in)
    )
    _throw { t: #p, in: in }
)
pAhead = (in, parser ->
    trace(in, "pAhead")
    in2 = parser(in)
    return in
)
pOpt = (in, parser ->
    trace(in, "pOpt")
    _catch: (e ->
        if e?t == #p: (
            if e?noreturn: _throw e
            return in, null
        )
        _throw e
    )
    parser(in)
)
_pmany = (in, parser, res ->
    trace(in, "_pmany: $(res.size)")
    _catch: (e ->
        if e?t == #p: (
            trace(in, "_pmany: << $(res.size), noreturn: $(e?noreturn)")
            if e?noreturn: _throw e
            return in, res
        )
        _throw e
    )
    in, v = parser(in)
    goto _pmany(in, parser, res.add(v))
)
pStar = (in, parser ->
    trace(in, "pStar")
    _pmany(in, parser, [])
)
pPlus = (in, parser ->
    trace(in, "pPlus")
    in, v = parser(in)
    _pmany(in, parser, [v])
)
pOr = (name, in ->
    var $i = 2
    // TODO instead of just the last one, collect all alternatives
    var $nr = null
    var $err = null
    loop: (
        parser = args.get($i)
        if not parser: break
        trace(in, "pOr: $name: $($i)")
        $nr = null
        $i += 1
        _catch: (e ->
            trace(in, "ERROR: $e NoReturn: $($nr)")
            if e?t != #p: _throw e
            if $nr: (
                if e?noreturn: _throw e
                if $err: _throw $err
                _throw { t: #p, in: e.in, noreturn: true, msg: $nr }
            )
            if e?noreturn: $err = e
        )
        in, v = parser(nr, in)
        trace(in, "pOr: $name <<")
        return in, v
    )
    trace(in, "pOr: $name: no alternatives")
    if $err: _throw $err
    _throw { t: #p, in: in, msg: "a $name" }
)

pHandle = (e, nr ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if nr: _throw { t:#p, in: e.in, noreturn: true, msg: nr }
    _throw e
)

lineFromPos = (text, pos ->
    // TODO bit inefficient ... but works
    lines = text.slice(0, pos).split("\n")
    var $last = lines.size - 1
    loop: (
        if $last < 0: return 1
        line = lines.get($last)
        if line.strip.size == 0: $last -= 1; continue
        return $last + 1, line.size + 1
    )
)

pStart = (rule, text ->
    in, v = (
        _catch: (e ->
            if e?t != #p: _throw e
            line, char = lineFromPos(e.in.text, e.in.pos)
            if e?msg: _throw "SyntaxError: expected $(e.msg) on line $line col $char"
            _throw "SyntaxError: on line $line col $char"
        )
        rule({text: text, pos: 0 })
    )
    if in.pos < in.text.size: (
        line, char = lineFromPos(in.text, in.pos)
        _throw "SyntaxError: expected end of input on line $line col $char"
    )
    return v
)

rule_end = (in ->
    c = in.text.get(in.pos)
    if c: _throw { t: #p, in: in }
    return in, null
)

rule_line = (in -> return in, lineFromPos(in.text, in.pos))

rule_lower = (in -> pChar(in, LOWER))
rule_upper = (in -> pChar(in, UPPER))
rule_alpha = (in -> pChar(in, ALPHA))
rule_numeric = (in -> pChar(in, NUMERIC))
rule_alphanumeric = (in -> pChar(in, ALPHANUMERIC))

rule_ws = (in ->
    trace(in, "rule_ws")
    c = in.text.get(in.pos)
    if not c: return in, null
    if WS.search(c.toChar): goto(rule_ws({text: in.text, pos: in.pos + 1 }))
    return in, null
)


// --- parser ---
rule_sp = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_sp")
  _name = "sp"
  _in, _v = pOr(_name, _in
    ,(_nr, _in ->
      _in, _v = pText(_in, " ")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\t")
      return _in, _v
     )
  )
)
rule_nl = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_nl")
  _name = "nl"
  _in, _v = pOr(_name, _in
    ,(_nr, _in ->
      _in, _v = pText(_in, "\r\n")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\n\r")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\n")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\r")
      return _in, _v
     )
  )
)
rule_slcomment = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_slcomment")
  _name = "slcomment"
  _in, _v = pText(_in, "//")
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = pNot(_in, (_in ->
        _in, _v = rule_nl(_in)
      ))
      _in, _v = pAny(_in)
      return _in, _v
    )(_in)
  ))
  return _in, _v
)
rule_icomment = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_icomment")
  _name = "icomment"
  _in, _v = pText(_in, "/*")
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = pNot(_in, (_in ->
        _in, _v = pText(_in, "*/")
      ))
      _in, _v = pAny(_in)
      return _in, _v
    )(_in)
  ))
  _in, _v = (_in ->
    _in, _v = pOr(_name, _in
      ,(_nr, _in ->
        _in, _v = pText(_in, "*/")
        return _in, _v
       )
      ,(_nr, _in ->
        _in, _v = rule_end(_in)
        return _in, _v
       )
    )
  )(_in)
  return _in, _v
)
rule_comment = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_comment")
  _name = "comment"
  _in, _v = (_in ->
    _in, _v = pOr(_name, _in
      ,(_nr, _in ->
        _in, _v = rule_slcomment(_in)
        _in, _v = (_in ->
          _in, _v = pOr(_name, _in
            ,(_nr, _in ->
              _in, _v = rule_nl(_in)
              return _in, _v
             )
            ,(_nr, _in ->
              _in, _v = rule_end(_in)
              return _in, _v
             )
          )
        )(_in)
        return _in, _v
       )
      ,(_nr, _in ->
        _in, _v = rule_icomment(_in)
        return _in, _v
       )
    )
  )(_in)
  return _in, _v
)
rule_ws = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_ws")
  _name = "ws"
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = pOr(_name, _in
        ,(_nr, _in ->
          _in, _v = rule_sp(_in)
          return _in, _v
         )
        ,(_nr, _in ->
          _in, _v = rule_nl(_in)
          return _in, _v
         )
        ,(_nr, _in ->
          _in, _v = rule_comment(_in)
          return _in, _v
         )
      )
    )(_in)
  ))
  return _in, _v
)
rule_name = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_name")
  _name = "name"
  _in, _v = pPlus(_in, (_in ->
    _in, _v = rule_alpha(_in)
  ))
  ls = _v
  _v = (ls.toChar)
  return _in, _v
)
rule_escape = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_escape")
  _name = "escape"
  _in, _v = pOr(_name, _in
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\\"")
      _v = ("\"")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\n")
      _v = ("\n")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\r")
      _v = ("\r")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\t")
      _v = ("\t")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\\\")
      _v = ("\\")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pNot(_in, (_in ->
        _in, _v = pText(_in, "\"")
      ))
      _in, _v = pAny(_in)
      l = _v
      _v = (l.toChar)
      return _in, _v
     )
  )
)
rule_text = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_text")
  _name = "text"
  _in, _v = pText(_in, "\"")
  _in, _v = pStar(_in, (_in ->
    _in, _v = rule_escape(_in)
  ))
  ts = _v
  $_nr = "a closing '\"'"
  _in, _v = pText(_in, "\"")
  _v = (ts.join)
  return _in, _v
)
rule_cscape = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_cscape")
  _name = "cscape"
  _in, _v = pOr(_name, _in
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\]")
      _v = ("]")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\n")
      _v = ("\n")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\r")
      _v = ("\r")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\t")
      _v = ("\t")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "\\\\")
      _v = ("\\")
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pNot(_in, (_in ->
        _in, _v = pText(_in, "]")
      ))
      _in, _v = pAny(_in)
      l = _v
      _v = (l.toChar)
      return _in, _v
     )
  )
)
rule_char = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_char")
  _name = "char"
  _in, _v = pText(_in, "[")
  _in, _v = pStar(_in, (_in ->
    _in, _v = rule_cscape(_in)
  ))
  ts = _v
  $_nr = "closing ']'"
  _in, _v = pText(_in, "]")
  _v = (ts.join)
  return _in, _v
)
rule_act = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_act")
  _name = "act"
  _in, _v = pText(_in, "->")
  _in, _v = rule_ws(_in)
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = pNot(_in, (_in ->
        _in, _v = pText(_in, "\n")
      ))
      _in, _v = pAny(_in)
      return _in, _v
    )(_in)
  ))
  ts = _v
  _v = (ts.toChar)
  return _in, _v
)
rule_cond = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_cond")
  _name = "cond"
  _in, _v = pText(_in, "?")
  _in, _v = rule_ws(_in)
  _in, _v = pText(_in, "(")
  _in, _v = rule_ws(_in)
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = pNot(_in, (_in ->
        _in, _v = pText(_in, ")")
      ))
      _in, _v = pAny(_in)
      return _in, _v
    )(_in)
  ))
  ts = _v
  _in, _v = pText(_in, ")")
  _v = (ts.toChar)
  return _in, _v
)
rule_term = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_term")
  _name = "term"
  _in, _v = pOr(_name, _in
    ,(_nr, _in ->
      _in, _v = pText(_in, "(")
      _in, _v = rule_ws(_in)
      _in, _v = rule_or(_in)
      t = _v
      $_nr = "a closing ')'"
      _in, _v = rule_ws(_in)
      _in, _v = pText(_in, ")")
      _v = ({ type: #Sub, term: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, ".")
      _v = ({ type: #Ws })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "_")
      _v = ({ type: #Any })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_cond(_in)
      c = _v
      _v = ({ type: #Cond, act: c })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_name(_in)
      n = _v
      _in, _v = pNot(_in, (_in ->
        _in, _v = pText(_in, ":")
      ))
      _v = ({ type: #Call, rule: n })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_text(_in)
      t = _v
      _v = ({ type: #Text, text: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_char(_in)
      t = _v
      _v = ({ type: #Char, text: t })
      return _in, _v
     )
  )
)
rule_post = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_post")
  _name = "post"
  _in, _v = pOr(_name, _in
    ,(_nr, _in ->
      _in, _v = pText(_in, "!")
      $_nr = "a term"
      _in, _v = rule_term(_in)
      t = _v
      _v = ({ type: #Not, term: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "&")
      $_nr = "a term"
      _in, _v = rule_term(_in)
      t = _v
      _v = ({ type: #Ahead, term: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_term(_in)
      t = _v
      _in, _v = pText(_in, "*")
      _v = ({ type: #Star, term: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_term(_in)
      t = _v
      _in, _v = pText(_in, "+")
      _v = ({ type: #Plus, term: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_term(_in)
      t = _v
      _in, _v = pText(_in, "?")
      _v = ({ type: #Opt, term: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_term(_in)
      return _in, _v
     )
  )
)
rule_named = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_named")
  _name = "named"
  _in, _v = pOr(_name, _in
    ,(_nr, _in ->
      _in, _v = rule_name(_in)
      n = _v
      _in, _v = pText(_in, "=")
      $_nr = "a term"
      _in, _v = rule_post(_in)
      t = _v
      _v = ({ type: #Bind, name: n, term: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = pText(_in, "@")
      _in, _v = rule_text(_in)
      t = _v
      _v = ({ type: #NoReturn, msg: t })
      return _in, _v
     )
    ,(_nr, _in ->
      _in, _v = rule_post(_in)
      return _in, _v
     )
  )
)
rule_and = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_and")
  _name = "and"
  _in, _v = rule_named(_in)
  h = _v
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = rule_ws(_in)
      _in, _v = rule_named(_in)
      return _in, _v
    )(_in)
  ))
  rs = _v
  _in, _v = rule_ws(_in)
  _in, _v = pOpt(_in, (_in ->
    _in, _v = rule_act(_in)
  ))
  a = _v
  _v = ({ type: #And, terms: [h].cat(rs), act: a })
  return _in, _v
)
rule_or = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_or")
  _name = "or"
  $_nr = "some terms"
  _in, _v = rule_and(_in)
  h = _v
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = rule_ws(_in)
      _in, _v = pText(_in, "|")
      _in, _v = rule_ws(_in)
      _in, _v = rule_and(_in)
      return _in, _v
    )(_in)
  ))
  rs = _v
  _v = ({ type: #Or, terms: [h].cat(rs) })
  return _in, _v
)
rule_rule = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_rule")
  _name = "rule"
  _in, _v = rule_ws(_in)
  _in, _v = rule_name(_in)
  n = _v
  $_nr = ":"
  _in, _v = rule_ws(_in)
  _in, _v = pText(_in, ":")
  _in, _v = rule_ws(_in)
  _in, _v = rule_or(_in)
  t = _v
  _v = ({ type: #Rule, name: n, term: t })
  return _in, _v
)
rule_parser = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_parser")
  _name = "parser"
  _in, _v = rule_name(_in)
  n = _v
  _in, _v = rule_ws(_in)
  _in, _v = pText(_in, "=")
  _in, _v = rule_ws(_in)
  _in, _v = pText(_in, "parser")
  _in, _v = rule_ws(_in)
  _in, _v = pText(_in, "{")
  _in, _v = pStar(_in, (_in ->
    _in, _v = rule_rule(_in)
  ))
  rs = _v
  $_nr = "a closing '}'"
  _in, _v = rule_ws(_in)
  _in, _v = pText(_in, "}")
  _v = ({ n: n, rs: rs })
  return _in, _v
)
rule_start = (_in ->
  var $_nr = null
  _catch: e -> pHandle(e, $_nr)
  trace(_in, "rule_start")
  _name = "start"
  _in, _v = pStar(_in, (_in ->
    _in, _v = (_in ->
      _in, _v = pNot(_in, (_in ->
        _in, _v = (_in ->
          _in, _v = rule_name(_in)
          _in, _v = rule_ws(_in)
          _in, _v = pText(_in, "=")
          _in, _v = rule_ws(_in)
          _in, _v = pText(_in, "parser")
          _in, _v = rule_ws(_in)
          _in, _v = pText(_in, "{")
          return _in, _v
        )(_in)
      ))
      _in, _v = pAny(_in)
      return _in, _v
    )(_in)
  ))
  b = _v
  _in, _v = rule_parser(_in)
  p = _v
  _in, _v = pStar(_in, (_in ->
    _in, _v = pAny(_in)
  ))
  a = _v
  _v = ({ before: b.toChar, after: a.toChar, rules: p.rs, name: p.n })
  return _in, _v
)
parserParser = text -> pStart(rule_start, text)

// ---


// emitting
emit = (r, buf, ind, name ->
    _match(r.type == #Or): goto emitOr r, buf, ind
    _match(r.type == #And): goto emitAnd r, buf, ind
    _match(r.type == #Sub): goto emitSub r, buf, ind
    _match(r.type == #Not): goto emitNot r, buf, ind
    _match(r.type == #Ahead): goto emitAhead r, buf, ind
    _match(r.type == #Opt): goto emitOpt r, buf, ind
    _match(r.type == #Star): goto emitStar r, buf, ind
    _match(r.type == #Plus): goto emitPlus r, buf, ind

    _match(r.type == #Cond): buf.write("$ind  if not ($(r.act)): _throw { t: #p, in: _in }")
    _match(r.type == #NoReturn): buf.write("$ind  \$_nr = \"$(r.msg.escape)\"\n")

    _match(r.type == #Call): buf.write("$ind  _in, _v = rule_$(r.rule)(_in)\n")
    _match(r.type == #Bind): emit(r.term, buf, ind); buf.write("$(ind)  $(r.name) = _v\n")
    _match(r.type == #Text): buf.write("$ind  _in, _v = pText(_in, \"$(r.text.escape)\")\n")
    _match(r.type == #Char): buf.write("$ind  _in, _v = pChar(_in, \"$(r.text.escape)\")\n")
    _match(r.type == #Ws): buf.write("$ind  _in, _v = rule_ws(_in)\n")
    _match(r.type == #Any): buf.write("$ind  _in, _v = pAny(_in)\n")
    _match(true): _throw "InternalError emit unknown: $(r.type)"
    _nomatch
)
emitNot = (r, buf, ind ->
    buf.write("$ind  _in, _v = pNot(_in, (_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")
)
emitAhead = (r, buf, ind ->
    buf.write("$ind  _in, _v = pAhead(_in, (_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")
)
emitOpt = (r, buf, ind ->
    buf.write("$ind  _in, _v = pOpt(_in, (_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")
)
emitStar = (r, buf, ind ->
    buf.write("$ind  _in, _v = pStar(_in, (_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")
)
emitPlus = (r, buf, ind ->
    buf.write("$ind  _in, _v = pPlus(_in, (_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")
)
emitAnd = (r, buf, ind ->
    var $n = 0
    // TODO maybe collect all binds, and "call" r.act
    r.terms.each: r -> emit r, buf, ind
    if r.act: buf.write("$ind  _v = ($(r.act))\n")
    buf.write("$ind  return _in, _v\n")
)
emitOr = (r, buf, ind ->
    if r.terms.size == 0: return
    if r.terms.size == 1: return emit(r.terms.get(0), buf, ind)

    buf.write("$ind  _in, _v = pOr(_name, _in\n")
    r.terms.each: (r ->
        buf.write("$ind    ,(_nr, _in ->\n")
        emit r, buf, ind.cat("    ")
        buf.write("$ind     )\n")
    )
    buf.write("$ind  )\n")
)
emitSub = (r, buf, ind ->
    buf.write("$ind  _in, _v = (_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  )(_in)\n")
)
emitRule = (r, buf ->
    name = r.name
    buf.write("rule_$name = (_in ->\n")
    buf.write("  var \$_nr = null\n")
    buf.write("  _catch: e -> pHandle(e, \$_nr)\n")
    buf.write("  trace(_in, \"rule_$name\")\n")
    buf.write("  _name = \"$name\"\n")
    emit r.term, buf, ""
    buf.write(")\n")
)

if not args.get(1): _throw "Usage: <input.tl> <output.tl>"

ast = parserParser(io.File(args.get(0)).read)

buf = io.Buffer.new
buf.write(ast.before)
buf.write("\n// --- tlmeta-base.tl ---\n")
buf.write(io.File("tlmeta-base.tl").read)
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

