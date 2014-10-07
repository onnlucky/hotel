selfclosing = Set("link","meta","!doctype")
nonnesting = Set("p","td","tr","a","img","hr","li")
htmlelement = Set("!doctype","a","p","div","meta","link","html","head","title","body","b","i","li","ol","ul","img","h1","h2","h3","h4","h5")

create_node = parent, name, attrs ->
    node = dom.Node.new(name, attrs)
    parent.add(node)
    return node

findparent = node, name ->
    if node.name == name: return node
    if not node.parent: return null
    findparent(node.parent, name)

normalizename = name ->
    lower = name.lower
    { htmlelement(lower) }: lower
    {                    }: name

normalizews = text -> text.trim.replace("\t"," ").split(" ").join(" ")

process = node, cmd ->
    if not cmd: return node
    if isString(cmd): node.add(normalizews(cmd)); return node
    name = normalizename(cmd.name)
    { cmd.type == "open"      }:
        { selfclosing(name) }: create_node(node, name, cmd.attrs); node
        { nonnesting(name)  }: parent = findparent(node, name)
                               { parent }: create_node(parent.parent, name, cmd.attrs)
                               {        }: create_node(node, name, cmd.attrs)
        {                   }: node = create_node(node, name, cmd.attrs)
                               if try(cmd.content): node.add(cmd.content)
                               return node
    { cmd.type == "close"     }:
        { selfclosing(name) }: node
        {                   }: parent = findparent(node, name)
                               if parent: return parent.parent
                               return node
    { cmd.type == "selfclose" }:
        { nonnesting(name)  }: parent = findparent(node, name)
                               { parent }: create_node(parent.parent, name, cmd.attrs); parent.parent
                               {        }: create_node(node, name, cmd.attrs); node
        {                       }: create_node(node, name, cmd.attrs); node

parse = s ->
    stream = _xml_parse(s)
    root = dom.Node.new("doc")
    var $node = root
    stream.each: cmd -> $node = process($node, cmd)
    return root

{{ parse }}

