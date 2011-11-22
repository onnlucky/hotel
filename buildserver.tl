var $state = { status: "init", time: Time.now }

!http.Server.new 9090: (conn ->
    conn.write "<li>state: $( $state.status )<li>updated: $( $state.time )"
    conn.done
)

src = "src"
repo = "dhcp91:virga"

os.run "git clone $repo $src"
loop: (
    (
        os.cwd src
        os.run "git reset"
        os.run "git clean -xfd"
        os.run "git pull --force"
        os.run "make"
        os.run "./tl test/smoke.tl"
        $state = { status: "success", time: Time.now }
    ) catch: (e->
        $state = { status: e.toText, time: Time.now }
    )
    sleep 5.minutes
)

