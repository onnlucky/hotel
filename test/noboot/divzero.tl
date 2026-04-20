out(10 // 3, "\n")
out(10 % 3, "\n")
(
    catch: out("idiv by zero\n")
    10 // 0
    out("idiv no error\n")
)
(
    catch: out("mod by zero\n")
    10 % 0
    out("mod no error\n")
)
