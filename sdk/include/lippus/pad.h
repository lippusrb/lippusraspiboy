#pragma once

#include "abi.h"

/* I tasti si campionano ogni 16 ms. pressed e released restano
   memorizzati finche' non chiami pad_read. held e' lo stato stabile.
   I bit sono attivi quando il tasto e' premuto (l'hardware chiude verso GND). */

static inline void pad_read(lippus_pad_t *out) { lippus_api()->pad(out); }

static inline lippus_pad_t pad_poll(void) {
    lippus_pad_t pad;
    pad_read(&pad);
    return pad;
}

static inline int pad_held(const lippus_pad_t *pad, uint16_t button) {
    return (pad->held & button) != 0;
}

static inline int pad_pressed(const lippus_pad_t *pad, uint16_t button) {
    return (pad->pressed & button) != 0;
}

static inline int pad_released(const lippus_pad_t *pad, uint16_t button) {
    return (pad->released & button) != 0;
}
