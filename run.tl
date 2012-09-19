samplerate = 44100
pwm = 1000
pwm_per_sample = pwm / (1. * samplerate) // Float(bitrate)

audio = Audio.open(samplerate, 2, Audio.byte8)

getbit = int, bit -> bool(binand(shift(1, bit - 1), int) > 0, 1, 0)

cmd = { motor, speed ->
    buf = Buffer.new
    var $pwm = .0
    var $clock = 0 // start low

    print "cmd:", motor, speed
    writebit = { bit ->
        print "bit:", bit
        while $pwm < 0.5: {
            buf.writeByte($clock*255) // left = clock
            buf.writeByte(bit*255)    // right = data
            $pwm += pwm_per_sample
        }
        $clock = bool($clock == 1, 0, 1)
        while $pwm < 1.0: {
            buf.writeByte($clock*255) // left = clock
            buf.writeByte(bit*255)    // right = data
            $pwm += pwm_per_sample
        }
        $pwm -= 1.0
    }

    // TODO fade to clock?
    var $high = 0
    3.to(1): n -> bit = getbit(motor, n); writebit(bit); $high += bit
    8.to(1): n -> bit = getbit(speed, n); writebit(bit); $high += bit
    writebit($high % 2)

    audio.write(buf)
    return
}

// doing it like this actually writes silence inbetween, because we have to write at sample size
cmd(1, 255)
cmd(2, 255)
sleep 5
cmd(1, 128)
cmd(2, 128)

