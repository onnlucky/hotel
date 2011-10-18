x = 100
test1 = ->{ out(x, "\n"); }
x = 200
test2 = ->{ out(x, "\n"); }
x = 300
test3 = ->{ out(x, "\n"); }
x = 400
test4 = ->{ out(x, "\n"); }

test1();
test2();
test3();
test4();

x = test1;

test3();
test2();
x();
