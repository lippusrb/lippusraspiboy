#pragma once

#include <stdint.h>

#include "layout.h"

/* ABI fra il gioco e il firmware. Si aggiungono campi solo in coda.
   Se il layout cambia, va incrementato LIPPUS_API_VERSION. */

#define LIPPUS_UP (1u << 0)
#define LIPPUS_DOWN (1u << 1)
#define LIPPUS_LEFT (1u << 2)
#define LIPPUS_RIGHT (1u << 3)
#define LIPPUS_CLICK (1u << 4)
#define LIPPUS_A (1u << 5)
#define LIPPUS_B (1u << 6)
#define LIPPUS_X (1u << 7)
#define LIPPUS_Y (1u << 8)
#define LIPPUS_START (1u << 9)
#define LIPPUS_SELECT (1u << 10)

typedef struct lippus_pad {
    uint16_t held;
    uint16_t pressed;
    uint16_t released;
} lippus_pad_t;

/* Una nota di snd_tune. hz == 0 e' una pausa. */
typedef struct lippus_note {
    uint16_t hz;
    uint16_t ms;
} lippus_note_t;

typedef struct lippus_api {
    uint32_t version;
    uint16_t width;
    uint16_t height;
    void (*clear)(uint16_t color);
    void (*pixel)(int x, int y, uint16_t color);
    void (*fill_rect)(int x, int y, int w, int h, uint16_t color);
    void (*rect)(int x, int y, int w, int h, uint16_t color);
    void (*line)(int x0, int y0, int x1, int y1, uint16_t color);
    void (*circle)(int cx, int cy, int r, uint16_t color);
    void (*fill_circle)(int cx, int cy, int r, uint16_t color);
    void (*blit)(int x, int y, int w, int h, const uint16_t *pixels, int use_key,
                 uint16_t key);
    void (*text)(int x, int y, const char *s, uint16_t color, int scale);
    int (*text_width)(const char *s, int scale);
    void (*clip)(int x, int y, int w, int h);
    void (*present)(void);
    void (*pad)(lippus_pad_t *out);
    uint32_t (*ticks)(void);
    void (*sleep_ms)(uint32_t ms);
    void (*tone)(uint16_t hz);
    void (*exit_to_menu)(void);
    void (*sound)(uint8_t voice, uint8_t wave, uint16_t hz, uint8_t volume, uint16_t ms);
    void (*tune)(uint8_t voice, uint8_t wave, uint8_t volume, const lippus_note_t *notes,
                 uint16_t count);
} lippus_api_t;

/* Puntatore riempito dallo startup del gioco. Il firmware non lo definisce. */
extern const lippus_api_t *g_lippus_api;

static inline const lippus_api_t *lippus_api(void) {
    return g_lippus_api;
}
