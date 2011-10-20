bootup = (&postpone){ return postpone }
postponed = bootup(out("hello\n"))
postponed()
postponed()
postponed()
postponed()
out("DONE\n")
