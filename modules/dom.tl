// a DOM like api with named nodes and anonymous text nodes
// TODO add a Document too, for some meta stuff like doctype and such

// TODO use a set instead ...
selfclosing = [img=1,meta=1,link=1]
isSelfClosing = name -> return selfclosing[name] or false

blocks = [h1=1,h2=1,h3=1,p=1,div=1,ul=1,ol=1,article=1,section=1,header=1,footer=1,nav=1
          figure=1,table=1,thead=1,tr=1,td=1,
          error=1,script=1,style=1,doc=1,title=1,meta=1,link=1]
isBlock = name -> return blocks[name] or false

containers = [html=1,body=1,div=1,article=1,section=1,header=1,footer=1,nav=1,table=1]
isContainer = name -> return containers[name] or false

iter2 = node, fn ->
    fn(node)
    var $n = node.firstChild
    while $n:
        iter2($n, fn)
        $n = $n.next
    return null

iter = node, fn ->
    fn = fn or args.block
    if not fn: throw "require a block or cb"
    iter2(node, fn)

DomEntry = {
    remove= ->
        assert @parent
        if @parent.firstChild == this: @parent.firstChild = @next
        if @parent.lastChild == this: @parent.lastChild = @prev
        @parent = null
        if @prev: @prev.next = @next
        if @next: @next.prev = @prev
        @prev = null
        @next = null
        return this

    addBefore= node ->
        assert not node.parent
        assert @parent
        node.parent = @parent
        old = @prev
        @prev = node
        node.prev = old; node.next = this
        if old: old.next = node; return this
        @parent.firstChild = node
        return this

    addAfter= node ->
        assert not node.parent
        assert @parent
        node.parent = @parent
        old = @next
        @next = node
        node.prev = this; node.next = old
        if old: old.prev = node; return this
        @parent.lastChild = node
        return this
}

DomText = {
    class= DomEntry
    toString= -> return @text or ""
    firstChild= null
    lastChild= null
    attrs= null
}

Text = {
    new= (text ->
        _Mutable_new {
            parent= null
            prev= null
            next= null

            text= text or ""
            class= DomText
        }
    )
}

DomNode = {
    class= DomEntry

    _add= node ->
        if not @lastChild:
            node.parent = this
            @firstChild = node
            @lastChild = node
            return
        @lastChild.addAfter(node)

    add= node ->
        if not node: return this
        if isString(node): this._add(Text.new(node)); return this
        // TODO if node"has"("each"): ...
        if isList(node): node.each(n -> this.add(n)); return this
        this._add(node)
        return this

    clear= ->
        @firstChild = null
        @lastChild = null
        return this

    attr= key, value ->
        if args.size == 1: return @attrs[key]
        @attrs[key] = value
        return this

    toString= mode ->
        // TODO hotel should provide a StringBuilder or such ... but this works
        sb = Array.new
        if mode != "children":
            sb.add("<$(@name)")
            @attrs.each: k, v -> sb.add(" $k=\"$v\"")
            if ((not @firstChild) and isSelfClosing(@name)):
                sb.add("/>")
                if isBlock(@name): sb.add("\n")
                return sb.join
            sb.add(">")
            if isContainer(@name): sb.add("\n")

        var $n = @firstChild
        while $n:
            sb.add($n)
            $n = $n.next

        if mode != "children":
            sb.add("</$(@name)>")
            if isBlock(@name): sb.add("\n")

        sb.join

    // think Element.textContent
    text= ->
        sb = Array.new
        iter(this): n -> if n.class == DomText: sb.add(n.text)
        sb.join

    each= fn ->
        fn = args.block or fn
        if not fn: throw "expect a block or function"
        var $n = @firstChild
        var $m = $n?next
        while $n: fn($n); $n = $m; $m = $n?next
        return this

    find= sel -> sizzle.find(sel, this)
    findAll= sel -> sizzle.findAll(sel, this)
}

Node = {
    new= (name, children, attrs ->
        //assert (not attrs) or HashMap.is(attrs)
        n = _Mutable_new {
            parent= null
            prev= null
            next= null
            firstChild= null
            lastChild= null

            name= (name or "").lower
            attrs= attrs or HashMap.new
            class= DomNode
        }
        if children: n.add(children)
        return n
    )
}

test "dom nodes":
    n = Node.new("div")
    p1 = Node.new("p", "1")
    p2 = Node.new("p", "2")
    p3 = Node.new("p").add(Text.new("3"))
    n.add([p1, p2, p3])
    assert n.firstChild.firstChild.text == "1"
    assert n.firstChild.next.firstChild.text == "2"
    assert n.firstChild.next.next.firstChild.text == "3"
    assert n.lastChild.firstChild.text == "3"
    assert n.lastChild.prev.firstChild.text == "2"
    assert n.lastChild.prev.prev.firstChild.text == "1"

    p4 = Node.new("crap").attr("foo", 42)
    assert p4.attr("foo") == 42
    assert p4.attrs["foo"] == 42
    p1.addAfter(p4)
    assert n.firstChild.firstChild.text == "1"
    assert n.firstChild.next.name == "crap"
    assert n.firstChild.next.next.firstChild.text == "2"
    assert n.lastChild.prev.prev.prev.firstChild.text == "1"

    p4.remove
    assert n.firstChild.firstChild.text == "1"
    assert n.firstChild.next.next.firstChild.text == "3"
    assert n.lastChild.prev.prev.firstChild.text == "1"
    assert p4.attr("foo") == 42

{{ Text, Node }}

