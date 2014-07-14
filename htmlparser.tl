selfclosing = Set("link","meta")
nonnesting = Set("p", "td", "tr", "a")

stream = _xml_parse(io.File("../../Downloads/test.html").readString)

Node = parent, name ->
    node = dom.Node.new(name)
    parent.add(node)
    return node

findparent = node, name ->
    if node.name == name: return node
    if not node.parent: return null
    findparent(node.parent, name)

pprint = node, indent ->
    indent = indent or ""
    if isString(node): print "$indent$node"; return
    if node._nodes.size == 0: print "$indent<$(node.name)/>"; return
    print "$indent<$(node.name)>"
    node._nodes.each: n ->
        pprint(n, indent + "  ")
    print "$indent</$(node.name)>"

process = node, cmd ->
    print "process", repr(cmd)
    { not cmd                 }: node
    { isString(cmd)           }: node.add(cmd); node
    { cmd.type == "open"      }:
        { selfclosing(cmd.name) }: Node(node, cmd.name); node
        { nonnesting(cmd.name)  }: parent = findparent(node, cmd.name)
                                   { parent }: Node(parent.parent, cmd.name)
                                   {        }: Node(node, cmd.name)
        {                       }: Node(node, cmd.name)
    { cmd.type == "close"     }:
        { selfclosing(cmd.name) }: node
        {                       }: parent = findparent(node, cmd.name)
                                   if parent: return parent
                                   return node
    { cmd.type == "selfclose" }:
        { nonnesting(cmd.name)  }: parent = findparent(node, cmd.name)
                                   { parent }: Node(parent.parent, cmd.name); parent.parent
                                   {        }: Node(node, cmd.name); node
        {                       }: Node(node, cmd.name); node

root = dom.Node.new("doc")
node = Var.new root
stream.each: cmd -> $node = process($node, cmd)

print(root.toString)

