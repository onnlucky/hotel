FILE = "/tmp/tl-persist-test.db"

io.run "rm -f $FILE"
db = Persist.new(FILE)
obj = db.Object.get("hello")
obj.TESTING = "hello world!"
// db.close or db.flush ??

assert io.File(FILE).read.find("TESTING")
io.run "rm -f $FILE"

