<doc>
/author: Onne Gorter
/title: Hotel doc proposal

Any blob of _styled_ *text* here. /see(there) `identifier`

<code>
print "hello world!"
</code>

/h1: all the above
All the above actually parses into something very concrete:
<code>
{
    name: 'doc
    nodes: [
        { name: 'author, text: "Onne Gorter" }
        { name: 'title, text: "Hotel doc proposal" }
        { name: 'p, nodes: {
            { text: "Any blob of " }
            { name: 'i, text: "styled" }
            { name: 'b, text: "text" }
            { text: " here. " }
            { name: 'see, text: "there" }
            { text: " " }
            { name: 'c, text: "identifier" }
        }
        { name: 'code, text: "print \"hello world!\"" }
        { name: h1, text: "all the above" }
        // ... and more, but showing that here, well ...
    ]
}
</code>

</doc>

print "hello world"

</doc>
and the above would just run if we were to execute this file ...
</doc>

