Pi = 3.14159265358979
Tau = 2*Pi

samplerate = 44100
pwm = 1000
pwm_per_sample = pwm / (1. * samplerate) // Float(bitrate)

audio = Audio.open(samplerate, 2, Audio.byte8)

getbit = int, bit -> bool(binand(shift(1, bit - 1), int) > 0, 1, 0)

// should have a global buf and flush, now we write too much silence because we are not writing sample size
cmd = { motor, speed ->
    buf = Buffer.new
    var $pwm = .0

    print "cmd:", motor, speed
    writebit = { bit ->
        print "bit:", bit
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
    fsamples = 64
    fadetolow = -> fsamples.times: s -> buf.writeByte(128 - 128/fsamples*s); buf.writeByte(128)
    fadetozero = -> fsamples.times: s -> buf.writeByte(128/fsamples*s); buf.writeByte(128)

    var $high = 0
    fadetolow();
    3.to(1): n -> bit = getbit(motor, n); writebit(bit); $high += bit
    8.to(1): n -> bit = getbit(speed, n); writebit(bit); $high += bit
    writebit($high % 2)
    fadetozero();

    audio.write(buf)
    return
}

width = 0.07      // 7 cm between center of tracks
trackspeed = 0.01 // 1 cm/s motor speed at max

forward = { meters ->
    cmd(1, 255); cmd(2, 255)
    duration = meters / trackspeed
    print "dur:", duration
    sleep duration
    cmd(1, 128); cmd(2, 128);
}
turn = { degree ->
    cmd(1, 128); cmd(2, 128)
    duration = width * degree / trackspeed
    cmd(1, 255)
    print "dur:", duration
    sleep duration
    cmd(1, 128)
}

// 10 by 10 square ... (+ width / 2 for turns)
forward(.1)
turn(Tau/4)
forward(.1)
turn(Tau/4)
forward(.1)
turn(Tau/4)
forward(.1)
turn(Tau/4)

