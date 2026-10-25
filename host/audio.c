#include "host.h"

#include "lippus/audio.h"

#include <AudioToolbox/AudioToolbox.h>
#include <os/lock.h>
#include <string.h>

/* Stesso mixer del firmware, a 20 kHz, mandato alla cassa del Mac. */

#define SAMPLE_RATE 20000u
#define HZ_MAX 8000u

typedef struct {
    uint8_t wave;
    uint8_t volume;
    uint8_t timed;
    uint32_t inc;
    uint32_t phase;
    uint32_t left;
    uint32_t noise;
} voice_t;

static voice_t voices[SND_VOICES];
static os_unfair_lock audio_lock = OS_UNFAIR_LOCK_INIT;
static AudioQueueRef audio_queue;

static const lippus_note_t *tune_notes;
static uint16_t tune_count;
static uint16_t tune_pos;
static uint32_t tune_left;
static uint8_t tune_voice;
static uint8_t tune_wave;
static uint8_t tune_volume;
static uint8_t tune_on;

static uint32_t hz_to_inc(uint16_t hz) {
    if (hz > HZ_MAX) {
        hz = HZ_MAX;
    }
    return (uint32_t)(((uint64_t)hz << 32) / SAMPLE_RATE);
}

static uint32_t noise_step(uint32_t n) {
    if (n == 0) {
        n = 1u;
    }
    uint32_t bit = ((n >> 0) ^ (n >> 2) ^ (n >> 3) ^ (n >> 5)) & 1u;
    return (n >> 1) | (bit << 30);
}

static void voice_program(unsigned voice, unsigned wave, unsigned hz, unsigned volume, unsigned ms) {
    if (voice >= SND_VOICES) {
        return;
    }
    voice_t *v = &voices[voice];
    if (wave > SND_NOISE || hz == 0 || volume == 0 || wave == SND_OFF) {
        v->wave = SND_OFF;
        v->timed = 0;
        v->left = 0;
        v->inc = 0;
        return;
    }
    uint32_t inc = hz_to_inc((uint16_t)hz);
    int same = v->wave == wave && v->inc == inc && v->volume == (uint8_t)volume;
    v->wave = (uint8_t)wave;
    v->volume = (uint8_t)volume;
    v->inc = inc;
    if (!same) {
        v->phase = 0;
        if (v->noise == 0) {
            v->noise = 0x123456u;
        }
    }
    if (ms == 0) {
        v->timed = 0;
        v->left = 0;
    } else {
        v->timed = 1;
        v->left = (uint32_t)ms * (SAMPLE_RATE / 1000u);
        if (v->left == 0) {
            v->left = 1;
        }
    }
}

static int voice_sample(voice_t *v) {
    if (v->wave == SND_OFF) {
        return 0;
    }
    if (v->timed) {
        if (v->left == 0) {
            v->wave = SND_OFF;
            return 0;
        }
        v->left--;
    }
    uint32_t phase = v->phase;
    v->phase = phase + v->inc;
    int sample = 0;
    if (v->wave == SND_SQUARE) {
        sample = (phase & 0x80000000u) ? 127 : -128;
    } else if (v->wave == SND_TRIANGLE) {
        uint32_t ramp = phase >> 23;
        uint32_t mag = ramp & 255u;
        if (ramp & 256u) {
            mag = 255u - mag;
        }
        sample = (int)mag - 128;
    } else {
        if (v->phase < phase) {
            v->noise = noise_step(v->noise);
        }
        sample = (v->noise & 1u) ? 127 : -128;
    }
    return (sample * (int)v->volume) >> 8;
}

static void tune_step(void) {
    if (!tune_on) {
        return;
    }
    if (tune_left > 0) {
        tune_left--;
        return;
    }
    if (tune_pos >= tune_count) {
        tune_on = 0;
        voice_program(tune_voice, SND_OFF, 0, 0, 0);
        return;
    }
    const lippus_note_t *note = &tune_notes[tune_pos++];
    unsigned wave = note->hz == 0 ? SND_OFF : tune_wave;
    voice_program(tune_voice, wave, note->hz, tune_volume, 0);
    tune_left = (uint32_t)note->ms * (SAMPLE_RATE / 1000u);
    if (tune_left == 0) {
        tune_left = 1;
    }
}

static int mix_one(void) {
    tune_step();
    int mix = 0;
    for (unsigned i = 0; i < SND_VOICES; i++) {
        mix += voice_sample(&voices[i]);
    }
    if (mix > 127) {
        mix = 127;
    }
    if (mix < -128) {
        mix = -128;
    }
    return mix;
}

static void audio_callback(void *user, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    (void)user;
    int frames = (int)(buffer->mAudioDataBytesCapacity / sizeof(int16_t));
    int16_t *out = (int16_t *)buffer->mAudioData;
    os_unfair_lock_lock(&audio_lock);
    for (int i = 0; i < frames; i++) {
        out[i] = (int16_t)(mix_one() << 8);
    }
    os_unfair_lock_unlock(&audio_lock);
    buffer->mAudioDataByteSize = (UInt32)(frames * (int)sizeof(int16_t));
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

void host_audio_init(void) {
    for (unsigned i = 0; i < SND_VOICES; i++) {
        voices[i].noise = 0x123456u + i;
    }
    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof format);
    format.mSampleRate = SAMPLE_RATE;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBitsPerChannel = 16;
    format.mChannelsPerFrame = 1;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 2;
    format.mBytesPerPacket = 2;
    if (AudioQueueNewOutput(&format, audio_callback, NULL, NULL, NULL, 0, &audio_queue) != 0) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        AudioQueueBufferRef buffer = NULL;
        AudioQueueAllocateBuffer(audio_queue, 400 * sizeof(int16_t), &buffer);
        if (!buffer) {
            continue;
        }
        buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
        memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
        AudioQueueEnqueueBuffer(audio_queue, buffer, 0, NULL);
    }
    AudioQueueStart(audio_queue, NULL);
}

void host_sound(uint8_t voice, uint8_t wave, uint16_t hz, uint8_t volume, uint16_t ms) {
    if (voice >= SND_VOICES) {
        return;
    }
    os_unfair_lock_lock(&audio_lock);
    if (tune_on && tune_voice == voice) {
        tune_on = 0;
    }
    voice_program(voice, wave, hz, volume, ms);
    os_unfair_lock_unlock(&audio_lock);
}

void host_tone(uint16_t hz) {
    if (hz == 0) {
        host_sound(0, SND_OFF, 0, 0, 0);
    } else {
        host_sound(0, SND_SQUARE, hz, 255, 0);
    }
}

void host_tune(uint8_t voice, uint8_t wave, uint8_t volume, const lippus_note_t *notes, uint16_t count) {
    if (voice >= SND_VOICES) {
        return;
    }
    if (wave == SND_OFF || wave > SND_NOISE) {
        wave = SND_SQUARE;
    }
    if (volume == 0) {
        volume = 255;
    }
    os_unfair_lock_lock(&audio_lock);
    tune_on = 0;
    if (!notes || count == 0) {
        voice_program(voice, SND_OFF, 0, 0, 0);
        os_unfair_lock_unlock(&audio_lock);
        return;
    }
    tune_notes = notes;
    tune_count = count;
    tune_pos = 0;
    tune_left = 0;
    tune_voice = voice;
    tune_wave = wave;
    tune_volume = volume;
    tune_on = 1;
    os_unfair_lock_unlock(&audio_lock);
}
