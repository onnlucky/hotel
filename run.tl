src = "tlbuilder"
repo = "onnlucky.com:hotel.git"

var $good = null
var $fail = null
var $at = "init"

!http.Server.new(8080).serve: (conn ->
    conn.setContentType "text/html"
    conn.write "<p>now: $($at)"
    if $fail: conn.write "<p><b>failed</b>: $($fail.at)"
    if $good: conn.write "<p>good: $($good)"
)

run = (cmd ->
    print cmd
    $at = cmd
    io.Process.run cmd
)

print "starting: $repo (in $src)"
run "rm -rf $src"
run "git clone $repo $src"
io.chdir src
loop: (
    (
        _catch: (e->
            print "Error:", e
            $fail = { at: $at }
        )
        run "git reset --hard"
        run "git clean -xfd"
        run "git pull --force"
        run "make"
        run "./tl test/fib.tl"
        $good = run "git log --oneline -n1"
    )
    sleep minutes 5
)

