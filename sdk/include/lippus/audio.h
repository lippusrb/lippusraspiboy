#pragma once

#include "abi.h"

/* Quattro voci sul PWM GP20, filtrate dal passa-basso e dal potenziometro.
   Il volume qui e' relativo, da 0 a 255: il potenziometro resta il volume vero.
   ms == 0 tiene la nota finche' non la cambi. snd_tone usa la voce 0. */

#define SND_OFF 0u
#define SND_SQUARE 1u
#define SND_TRIANGLE 2u
#define SND_NOISE 3u
#define SND_VOICES 4u

static inline void snd_tone(unsigned hz) { lippus_api()->tone((uint16_t)hz); }

static inline void snd_stop(void) { lippus_api()->tone(0); }

static inline void snd_voice(unsigned voice, unsigned wave, unsigned hz, unsigned volume,
                             unsigned ms) {
    lippus_api()->sound((uint8_t)voice, (uint8_t)wave, (uint16_t)hz, (uint8_t)volume,
                        (uint16_t)ms);
}

/* Suona notes in sottofondo sulla voce scelta. L'array deve restare valido.
   count == 0 interrompe la musichetta. */
static inline void snd_tune(unsigned voice, unsigned wave, unsigned volume,
                            const lippus_note_t *notes, unsigned count) {
    lippus_api()->tune((uint8_t)voice, (uint8_t)wave, (uint8_t)volume, notes,
                       (uint16_t)count);
}
