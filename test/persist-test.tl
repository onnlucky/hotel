FILE = "/tmp/tl-persist-test.db"

io.run "rm -f $FILE"
db = Persist.new(FILE)
obj = db.Object.get("hello")
obj.TESTING = "hello world!"
list = db.List.get("foobar")
list.add "hello world!"
list.add "the end"
// db.close or db.flush ??

assert io.File(FILE).readString.find("TESTING")

db = Persist.new(FILE)
obj = db.Object.get("hello")
assert obj.TESTING == "hello world!"

list = db.List.get("foobar")
list.each: i -> assert i == "hello world!" or i == "the end"

io.run "rm -f $FILE"
