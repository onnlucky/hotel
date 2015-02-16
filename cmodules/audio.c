#include "debug.h"
#include "../llib/lqueue.h"
#include <tl.h>

#include <portaudio.h>

#include "trace-off.h"

// TODO a little more buffering is in order, now if samples mismatch, we have to write silence
// and tl has only 1024 samples time to provide new buffer (at 44100 that is ~23ms)

TL_REF_TYPE(tlAudio);

struct tlAudio {
    tlLock lock;
    int channels;
    int format;
    PaStream* stream;

    // current task from which we are reading samples
    tlTask* current;
    // queue of tasks that have send buffers
    lqueue wait_q;
};

static tlKind _tlAudioKind = { .name = "Audio", .locked = true };
tlKind* tlAudioKind = &_tlAudioKind;

static tlBuffer* nextbuffer(tlAudio* audio) {
    while (true) {
        if (audio->current) {
            tlBuffer* buf = tlBufferAs(tlArgsGet(tlArgsAs(tlTaskGetValue(audio->current)), 0));
            if (tlBufferSize(buf) > 0) return buf;

            tlTaskSetValue(audio->current, buf);
            tlTaskReadyExternal(audio->current);
        }
        audio->current = tlTaskFromEntry(lqueue_get(&audio->wait_q));
        if (!audio->current) return null;
    }
    assert(false);
    return null;
}

static void streamfinish(void* data) {
    tlAudio* audio = tlAudioAs(data);
    trace("streamfinish: %p", audio);
    if (Pa_IsStreamActive(audio->stream)) {
        int err = Pa_StopStream(audio->stream);
        if (err) warning("portaudio error: %s", Pa_GetErrorText(err));
    }
}

static int writestream(const void* input, void* output, unsigned long frames, const PaStreamCallbackTimeInfo* time, PaStreamCallbackFlags status, void* data) {
    tlAudio* audio = tlAudioAs(data);
    trace("writestream: %d, %ld", audio->channels, frames);

    tlBuffer* buf = nextbuffer(audio);

    int silence = 0;
    uint8_t* out =(uint8_t*)output;
    for (unsigned long f = 0; f < frames; f++) {
        if (buf && tlBufferSize(buf) == 0) buf = nextbuffer(audio);
        for (int c = 0; c < audio->channels; c++) {
            if (buf) {
                *out++ = tlBufferReadByte(buf);
            } else {
                silence++;
                *out++ = 128; // silence ...
            }
        }
    }
    // exact sized buffers should still be swapped out, so the writing task can resume
    if (buf && tlBufferSize(buf) == 0) buf = nextbuffer(audio);
    if (silence) trace("written silence: %d", silence);
    return paContinue;
}

static bool _pa_init = false;
static void maybe_pa_init() {
    if (!_pa_init) {
        trace("init");
        _pa_init = true;
        int err = Pa_Initialize();
        if (err) fatal("portaudio error: %s", Pa_GetErrorText(err));
    }
}

tlAudio* AudioNew(int samplerate, int channels, unsigned format) {
    maybe_pa_init();
    tlAudio* audio = tlAlloc(tlAudioKind, sizeof(tlAudio));

    int err;
    err = Pa_OpenDefaultStream(&audio->stream, 0, channels, format, samplerate, 1024, writestream, audio);
    if (err) fatal("portaudio error: %s", Pa_GetErrorText(err));

    err = Pa_SetStreamFinishedCallback(audio->stream, streamfinish);
    if (err) fatal("portaudio error: %s", Pa_GetErrorText(err));

    audio->channels = channels;
    audio->format = format;

    return audio;
}

static tlHandle _Audio_open(tlTask* task, tlArgs* args) {
    int samplerate = tl_int_or(tlArgsGet(args, 0), -1);
    int channels = tl_int_or(tlArgsGet(args, 1), -1);
    int format = tl_int_or(tlArgsGet(args, 2), -1);
    if (samplerate <= 0 || samplerate > 1000000) TL_THROW("invalid sample rate");
    if (channels <= 0 || channels > 10) TL_THROW("invalid channel count");
    if (format < paFloat32 || format > paUInt8) TL_THROW("invalid audio format");

    tlAudio* audio = AudioNew(samplerate, channels, format);
    trace("open: %p", audio);
    return audio;
}

static tlHandle _audio_write(tlTask* task, tlArgs* args) {
    tlAudio* audio = tlAudioAs(tlArgsTarget(args));
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("require a buffer");

    tlTaskWaitExternal(task);
    lqueue_put(&audio->wait_q, tlTaskGetEntry(task));

    if (!Pa_IsStreamActive(audio->stream)) {
        int err = Pa_StartStream(audio->stream);
        if (err) fatal("portaudio error: %s", Pa_GetErrorText(err));
    }
    trace("playing");
    return null;
}

tlHandle tl_load() {
    if (!_tlAudioKind.klass) {
        _tlAudioKind.klass = tlClassObjectFrom(
            "write", _audio_write,
            null
        );
    }
    tlObject* AudioStatic = tlClassObjectFrom(
        "open", _Audio_open,
        "byte8", null,
        "byte16", null,
        null
    );
    tlObjectSet_(AudioStatic, tlSYM("byte8"), tlINT(paUInt8));
    tlObjectSet_(AudioStatic, tlSYM("byte16"), tlINT(paInt16));
    return AudioStatic;
}

