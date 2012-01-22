
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
    _throw { t: #p, in: in }
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
pOr = (name, nr, in ->
    var $i = 3
    // TODO instead of just the last one, collect all alternatives
    var $err = null
    loop: (
        parser = args.get($i)
        if not parser: break
        trace(in, "pOr: $name: $($i)")
        $i += 1
        _catch: (e ->
            trace(in, "ERROR: $e NoReturn: $($nr)")
            if e?t != #p: _throw e
            if $nr: (
                if e?noreturn: _throw e
                _throw { t: #p, in: e.in, noreturn: true, msg: $nr }
            )
            if e?noreturn: $err = e
        )
        // TODO return parser(in) should work somehow ...
        in, v = parser(in)
        trace(in, "pOr: $name <<")
        return in, v
    )
    trace(in, "pOr: $name: no alternatives")
    if $err: _throw $err
    _throw { t: #p, in: in, msg: "a $name" }
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
  trace(_in, "rule_sp")
  _name = "sp"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  pOr(_name, _nr, _in
    ,(_in ->
      _in, _v1 = pText(_in, " ")
      return _in, _v1
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\t")
      return _in, _v1
     )
  )
)
rule_nl = (_in ->
  trace(_in, "rule_nl")
  _name = "nl"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  pOr(_name, _nr, _in
    ,(_in ->
      _in, _v1 = pText(_in, "\r\n")
      return _in, _v1
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\n\r")
      return _in, _v1
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\n")
      return _in, _v1
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\r")
      return _in, _v1
     )
  )
)
rule_slcomment = (_in ->
  trace(_in, "rule_slcomment")
  _name = "slcomment"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pText(_in, "//")
  _in, _v2 = pStar(_in, (_in ->
    (_in ->
      _in, _v1 = pNot(_in, (_in ->
        rule_nl(_in)
      ))
      _in, _v2 = pAny(_in)
      return _in, _v2
    )(_in)
  ))
  return _in, _v2
)
rule_icomment = (_in ->
  trace(_in, "rule_icomment")
  _name = "icomment"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pText(_in, "/*")
  _in, _v2 = pStar(_in, (_in ->
    (_in ->
      _in, _v1 = pNot(_in, (_in ->
        pText(_in, "*/")
      ))
      _in, _v2 = pAny(_in)
      return _in, _v2
    )(_in)
  ))
  _in, _v3 = (_in ->
    pOr(_name, _nr, _in
      ,(_in ->
        _in, _v1 = pText(_in, "*/")
        return _in, _v1
       )
      ,(_in ->
        _in, _v1 = rule_end(_in)
        return _in, _v1
       )
    )
  )(_in)
  return _in, _v3
)
rule_comment = (_in ->
  trace(_in, "rule_comment")
  _name = "comment"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = (_in ->
    pOr(_name, _nr, _in
      ,(_in ->
        _in, _v1 = rule_slcomment(_in)
        _in, _v2 = (_in ->
          pOr(_name, _nr, _in
            ,(_in ->
              _in, _v1 = rule_nl(_in)
              return _in, _v1
             )
            ,(_in ->
              _in, _v1 = rule_end(_in)
              return _in, _v1
             )
          )
        )(_in)
        return _in, _v2
       )
      ,(_in ->
        _in, _v1 = rule_icomment(_in)
        return _in, _v1
       )
    )
  )(_in)
  return _in, _v1
)
rule_ws = (_in ->
  trace(_in, "rule_ws")
  _name = "ws"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pStar(_in, (_in ->
    (_in ->
      pOr(_name, _nr, _in
        ,(_in ->
          _in, _v1 = rule_sp(_in)
          return _in, _v1
         )
        ,(_in ->
          _in, _v1 = rule_nl(_in)
          return _in, _v1
         )
        ,(_in ->
          _in, _v1 = rule_comment(_in)
          return _in, _v1
         )
      )
    )(_in)
  ))
  return _in, _v1
)
rule_name = (_in ->
  trace(_in, "rule_name")
  _name = "name"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pPlus(_in, (_in ->
    rule_alpha(_in)
  ))
  ls = _v1
  _v2 = (ls.toChar)
  return _in, _v2
)
rule_escape = (_in ->
  trace(_in, "rule_escape")
  _name = "escape"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  pOr(_name, _nr, _in
    ,(_in ->
      _in, _v1 = pText(_in, "\\\"")
      _v2 = ("\"")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\n")
      _v2 = ("\n")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\r")
      _v2 = ("\r")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\t")
      _v2 = ("\t")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\\\")
      _v2 = ("\\")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pNot(_in, (_in ->
        pText(_in, "\"")
      ))
      _in, _v2 = pAny(_in)
      l = _v2
      _v3 = (l.toChar)
      return _in, _v3
     )
  )
)
rule_text = (_in ->
  trace(_in, "rule_text")
  _name = "text"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pText(_in, "\"")
  _in, _v2 = pStar(_in, (_in ->
    rule_escape(_in)
  ))
  ts = _v2
  _in, _v3 = _in
  $_nr = "a closing '\"'"
  _in, _v4 = pText(_in, "\"")
  _v5 = (ts.join)
  return _in, _v5
)
rule_cscape = (_in ->
  trace(_in, "rule_cscape")
  _name = "cscape"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  pOr(_name, _nr, _in
    ,(_in ->
      _in, _v1 = pText(_in, "\\]")
      _v2 = ("]")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\n")
      _v2 = ("\n")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\r")
      _v2 = ("\r")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\t")
      _v2 = ("\t")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "\\\\")
      _v2 = ("\\")
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pNot(_in, (_in ->
        pText(_in, "]")
      ))
      _in, _v2 = pAny(_in)
      l = _v2
      _v3 = (l.toChar)
      return _in, _v3
     )
  )
)
rule_char = (_in ->
  trace(_in, "rule_char")
  _name = "char"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pText(_in, "[")
  _in, _v2 = pStar(_in, (_in ->
    rule_cscape(_in)
  ))
  ts = _v2
  _in, _v3 = _in
  $_nr = "closing ']'"
  _in, _v4 = pText(_in, "]")
  _v5 = (ts.join)
  return _in, _v5
)
rule_act = (_in ->
  trace(_in, "rule_act")
  _name = "act"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pText(_in, "->")
  _in, _v2 = rule_ws(_in)
  _in, _v3 = pStar(_in, (_in ->
    (_in ->
      _in, _v1 = pNot(_in, (_in ->
        pText(_in, "\n")
      ))
      _in, _v2 = pAny(_in)
      return _in, _v2
    )(_in)
  ))
  ts = _v3
  _v4 = (ts.toChar)
  return _in, _v4
)
rule_cond = (_in ->
  trace(_in, "rule_cond")
  _name = "cond"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pText(_in, "?")
  _in, _v2 = rule_ws(_in)
  _in, _v3 = pText(_in, "(")
  _in, _v4 = rule_ws(_in)
  _in, _v5 = pStar(_in, (_in ->
    (_in ->
      _in, _v1 = pNot(_in, (_in ->
        pText(_in, ")")
      ))
      _in, _v2 = pAny(_in)
      return _in, _v2
    )(_in)
  ))
  ts = _v5
  _in, _v6 = pText(_in, ")")
  _v7 = (ts.toChar)
  return _in, _v7
)
rule_term = (_in ->
  trace(_in, "rule_term")
  _name = "term"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  pOr(_name, _nr, _in
    ,(_in ->
      _in, _v1 = pText(_in, "(")
      _in, _v2 = rule_ws(_in)
      _in, _v3 = rule_or(_in)
      t = _v3
      _in, _v4 = _in
      $_nr = "a closing ')'"
      _in, _v5 = rule_ws(_in)
      _in, _v6 = pText(_in, ")")
      _v7 = ({ type: #Sub, term: t })
      return _in, _v7
     )
    ,(_in ->
      _in, _v1 = pText(_in, ".")
      _v2 = ({ type: #Ws })
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = pText(_in, "_")
      _v2 = ({ type: #Any })
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = rule_cond(_in)
      c = _v1
      _v2 = ({ type: #Cond, act: c })
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = rule_name(_in)
      n = _v1
      _in, _v2 = pNot(_in, (_in ->
        pText(_in, ":")
      ))
      _v3 = ({ type: #Call, rule: n })
      return _in, _v3
     )
    ,(_in ->
      _in, _v1 = rule_text(_in)
      t = _v1
      _v2 = ({ type: #Text, text: t })
      return _in, _v2
     )
    ,(_in ->
      _in, _v1 = rule_char(_in)
      t = _v1
      _v2 = ({ type: #Char, text: t })
      return _in, _v2
     )
  )
)
rule_post = (_in ->
  trace(_in, "rule_post")
  _name = "post"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  pOr(_name, _nr, _in
    ,(_in ->
      _in, _v1 = pText(_in, "!")
      _in, _v2 = _in
      $_nr = "a term"
      _in, _v3 = rule_term(_in)
      t = _v3
      _v4 = ({ type: #Not, term: t })
      return _in, _v4
     )
    ,(_in ->
      _in, _v1 = rule_term(_in)
      t = _v1
      _in, _v2 = pText(_in, "*")
      _v3 = ({ type: #Star, term: t })
      return _in, _v3
     )
    ,(_in ->
      _in, _v1 = rule_term(_in)
      t = _v1
      _in, _v2 = pText(_in, "+")
      _v3 = ({ type: #Plus, term: t })
      return _in, _v3
     )
    ,(_in ->
      _in, _v1 = rule_term(_in)
      t = _v1
      _in, _v2 = pText(_in, "?")
      _v3 = ({ type: #Opt, term: t })
      return _in, _v3
     )
    ,(_in ->
      _in, _v1 = rule_term(_in)
      return _in, _v1
     )
  )
)
rule_named = (_in ->
  trace(_in, "rule_named")
  _name = "named"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  pOr(_name, _nr, _in
    ,(_in ->
      _in, _v1 = rule_name(_in)
      n = _v1
      _in, _v2 = pText(_in, "=")
      _in, _v3 = _in
      $_nr = "a term"
      _in, _v4 = rule_post(_in)
      t = _v4
      _v5 = ({ type: #Bind, name: n, term: t })
      return _in, _v5
     )
    ,(_in ->
      _in, _v1 = pText(_in, "@")
      _in, _v2 = rule_text(_in)
      t = _v2
      _v3 = ({ type: #NoReturn, msg: t })
      return _in, _v3
     )
    ,(_in ->
      _in, _v1 = rule_post(_in)
      return _in, _v1
     )
  )
)
rule_and = (_in ->
  trace(_in, "rule_and")
  _name = "and"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = rule_named(_in)
  h = _v1
  _in, _v2 = pStar(_in, (_in ->
    (_in ->
      _in, _v1 = rule_ws(_in)
      _in, _v2 = rule_named(_in)
      return _in, _v2
    )(_in)
  ))
  rs = _v2
  _in, _v3 = rule_ws(_in)
  _in, _v4 = pOpt(_in, (_in ->
    rule_act(_in)
  ))
  a = _v4
  _v5 = ({ type: #And, terms: [h].cat(rs), act: a })
  return _in, _v5
)
rule_or = (_in ->
  trace(_in, "rule_or")
  _name = "or"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = _in
  $_nr = "some terms"
  _in, _v2 = rule_and(_in)
  h = _v2
  _in, _v3 = pStar(_in, (_in ->
    (_in ->
      _in, _v1 = rule_ws(_in)
      _in, _v2 = pText(_in, "|")
      _in, _v3 = rule_ws(_in)
      _in, _v4 = rule_and(_in)
      return _in, _v4
    )(_in)
  ))
  rs = _v3
  _v4 = ({ type: #Or, terms: [h].cat(rs) })
  return _in, _v4
)
rule_rule = (_in ->
  trace(_in, "rule_rule")
  _name = "rule"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = rule_ws(_in)
  _in, _v2 = rule_name(_in)
  n = _v2
  _in, _v3 = _in
  $_nr = ":"
  _in, _v4 = rule_ws(_in)
  _in, _v5 = pText(_in, ":")
  _in, _v6 = rule_ws(_in)
  _in, _v7 = rule_or(_in)
  t = _v7
  _v8 = ({ type: #Rule, name: n, term: t })
  return _in, _v8
)
rule_parser = (_in ->
  trace(_in, "rule_parser")
  _name = "parser"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = rule_name(_in)
  n = _v1
  _in, _v2 = rule_ws(_in)
  _in, _v3 = pText(_in, "=")
  _in, _v4 = rule_ws(_in)
  _in, _v5 = pText(_in, "parser")
  _in, _v6 = rule_ws(_in)
  _in, _v7 = pText(_in, "{")
  _in, _v8 = pStar(_in, (_in ->
    rule_rule(_in)
  ))
  rs = _v8
  _in, _v9 = _in
  $_nr = "a closing '}'"
  _in, _v10 = rule_ws(_in)
  _in, _v11 = pText(_in, "}")
  _v12 = ({ n: n, rs: rs })
  return _in, _v12
)
rule_start = (_in ->
  trace(_in, "rule_start")
  _name = "start"
  var $_nr = null
  _catch: (e ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if $_nr: _throw { t:#p, in: e.in, noreturn: true, msg: $_nr }
    _throw e
  )
  _in, _v1 = pStar(_in, (_in ->
    (_in ->
      _in, _v1 = pNot(_in, (_in ->
        (_in ->
          _in, _v1 = rule_name(_in)
          _in, _v2 = rule_ws(_in)
          _in, _v3 = pText(_in, "=")
          _in, _v4 = rule_ws(_in)
          _in, _v5 = pText(_in, "parser")
          _in, _v6 = rule_ws(_in)
          _in, _v7 = pText(_in, "{")
          return _in, _v7
        )(_in)
      ))
      _in, _v2 = pAny(_in)
      return _in, _v2
    )(_in)
  ))
  b = _v1
  _in, _v2 = rule_parser(_in)
  p = _v2
  _in, _v3 = pStar(_in, (_in ->
    pAny(_in)
  ))
  a = _v3
  _v4 = ({ before: b.toChar, after: a.toChar, rules: p.rs, name: p.n })
  return _in, _v4
)
parserParser = text -> pStart(rule_start, text)

// ---


// emitting
emit = (r, buf, ind, name ->
    _match(r.type == #Or): goto emitOr r, buf, ind
    _match(r.type == #And): goto emitAnd r, buf, ind
    _match(r.type == #Sub): goto emitSub r, buf, ind
    _match(r.type == #Not): goto emitNot r, buf, ind
    _match(r.type == #Opt): goto emitOpt r, buf, ind
    _match(r.type == #Star): goto emitStar r, buf, ind
    _match(r.type == #Plus): goto emitPlus r, buf, ind

    _match(r.type == #Cond): buf.write("_in\n$(ind)  if not ($(r.act)): _throw { t: #p, in: _in }")
    _match(r.type == #NoReturn): buf.write("_in\n$(ind)  \$_nr = \"$(r.msg.escape)\"\n")

    _match(r.type == #Call): buf.write("rule_$(r.rule)(_in)\n")
    _match(r.type == #Bind): emit(r.term, buf, ind); buf.write("$(ind)  $(r.name) = $name\n")
    _match(r.type == #Text): buf.write("pText(_in, \"$(r.text.escape)\")\n")
    _match(r.type == #Char): buf.write("pChar(_in, \"$(r.text.escape)\")\n")
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
emitOr = (r, buf, ind ->
    if r.terms.size == 0: return
    if r.terms.size == 1: return emit(r.terms.get(0), buf, ind)

    buf.write("$(ind)  pOr(_name, _nr, _in\n")
    r.terms.each: (r ->
        buf.write("$(ind)    ,(_in ->\n")
        emit r, buf, ind.cat("    ")
        buf.write("$(ind)     )\n")
    )
    buf.write("$(ind)  )\n")
)

emitSub = (r, buf, ind ->
    buf.write("(_in ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$(ind)  )(_in)\n")
)
emitRule = (r, buf ->
    name = r.name
    buf.write("rule_$name = (_in ->\n")
    buf.write("  trace(_in, \"rule_$name\")\n")
    // TODO only if has a pOr
    buf.write("  _name = \"$name\"\n")
    buf.write("  var \$_nr = null\n")
    // TODO only if rule has a noreturn
    buf.write("  _catch: (e ->\n")
    buf.write("    if e?t != #p: _throw e\n")
    buf.write("    if e?noreturn: _throw e\n")
    buf.write("    if \$_nr: _throw { t:#p, in: e.in, noreturn: true, msg: \$_nr }\n")
    buf.write("    _throw e\n")
    buf.write("  )\n")
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

