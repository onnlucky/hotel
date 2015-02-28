fields = {
    width = 0
    height = 1
}
methods = {
    area = -> this.width * this.height
}
constructor = width, height ->
    UserClass.setField(this, "width", width)
    UserClass.setField(this, "height", height)
    return this

Rect = UserClass("Rect", constructor, fields, methods)
Rect(20, 30).area

