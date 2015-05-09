addresses = as, opt ->
    { not as       }: []
    { isString(as) }: [opt, as]
    {              }: [opt, to.join(",")]

addresses_to = as ->
    { not as       }: []
    { isString(as) }: [as]
    {              }: [to.join(",")]

#. send(subject, body, to, from?): send emails using the 'mail' system command
#. [cc]: add a carbon copy address, may be a String, or a List
#. [bcc]: add a blind carbon copy address, may be a String or a List
#. to: the address to send the email to, may be a String or a List
#. from: the sender, if not given
send = subject, body, to, from ->
    to = addresses_to(to)
    if to.size == 0 or to.size == 1 and to[1].size == 0: Error("require at least a 'to' address").throw
    from = addresses(from, "-r")
    cc = addresses(args["cc"], "-c")
    bcc = addresses(args["bcc"], "-b")

    cmd = (["mail", "-s", toString(subject)] + cc + bcc + from + to).filter
    child = _io_launch("/tmp", cmd, true, true, false, true, null, false)
    s = io.Stream.new(child.in); s.write(body); s.close
    output = io.Stream.new(child.out).readString
    status = child.wait
    if status != 0: Error("sending email failed: $output").throw

{{ send }}

