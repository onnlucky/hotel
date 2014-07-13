selfclosing = Set("link","meta")
nonnesting = Set("p")

stream = _xml_parse("<link><p>1<p>2<p/>3<p>4")
print repr stream

_methods = {
    add = node -> this._nodes.add(node)
}
Node = parent, name ->
    {
        {parent,name}
        _nodes = Array.new
        class = _methods
    }

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
    { not cmd                 }: node
    { isString(cmd)           }: node.add(cmd); node
    { cmd.type == "open"      }:
        { selfclosing(cmd.name) }: node.add(Node(node, cmd.name)); node
        { nonnesting(cmd.name)  }: parent = findparent(node, cmd.name)
                                   { parent }: parent.parent.add(Node(parent.parent, cmd.name))
                                   {        }: node.add(Node(node, cmd.name))
        {                       }: node.add(Node(node, cmd.name))
    { cmd.type == "close"     }:
        { selfclosing(cmd.name) }: node
        {                       }: parent = findparent(node, cmd.name)
                                   if parent: return parent
                                   return node
    { cmd.type == "selfclose" }:
        { nonnesting(cmd.name)  }: parent = findparent(node, cmd.name)
                                   { parent }: parent.parent.add(Node(parent.parent, cmd.name)); parent.parent
                                   {        }: node.add(Node(node, cmd.name)); node
        {                       }: node.add(Node(node, cmd.name)); node

root = Node(null, "root")
node = Var.new root
stream.each: cmd ->
    $node = process($node, cmd)


pprint(root)

print nonnesting("p")

