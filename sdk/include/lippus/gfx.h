#pragma once

#include "abi.h"

#define LIPPUS_BLACK 0x0000u
#define LIPPUS_WHITE 0xFFFFu
#define LIPPUS_RED 0xF800u
#define LIPPUS_GREEN 0x07E0u
#define LIPPUS_BLUE 0x001Fu
#define LIPPUS_YELLOW 0xFFE0u
#define LIPPUS_CYAN 0x07FFu
#define LIPPUS_MAGENTA 0xF81Fu
#define LIPPUS_ORANGE LIPPUS_RGB(255, 140, 32)
#define LIPPUS_AMBER LIPPUS_RGB(255, 196, 72)
#define LIPPUS_CREAM LIPPUS_RGB(245, 236, 220)

/* Magenta e' il colore trasparente di default per gli sprite. */
#define LIPPUS_COLOR_KEY LIPPUS_MAGENTA

static inline int gfx_width(void) { return (int)lippus_api()->width; }
static inline int gfx_height(void) { return (int)lippus_api()->height; }

static inline void gfx_clear(uint16_t color) { lippus_api()->clear(color); }

static inline void gfx_pixel(int x, int y, uint16_t color) {
    lippus_api()->pixel(x, y, color);
}

static inline void gfx_fill_rect(int x, int y, int w, int h, uint16_t color) {
    lippus_api()->fill_rect(x, y, w, h, color);
}

static inline void gfx_rect(int x, int y, int w, int h, uint16_t color) {
    lippus_api()->rect(x, y, w, h, color);
}

static inline void gfx_line(int x0, int y0, int x1, int y1, uint16_t color) {
    lippus_api()->line(x0, y0, x1, y1, color);
}

static inline void gfx_circle(int cx, int cy, int r, uint16_t color) {
    lippus_api()->circle(cx, cy, r, color);
}

static inline void gfx_fill_circle(int cx, int cy, int r, uint16_t color) {
    lippus_api()->fill_circle(cx, cy, r, color);
}

/* pixels e' RGB565 nativo, riga per riga. Se use_key != 0, i pixel uguali
   a key non vengono disegnati. */
static inline void gfx_blit(int x, int y, int w, int h, const uint16_t *pixels,
                            int use_key, uint16_t key) {
    lippus_api()->blit(x, y, w, h, pixels, use_key, key);
}

/* scale >= 1. '\\n' va a capo. I caratteri fuori da ASCII 32..126 avanzano vuoti. */
static inline void gfx_text(int x, int y, const char *s, uint16_t color, int scale) {
    lippus_api()->text(x, y, s, color, scale);
}

static inline int gfx_text_width(const char *s, int scale) {
    return lippus_api()->text_width(s, scale);
}

/* w o h <= 0 azzera il clip e torna allo schermo intero. */
static inline void gfx_clip(int x, int y, int w, int h) {
    lippus_api()->clip(x, y, w, h);
}

/* Invia il framebuffer al display. Chiamala una volta per frame:
   da qui parte anche il controllo di START+SELECT per tornare al menu. */
static inline void gfx_present(void) { lippus_api()->present(); }
