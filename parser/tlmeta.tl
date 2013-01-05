
// --- tlmeta-base.tl ---
DEBUG=false
DEBUG2=false
trace = { in, &msg -> if DEBUG: print in.pos, in.text.escape.slice(in.pos, in.pos + 10), "--", msg() }
trace2 = { in, &msg -> if DEBUG2: print in.pos, in.text.escape.slice(in.pos, in.pos + 10), "--", msg() }

WS = " \t\n\r"
LOWER = "abcdefghijklmnopqrstuvwxyz"
UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ALPHA = LOWER.cat(UPPER)
NUMERIC = "0123456789"
ALPHANUMERIC = ALPHA.cat(NUMERIC)

advance = { in, adv -> { text: in.text, pos: in.pos + adv, oop: in.oop }}
error = { in, nr, msg ->
    if nr: throw { t: #p, in: in, msg: nr, noreturn: true }
    throw {t: #p, in: in, msg: msg }
}

rule_pass = { in, nr ->
    trace2(in, "pass")
    return in
}
rule_any = { in, nr ->
    trace2(in, "any")
    c = in.text.get(in.pos)
    if not c: error(in, nr, "eof")
    return advance(in, 1), c
}
rule_end = { in, nr ->
    trace2(in, "end")
    c = in.text.get(in.pos)
    if c: error(in, nr, "end")
    return in, null
}
pText = { in, nr, text ->
    trace(in, " >>  TRY \"$(text.escape)\"")
    if in.text.startsWith(text, in.pos): {
        trace(in, " << !OK!");
        return advance(in, text.size), text
    }
    trace(in, " << FAIL");
    error(in, nr, text)
}
pChar = { in, nr, text ->
    trace2(in, "pChar: $text")
    c = in.text.get(in.pos)
    if not c: error(in, nr, "[$text]")
    if text.find(c.toChar): return advance(in, 1), c
    error(in, nr, "[$text]")
}
pNot = { in, nr, parser ->
    trace2(in, "pNot")
    in = (
        catch: { e =>
            if e?t != #p: throw e
            if e?noreturn: throw e
            return in, null
        }
        parser(in)
    )
    error(in, nr, "not")
}
pAhead = { in, nr, parser ->
    trace2(in, "pAhead")
    in2 = parser(in, nr)
    return in
}
pOpt = { in, nr, parser ->
    trace2(in, "pOpt")
    catch: { e =>
        if e?t != #p: throw e
        if e?noreturn: throw e
        return in, null
    }
    parser(in)
}
_pmany = { in, parser, res ->
    trace2(in, "_pmany: $(res.size)")
    catch: { e =>
        if e?t != #p: throw e
        if e?noreturn: throw e
        trace2(in, " << pStar|pPlus");
        return in, res
    }
    in, v = parser(in)
    goto _pmany(in, parser, res.add(v))
}
pStar = { in, nr, parser ->
    trace2(in, " >> pStar")
    _pmany(in, parser, [])
}
pPlus = { in, nr, parser ->
    trace2(in, " >> pPlus")
    in, v = parser(in, nr)
    _pmany(in, parser, [v])
}
pOr = { in, nr, name ->
    var $i = 3
    // TODO instead of just the last one, collect all alternatives
    loop: {
        parser = args.get($i)
        if not parser: break
        trace2(in, "pOr: $name: $($i)")
        $i += 1
        catch: { e =>
            if e?t != #p: throw e
            if e?noreturn: throw e
        }
        in, v = parser(in)
        trace2(in, "pOr: $name <<")
        return in, v
    }
    trace2(in, "pOr: $name: no alternatives")
    error(in, nr, "a $name")
}

pHandle = { e, nr, name ->
    if e?t != #p: throw e
    trace(e.in, " << FAIL $name")
    if e?noreturn: throw e
    error(e.in, nr, e.msg)
}
pAct = { in, v -> return in, v }

colFromPos = { text, pos ->
    var $col = 0
    loop: {
        at = pos - $col - 1
        if at < 0: return pos
        if text.startsWith("\n", at): return $col
        $col += 1
    }
}
lineFromPos = { text, pos ->
    // TODO bit inefficient ... but works
    lines = text.slice(0, pos).split("\n")
    var $last = lines.size - 1
    loop: {
        if $last < 0: return 1
        line = lines.get($last)
        if line.strip.size == 0: $last -= 1; continue
        return $last + 1, line.size + 1
    }
}

_tllookup = { oop, name, pos ->
    hit = oop._cache.get("$name-$pos")
    if hit: {
        //print "HIT: $name $pos -> $(hit.in.pos) '$(hit.v)'";
        return hit
    }
    return
}
_tlcache = { oop, name, pos, in, v ->
    //print "CACHE: $name $pos -> $(in.pos) '$v'";
    oop._cache.set("$name-$pos", {in: in, v: v})
    return in, v
}

pStart = { rule, text ->
    oop = _Object_new({ _cache: HashMap.new })
    _with_lock(oop): {
        in, v = (
            catch: { e ->
                if e?t != #p: throw e
                line, char = lineFromPos(e.in.text, e.in.pos)
                if e?msg: throw "SyntaxError: expected $(e.msg) on line $line col $char"
                throw "SyntaxError: on line $line col $char"
            }
            rule({text: text, pos: 0, oop: oop })
        )
        if in.pos < in.text.size: {
            line, char = lineFromPos(in.text, in.pos)
            throw "SyntaxError: expected end of input on line $line col $char"
        }
        return v
    }
}

rule_line = in -> return in, lineFromPos(in.text, in.pos)
rule_column = in -> return in, colFromPos(in.text, in.pos)

rule_lower = in, nr -> pChar(in, nr, LOWER)
rule_upper = in, nr -> pChar(in, nr, UPPER)
rule_alpha = in, nr -> pChar(in, nr, ALPHA)
rule_numeric = in, nr -> pChar(in, nr, NUMERIC)
rule_alphanumeric = in, nr -> pChar(in, nr, ALPHANUMERIC)

rule_ws = { in, nr ->
    trace2(in, "rule_ws")
    c = in.text.get(in.pos)
    if not c: return in, null
    if WS.find(c.toChar): goto(rule_ws(advance(in, 1)))
    return in, null
}

rule_wsnl = { in ->
    trace(in, "rule_ws")
    c = in.text.get(in.pos)
    if not c: return in, null
    if WS.find(c.toChar): goto(rule_ws(advance(in, 1)))
    return in, null
}


// --- parser ---
rule_sp = {_in, _nr ->
  trace(_in, " >>  TRY sp", _nr)
  this = _in.oop
  _name = "sp"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pOr(_in, _nr, _name
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, " ")
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\t")
     }
  )
  trace(_in, " << !OK! sp")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_nl = {_in, _nr ->
  trace(_in, " >>  TRY nl", _nr)
  this = _in.oop
  _name = "nl"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pOr(_in, _nr, _name
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\r\n")
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\n\r")
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\n")
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\r")
     }
  )
  trace(_in, " << !OK! nl")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_slcomment = {_in, _nr ->
  trace(_in, " >>  TRY slcomment", _nr)
  this = _in.oop
  _name = "slcomment"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pText(_in, _nr, "//")
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = rule_nl(_in, _nr)
      })
      _in, _v = rule_any(_in, _nr)
    }(_in, _nr)
  })
  trace(_in, " << !OK! slcomment")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_icomment = {_in, _nr ->
  trace(_in, " >>  TRY icomment", _nr)
  this = _in.oop
  _name = "icomment"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pText(_in, _nr, "/*")
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = pText(_in, _nr, "*/")
      })
      _in, _v = rule_any(_in, _nr)
    }(_in, _nr)
  })
  _in, _v = {_in, _nr ->
    _in, _v = pOr(_in, _nr, _name
      ,{_in, _nr ->
        _in, _v = pText(_in, _nr, "*/")
       }
      ,{_in, _nr ->
        _in, _v = rule_end(_in, _nr)
       }
    )
  }(_in, _nr)
  trace(_in, " << !OK! icomment")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_comment = {_in, _nr ->
  trace(_in, " >>  TRY comment", _nr)
  this = _in.oop
  _name = "comment"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = {_in, _nr ->
    _in, _v = pOr(_in, _nr, _name
      ,{_in, _nr ->
        _in, _v = rule_slcomment(_in, _nr)
        _in, _v = {_in, _nr ->
          _in, _v = pOr(_in, _nr, _name
            ,{_in, _nr ->
              _in, _v = rule_nl(_in, _nr)
             }
            ,{_in, _nr ->
              _in, _v = rule_end(_in, _nr)
             }
          )
        }(_in, _nr)
       }
      ,{_in, _nr ->
        _in, _v = rule_icomment(_in, _nr)
       }
    )
  }(_in, _nr)
  trace(_in, " << !OK! comment")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_ws = {_in, _nr ->
  trace(_in, " >>  TRY ws", _nr)
  this = _in.oop
  _name = "ws"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = pOr(_in, _nr, _name
        ,{_in, _nr ->
          _in, _v = rule_sp(_in, _nr)
         }
        ,{_in, _nr ->
          _in, _v = rule_nl(_in, _nr)
         }
        ,{_in, _nr ->
          _in, _v = rule_comment(_in, _nr)
         }
      )
    }(_in, _nr)
  })
  trace(_in, " << !OK! ws")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_name = {_in, _nr ->
  trace(_in, " >>  TRY name", _nr)
  this = _in.oop
  _name = "name"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pPlus(_in, _nr, {_in, _nr ->
    _in, _v = rule_alpha(_in, _nr)
  })
  ls = _v; pAct(_in, _v)
  _in, _v = pAct(_in, (ls.toChar))
  trace(_in, " << !OK! name")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_escape = {_in, _nr ->
  trace(_in, " >>  TRY escape", _nr)
  this = _in.oop
  _name = "escape"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pOr(_in, _nr, _name
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\\"")
      _in, _v = pAct(_in, ("\""))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\n")
      _in, _v = pAct(_in, ("\n"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\r")
      _in, _v = pAct(_in, ("\r"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\t")
      _in, _v = pAct(_in, ("\t"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\\$")
      _in, _v = pAct(_in, ("\$"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\\\")
      _in, _v = pAct(_in, ("\\"))
     }
    ,{_in, _nr ->
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = pText(_in, _nr, "\"")
      })
      _in, _v = rule_any(_in, _nr)
      l = _v; pAct(_in, _v)
      _in, _v = pAct(_in, (l.toChar))
     }
  )
  trace(_in, " << !OK! escape")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_text = {_in, _nr ->
  trace(_in, " >>  TRY text", _nr)
  this = _in.oop
  _name = "text"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pText(_in, _nr, "\"")
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = rule_escape(_in, _nr)
  })
  ts = _v; pAct(_in, _v)
  _nr = "a closing '\"'"
  _in, _v = pText(_in, _nr, "\"")
  _in, _v = pAct(_in, (ts.join))
  trace(_in, " << !OK! text")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_cscape = {_in, _nr ->
  trace(_in, " >>  TRY cscape", _nr)
  this = _in.oop
  _name = "cscape"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pOr(_in, _nr, _name
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\]")
      _in, _v = pAct(_in, ("]"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\n")
      _in, _v = pAct(_in, ("\n"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\r")
      _in, _v = pAct(_in, ("\r"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\t")
      _in, _v = pAct(_in, ("\t"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\\$")
      _in, _v = pAct(_in, ("\$"))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "\\\\")
      _in, _v = pAct(_in, ("\\"))
     }
    ,{_in, _nr ->
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = pText(_in, _nr, "]")
      })
      _in, _v = rule_any(_in, _nr)
      l = _v; pAct(_in, _v)
      _in, _v = pAct(_in, (l.toChar))
     }
  )
  trace(_in, " << !OK! cscape")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_char = {_in, _nr ->
  trace(_in, " >>  TRY char", _nr)
  this = _in.oop
  _name = "char"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pText(_in, _nr, "[")
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = rule_cscape(_in, _nr)
  })
  ts = _v; pAct(_in, _v)
  _nr = "closing ']'"
  _in, _v = pText(_in, _nr, "]")
  _in, _v = pAct(_in, (ts.join))
  trace(_in, " << !OK! char")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_act = {_in, _nr ->
  trace(_in, " >>  TRY act", _nr)
  this = _in.oop
  _name = "act"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pText(_in, _nr, "->")
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = pText(_in, _nr, "\n")
      })
      _in, _v = rule_any(_in, _nr)
    }(_in, _nr)
  })
  ts = _v; pAct(_in, _v)
  _in, _v = pAct(_in, (ts.toChar))
  trace(_in, " << !OK! act")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_cond = {_in, _nr ->
  trace(_in, " >>  TRY cond", _nr)
  this = _in.oop
  _name = "cond"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pText(_in, _nr, "?")
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pText(_in, _nr, "(")
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = pText(_in, _nr, ")")
      })
      _in, _v = rule_any(_in, _nr)
    }(_in, _nr)
  })
  ts = _v; pAct(_in, _v)
  _in, _v = pText(_in, _nr, ")")
  _in, _v = pAct(_in, (ts.toChar))
  trace(_in, " << !OK! cond")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_term = {_in, _nr ->
  trace(_in, " >>  TRY term", _nr)
  this = _in.oop
  _name = "term"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pOr(_in, _nr, _name
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "(")
      _in, _v = rule_ws(_in, _nr)
      _in, _v = rule_or(_in, _nr)
      t = _v; pAct(_in, _v)
      _nr = "a closing ')'"
      _in, _v = rule_ws(_in, _nr)
      _in, _v = pText(_in, _nr, ")")
      _in, _v = pAct(_in, ({ type: #Sub, term: t }))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "..")
      _in, _v = pAct(_in, ({ type: #Wsnl }))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, ".")
      _in, _v = pAct(_in, ({ type: #Ws }))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "_")
      _in, _v = pAct(_in, ({ type: #Any }))
     }
    ,{_in, _nr ->
      _in, _v = rule_cond(_in, _nr)
      c = _v; pAct(_in, _v)
      _in, _v = pAct(_in, ({ type: #Cond, act: c }))
     }
    ,{_in, _nr ->
      _in, _v = rule_name(_in, _nr)
      n = _v; pAct(_in, _v)
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = pText(_in, _nr, ":")
      })
      _in, _v = pAct(_in, ({ type: #Call, rule: n }))
     }
    ,{_in, _nr ->
      _in, _v = rule_text(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pAct(_in, ({ type: #Text, text: t }))
     }
    ,{_in, _nr ->
      _in, _v = rule_char(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pAct(_in, ({ type: #Char, text: t }))
     }
  )
  trace(_in, " << !OK! term")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_post = {_in, _nr ->
  trace(_in, " >>  TRY post", _nr)
  this = _in.oop
  _name = "post"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pOr(_in, _nr, _name
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "!")
      _nr = "a term"
      _in, _v = rule_term(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pAct(_in, ({ type: #Not, term: t }))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "&")
      _nr = "a term"
      _in, _v = rule_term(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pAct(_in, ({ type: #Ahead, term: t }))
     }
    ,{_in, _nr ->
      _in, _v = rule_term(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pText(_in, _nr, "*")
      _in, _v = pAct(_in, ({ type: #Star, term: t }))
     }
    ,{_in, _nr ->
      _in, _v = rule_term(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pText(_in, _nr, "+")
      _in, _v = pAct(_in, ({ type: #Plus, term: t }))
     }
    ,{_in, _nr ->
      _in, _v = rule_term(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pText(_in, _nr, "?")
      _in, _v = pAct(_in, ({ type: #Opt, term: t }))
     }
    ,{_in, _nr ->
      _in, _v = rule_term(_in, _nr)
     }
  )
  trace(_in, " << !OK! post")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_named = {_in, _nr ->
  trace(_in, " >>  TRY named", _nr)
  this = _in.oop
  _name = "named"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pOr(_in, _nr, _name
    ,{_in, _nr ->
      _in, _v = rule_name(_in, _nr)
      n = _v; pAct(_in, _v)
      _in, _v = pText(_in, _nr, "=")
      _nr = "a term"
      _in, _v = rule_post(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pAct(_in, ({ type: #Bind, name: n, term: t }))
     }
    ,{_in, _nr ->
      _in, _v = pText(_in, _nr, "@")
      _in, _v = rule_text(_in, _nr)
      t = _v; pAct(_in, _v)
      _in, _v = pAct(_in, ({ type: #NoReturn, msg: t }))
     }
    ,{_in, _nr ->
      _in, _v = rule_post(_in, _nr)
     }
  )
  trace(_in, " << !OK! named")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_and = {_in, _nr ->
  trace(_in, " >>  TRY and", _nr)
  this = _in.oop
  _name = "and"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = rule_named(_in, _nr)
  h = _v; pAct(_in, _v)
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = rule_ws(_in, _nr)
      _in, _v = rule_named(_in, _nr)
    }(_in, _nr)
  })
  rs = _v; pAct(_in, _v)
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pOpt(_in, _nr, {_in, _nr ->
    _in, _v = rule_act(_in, _nr)
  })
  a = _v; pAct(_in, _v)
  _in, _v = pAct(_in, ({ type: #And, terms: [h].cat(rs), act: a }))
  trace(_in, " << !OK! and")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_or = {_in, _nr ->
  trace(_in, " >>  TRY or", _nr)
  this = _in.oop
  _name = "or"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _nr = "some terms"
  _in, _v = rule_and(_in, _nr)
  h = _v; pAct(_in, _v)
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = rule_ws(_in, _nr)
      _in, _v = pText(_in, _nr, "|")
      _in, _v = rule_ws(_in, _nr)
      _in, _v = rule_and(_in, _nr)
    }(_in, _nr)
  })
  rs = _v; pAct(_in, _v)
  _in, _v = pAct(_in, ({ type: #Or, terms: [h].cat(rs) }))
  trace(_in, " << !OK! or")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_rule = {_in, _nr ->
  trace(_in, " >>  TRY rule", _nr)
  this = _in.oop
  _name = "rule"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = rule_ws(_in, _nr)
  _in, _v = rule_name(_in, _nr)
  n = _v; pAct(_in, _v)
  _nr = ":"
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pText(_in, _nr, ":")
  _in, _v = rule_ws(_in, _nr)
  _in, _v = rule_or(_in, _nr)
  t = _v; pAct(_in, _v)
  _in, _v = pAct(_in, ({ type: #Rule, name: n, term: t }))
  trace(_in, " << !OK! rule")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_parser = {_in, _nr ->
  trace(_in, " >>  TRY parser", _nr)
  this = _in.oop
  _name = "parser"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = rule_name(_in, _nr)
  n = _v; pAct(_in, _v)
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pText(_in, _nr, "=")
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pText(_in, _nr, "parser")
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pText(_in, _nr, "{")
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = rule_rule(_in, _nr)
  })
  rs = _v; pAct(_in, _v)
  _nr = "a closing '}'"
  _in, _v = rule_ws(_in, _nr)
  _in, _v = pText(_in, _nr, "}")
  _in, _v = pAct(_in, ({ n: n, rs: rs }))
  trace(_in, " << !OK! parser")
  goto _tlcache(this, _name, _pos, _in, _v)
}
rule_start = {_in, _nr ->
  trace(_in, " >>  TRY start", _nr)
  this = _in.oop
  _name = "start"
  _pos = _in.pos
  _hit = _tllookup(this, _name, _pos)
  if _hit: return _hit.in, _hit.v
  catch: e -> pHandle(e, _nr, _name)
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = {_in, _nr ->
      _in, _v = pNot(_in, _nr, {_in, _nr ->
        _in, _v = {_in, _nr ->
          _in, _v = rule_name(_in, _nr)
          _in, _v = rule_ws(_in, _nr)
          _in, _v = pText(_in, _nr, "=")
          _in, _v = rule_ws(_in, _nr)
          _in, _v = pText(_in, _nr, "parser")
          _in, _v = rule_ws(_in, _nr)
          _in, _v = pText(_in, _nr, "{")
        }(_in, _nr)
      })
      _in, _v = rule_any(_in, _nr)
    }(_in, _nr)
  })
  b = _v; pAct(_in, _v)
  _in, _v = rule_parser(_in, _nr)
  p = _v; pAct(_in, _v)
  _in, _v = pStar(_in, _nr, {_in, _nr ->
    _in, _v = rule_any(_in, _nr)
  })
  a = _v; pAct(_in, _v)
  _in, _v = pAct(_in, ({ before: b.toChar, after: a.toChar, rules: p.rs, name: p.n }))
  trace(_in, " << !OK! start")
  goto _tlcache(this, _name, _pos, _in, _v)
}
parserParser = text -> pStart(rule_start, text)

// ---


// emitting
emit = { r, buf, ind, name ->
    _match(r.type == #Or): goto emitOr r, buf, ind
    _match(r.type == #And): goto emitAnd r, buf, ind
    _match(r.type == #Sub): goto emitSub r, buf, ind
    _match(r.type == #Not): goto emitNot r, buf, ind
    _match(r.type == #Opt): goto emitOpt r, buf, ind
    _match(r.type == #Star): goto emitStar r, buf, ind
    _match(r.type == #Plus): goto emitPlus r, buf, ind

    _match(r.type == #Ahead): goto emitAhead r, buf, ind
    _match(r.type == #Cond): buf.write("$ind  if not ($(r.act)): error(_in, _nr, \"if not ($(r.act.escape))\")\n")
    _match(r.type == #Char): buf.write("$ind  _in, _v = pChar(_in, _nr, \"$(r.text.escape)\")\n")

    _match(r.type == #NoReturn): buf.write("$ind  _nr = \"$(r.msg.escape)\"\n")
    _match(r.type == #Call): buf.write("$ind  _in, _v = rule_$(r.rule)(_in, _nr)\n")
    _match(r.type == #Bind): emit(r.term, buf, ind); buf.write("$(ind)  $(r.name) = _v; pAct(_in, _v)\n")
    _match(r.type == #Text): buf.write("$ind  _in, _v = pText(_in, _nr, \"$(r.text.escape)\")\n")
    _match(r.type == #Ws): buf.write("$ind  _in, _v = rule_ws(_in, _nr)\n")
    _match(r.type == #Wsnl): buf.write("$ind  _in, _v = rule_wsnl(_in, _nr)\n")
    _match(r.type == #Any): buf.write("$ind  _in, _v = rule_any(_in, _nr)\n")
    _match(true): throw "InternalError emit unknown: $(r.type)"
    _nomatch
}
emitAhead = { r, buf, ind ->
    buf.write("$ind  _in, _v = pAhead(_in, _nr, {_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  })\n")
}
emitNot = { r, buf, ind ->
    buf.write("$ind  _in, _v = pNot(_in, _nr, {_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  })\n")
}
emitOpt = { r, buf, ind ->
    buf.write("$ind  _in, _v = pOpt(_in, _nr, {_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  })\n")
}
emitStar = { r, buf, ind ->
    buf.write("$ind  _in, _v = pStar(_in, _nr, {_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  })\n")
}
emitPlus = { r, buf, ind ->
    buf.write("$ind  _in, _v = pPlus(_in, _nr, {_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  })\n")
}
emitAnd = { r, buf, ind ->
    var $n = 0
    // TODO maybe collect all binds, and "call" r.act
    r.terms.each: r -> emit r, buf, ind
    if r.act: buf.write("$ind  _in, _v = pAct(_in, ($(r.act)))\n")
}
emitOr = { r, buf, ind ->
    if r.terms.size == 0: return
    if r.terms.size == 1: return emit(r.terms.get(0), buf, ind)

    buf.write("$ind  _in, _v = pOr(_in, _nr, _name\n")
    r.terms.each: { r =>
        buf.write("$ind    ,{_in, _nr ->\n")
        emit r, buf, ind.cat("    ")
        buf.write("$ind     }\n")
    }
    buf.write("$ind  )\n")
}
emitSub = { r, buf, ind ->
    buf.write("$ind  _in, _v = {_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  }(_in, _nr)\n")
}
emitRule = { r, buf ->
    name = r.name
    buf.write("rule_$name = {_in, _nr ->\n")
    buf.write("  trace(_in, \" >>  TRY $name\", _nr)\n")
    buf.write("  this = _in.oop\n")
    buf.write("  _name = \"$name\"\n")
    buf.write("  _pos = _in.pos\n")
    buf.write("  _hit = _tllookup(this, _name, _pos)\n")
    buf.write("  if _hit: return _hit.in, _hit.v\n");
    buf.write("  catch: e -> pHandle(e, _nr, _name)\n")
    emit r.term, buf, ""
    buf.write("  trace(_in, \" << !OK! $name\")\n")
    buf.write("  goto _tlcache(this, _name, _pos, _in, _v)\n")
    buf.write("}\n")
}

if not args.get(1): throw "Usage: <input.tl> <output.tl>"

ast = parserParser(io.File(args.get(0)).read)

buf = io.Buffer.new
buf.write(ast.before)
buf.write("\n// --- tlmeta-base.tl ---\n")
buf.write(io.File("tlmeta-base.tl").read)
buf.write("\n// --- parser ---\n")
var $hasstart = false
ast.rules.each: { r ->
    if r.name == "start": $hasstart = true
    emitRule r, buf
}
start = (if $hasstart: "start") or ast.rules.get(0).name
buf.write("$(ast.name)$("") = text -> pStart(rule_$start, text)\n")
buf.write("\n// ---\n")
buf.write(ast.after)

io.File(args.get(1)).write(buf)
print "written '$(args.get(1))'"

