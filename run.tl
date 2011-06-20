cc, extra = continuation
print "HELLO", extra

done = return
bool(extra,{print("!! DONE !!"); done(); print("!! ERROR 2 !!")},{})()

cc("WORLD!")
print("ERROR")

