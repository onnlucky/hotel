fields = {
    width = 0
    height = 1
}
methods = {
    area = -> this.width * this.height
}
constructor = width, height ->
    print "constructor"
    this.width = width
    this.height = height

Rect = UserClass("Rect", constructor, fields, methods)
print Rect
print Rect.name
Rect(20, 30).area

