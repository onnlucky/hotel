parser { name: ls=letters+ post: "!" term | term "*" | term "+" | term
  escape: "\\\"" | "\\n" | "\\r" | "\\t" | "\\\\" | !"\"" _
    text: "\"" escape* "\""
    term: "(". or .")"
        | "."
        | "_"
        | name
        | text
   named: n=name "=" t=post              -> Bind.{ name: n, term: t }
        | post
     and: h=named rs=(" ". named)* a=act -> And.{ terms: h + rs, act: a }
      or: h=and rs=(."|". and)*          -> Or.{ terms: h + rs }
    rule: n=name .":". t=or              -> Rule.{ name: n, term: t }
  parser: ."parser" ."{". rs=rule* ."}". -> Parser.{ rules: rs }
}

