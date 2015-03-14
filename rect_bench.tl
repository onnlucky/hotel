COUNT = 10000
log.info("rect bench starting", COUNT)
start = time()

RectClass = {
    top = -> this.y
    bottom = -> this.y + this.height
    left = -> this.x
    right = -> this.x + this.width
    area = -> this.width * this.height
}
Rect = x, y, width, height -> {class=RectClass, {x, y, width, height}}

rects = Array.new
COUNT.times: n ->
    rects.add(Rect(n // 5 * 10, n % 5 * 10, 10, 10))
log.info("created")

var total_area = 0
rects.each: r -> total_area += r.area
log.info("total area:", total_area)

rects.randomize
log.info("shuffled")

rects.sort(l, r -> l.x < r.x or (l.x == r.x and l.y < r.y))
log.info("sorted")
runtime = time() - start
log.info("total time", runtime, "seconds")

if args[1] == "--publish":
    couchdb.open("rect_bench", "http://hotel.iriscouch.com", create=false).new(Object.set(system, {time=runtime, count=COUNT, ts=time().round}))
    log.info("published to hotel.iriscouch.com")

