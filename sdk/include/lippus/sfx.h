#pragma once

#include "lippus/audio.h"

/*
 * Sweep di frequenza sulle voci gia' presenti.
 *
 * Il firmware miscela quattro voci a 20 kHz (quadra, triangolo, rumore)
 * e una nota resta alla frequenza con cui e' stata lanciata. Un gioco non
 * puo' fare un sintetizzatore campione per campione: non registra interrupt
 * e non scrive il PWM. sfx_sweep fa salire o scendere la nota un passo per
 * frame; sfx_tick va chiamato ogni frame, con i millisecondi di quel frame.
 *
 * Duty cycle, dente di sega e inviluppo ADSR non ci sono nel mixer: la
 * quadra copre le varianti di duty e il dente di sega, la durata copre
 * l'inviluppo. Per una fanfara di note ferme resta snd_tune.
 */

typedef struct sfx_slot {
    uint8_t on;
    uint8_t wave;
    uint8_t volume;
    int16_t start_hz;
    int16_t end_hz;
    uint16_t total_ms;
    uint16_t done_ms;
} sfx_slot_t;

static inline sfx_slot_t *sfx_slots(void) {
    static sfx_slot_t slots[SND_VOICES];
    return slots;
}

static inline void sfx_sweep(unsigned voice, unsigned wave, int start_hz, int end_hz,
                             unsigned volume, unsigned ms) {
    if (voice >= SND_VOICES || ms == 0 || volume == 0 || wave == SND_OFF) {
        return;
    }
    if (start_hz < 40) {
        start_hz = 40;
    }
    if (end_hz < 40) {
        end_hz = 40;
    }
    sfx_slot_t *slot = &sfx_slots()[voice];
    slot->on = 1;
    slot->wave = (uint8_t)wave;
    slot->volume = (uint8_t)volume;
    slot->start_hz = (int16_t)start_hz;
    slot->end_hz = (int16_t)end_hz;
    slot->total_ms = (uint16_t)ms;
    slot->done_ms = 0;
    snd_voice(voice, wave, (unsigned)start_hz, volume, ms);
}

static inline void sfx_tick(unsigned frame_ms) {
    if (frame_ms == 0) {
        frame_ms = 1;
    }
    sfx_slot_t *slots = sfx_slots();
    for (unsigned voice = 0; voice < SND_VOICES; voice++) {
        sfx_slot_t *slot = &slots[voice];
        if (!slot->on) {
            continue;
        }
        unsigned done = slot->done_ms + frame_ms;
        if (done >= slot->total_ms) {
            slot->on = 0;
            snd_voice(voice, SND_OFF, 0, 0, 0);
            continue;
        }
        slot->done_ms = (uint16_t)done;
        int hz = slot->start_hz +
                 (slot->end_hz - slot->start_hz) * (int)done / (int)slot->total_ms;
        if (hz < 40) {
            hz = 40;
        }
        if (hz > 8000) {
            hz = 8000;
        }
        unsigned left = slot->total_ms - slot->done_ms;
        snd_voice(voice, slot->wave, (unsigned)hz, slot->volume, left + frame_ms);
    }
}
