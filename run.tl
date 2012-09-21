Pi = 3.14159265358979
Tau = 2*Pi

samplerate = 44100
pwm = 500
pwm_per_sample = pwm / (1. * samplerate) // Float(bitrate)

audio = Audio.open(samplerate, 2, Audio.byte8)

getbit = int, bit -> bool(binand(shift(1, bit - 1), int) > 0, 1, 0)

fsamples = 64
fadein = { buf ->
    if buf.size > 0: return
    fsamples.times: s -> buf.writeByte(128 - 128/fsamples*s); buf.writeByte(128)
}
fadeout = { buf ->
    if buf.size > 0: return
    fsamples.times: s -> buf.writeByte(128/fsamples*s); buf.writeByte(128)
}
cmd = { buf, motor, speed ->
    if not isNumber(speed): return
    speed = speed.round
    speed = bool(speed < 256, speed, 255)
    speed = bool(speed >= 0, speed, 0)
    print "cmd", motor, speed
    var $pwm = .0
    writebit = { bit ->
        while $pwm < 0.5: {
            buf.writeByte(255)     // left = clock
            buf.writeByte(bit*255) // right = data
            $pwm += pwm_per_sample
        }
        while $pwm < 1.0: {
            buf.writeByte(0)       // left = clock
            buf.writeByte(bit*255) // right = data
            $pwm += pwm_per_sample
        }
        $pwm -= 1.0
    }

    var $high = 0
    3.to(1): n -> bit = getbit(motor, n); writebit(bit); $high += bit
    8.to(1): n -> bit = getbit(speed, n); writebit(bit); $high += bit
    writebit($high % 2)
    return
}

buf = Buffer.new
romo = _Object_new {
    right: { speed ->
        fadein(buf); cmd(buf, 1, (speed + 1)*128)
    }
    left: { speed ->
        fadein(buf); cmd(buf, 2, (speed * -1 + 1)*128)
    }
    flush: { ->
        if buf.size == 0: return
        fadeout(buf); audio.write(buf);
    }
    forward: { speed -> this.right(speed); this.left(speed); this.flush }
    stop: { ->
        cmd(buf, 1, 128); cmd(buf, 2, 128); this.flush
    }
}

/*
romo.right(1)
romo.left(1)
romo.flush
sleep 0.1
romo.stop
*/

!http.Server.new(8080).serve: { conn ->
    _catch: {
        e -> print e.toText
        conn.write("oeps:", e.toText);
    }
    print conn.req.url.path
    if conn.req.url.path == "/": {
        conn.setContentType("text/html")
        conn.write io.File("index.html").read
        return
    }
    if conn.req.url.path.startsWith("/stop"): romo.stop; romo.stop
    if conn.req.url.path.startsWith("/cmd"): {
        q = conn.req.url.query
        q.split("&").each: { cmd ->
            kv = cmd.split("=")
            if kv.get(0) == "left": romo.left(kv.get(1).eval)
            if kv.get(0) == "right": romo.right(kv.get(1).eval)
        }
        romo.flush
    }
    conn.setContentType("text/html")
    conn.write("<a href='stop'>stop</a>")
}

return null

/*
width = 0.075     // 7 cm between center of tracks
drag = 1.3
trackspeed = 0.19 // 19 cm/s motor speed at max

stop = { -> cmd(1, 128); cmd(2, 128); cmd(1, 128); cmd(2, 128) }

forward = { meters ->
    duration = meters / trackspeed
    cmd(1, 255);
    cmd(2, 0)
    print "dur", duration
    sleep duration
}
turn = { degree ->
    cmd(1, 128); cmd(2, 128)
    duration = drag * width * degree / trackspeed
    if duration > 0: cmd(1, 255); cmd(2, 255)
    if duration < 0: cmd(1, 0); cmd(2, 0)
    print "dur:", duration
    sleep bool(duration > 0, duration, -1 * duration)
}

// 10 by 10 square
forward(.1)
turn(Tau/-4)
forward(.1)
turn(Tau/-4)
forward(.1)
turn(Tau/-4)
forward(.1)
turn(Tau/-4)
stop()

