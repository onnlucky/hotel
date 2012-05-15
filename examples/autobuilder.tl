#!/usr/bin/env tl

// config
src = "builder-hotel-src"
repo = "onnlucky.com:hotel.git"

// state
var $good = null
var $fail = null
var $at = "init"

// a simple webserver serving a very simple status response (on any req)
// the "!" means, run in a new task (run async if you will)
!http.Server.new(8080).serve: { conn ->
    print conn.req.url.path.slice(0, 14)
    conn.setContentType "text/html"
    conn.write "<head><title>hotel auto builder</title></head>"
    conn.write "<p>now: $($at)"
    if $fail: conn.write "<p><b>failed</b>: $($fail.at)<pre>$($fail.output)</pre>"
    if $good: conn.write "<p>last build:<pre>$($good)</pre>"
}

// track every command we run, for status
run = { cmd ->
    print cmd
    $at = cmd
    io.run cmd
}

// prepare
print "starting: $repo (in $src)"
run "rm -rf $src"
run "git clone $repo $src"
io.chdir src

// loop forever, inside the checked out dir, building and testing and waiting
loop: {
    (
        // TODO need a syntax for try/catch ...
        _catch: { e ->
            print "Error:", $at
            $fail = { at: $at, output: e?output }
        }
        run "git reset --hard"
        run "git clean -xfd"
        run "git pull --force"
        run "make"
        run "./tl test/fib.tl"
        run "make test"
        $good = run "git log -n1"
        $fail = null
    )
    $at = "waiting"
    sleep minutes 5
}

