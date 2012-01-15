parser {
    name: ls=alpha+  -> ls.join
  escape: "\\\""      -> "\""
        | "\\n"       -> "\n"
        | "\\r"       -> "\r"
        | "\\t"       -> "\t"
        | "\\\\"      -> "\\"
        | !"\"" l=_   -> l.toChar
    text: "\"" ts=escape* "\""             -> ts.join

     act: "->" ts=(!"\n" _)                -> ts.join

    term: "(". s=or .")"                   -> s
        | "."                              -> { type: #Ws }
        | "_"                              -> { type: #Any }
        | n=name                           -> { type: #Call, rule: n }
        | t=text                           -> { type: #Text, text: t }
    post: "!" t=term                       -> { type: #Not, term: t }
        | t=term "*"                       -> { type: #Star, term: t }
        | t=term "+"                       -> { type: #Plus, term: t }
        | t=term "?"                       -> { type: #Opt, term: t }
        | term
   named: n=name "=" t=post                -> { type: #Bind, name: n, term: t }
        | post
     and: h=named rs=(" ". named)* .a=act? -> { type: #And, terms: [h].cat(rs), act: a }
      or: h=and rs=(."|". and)*            -> { type: #Or, terms: [h].cat(rs) }
    rule: n=name .":". t=or                -> { type: #Rule, name: n, term: t }
   start: ."parser" ."{". rs=rule* ."}".   -> { type: #Parser, rules: rs }
}

