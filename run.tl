goto = { pcont => { &fn => _goto(pcont, fn) }}
return = { pcont => { *args => _return(pcont, args) }}

goto(print("here"), print("second"))

