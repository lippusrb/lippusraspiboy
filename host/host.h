#pragma once

#include <stdint.h>

#include "lippus/abi.h"

void host_gfx_init(int width, int height);
const uint16_t *host_fb(void);
int host_fb_width(void);
int host_fb_height(void);

void host_gfx_clear(uint16_t color);
void host_gfx_pixel(int x, int y, uint16_t color);
void host_gfx_fill_rect(int x, int y, int w, int h, uint16_t color);
void host_gfx_rect(int x, int y, int w, int h, uint16_t color);
void host_gfx_line(int x0, int y0, int x1, int y1, uint16_t color);
void host_gfx_circle(int cx, int cy, int r, uint16_t color);
void host_gfx_fill_circle(int cx, int cy, int r, uint16_t color);
void host_gfx_blit(int x, int y, int w, int h, const uint16_t *pixels, int use_key, uint16_t key);
void host_gfx_text(int x, int y, const char *text, uint16_t color, int scale);
int host_gfx_text_width(const char *text, int scale);
void host_gfx_clip(int x, int y, int w, int h);

void host_audio_init(void);
void host_tone(uint16_t hz);
void host_sound(uint8_t voice, uint8_t wave, uint16_t hz, uint8_t volume, uint16_t ms);
void host_tune(uint8_t voice, uint8_t wave, uint8_t volume, const lippus_note_t *notes, uint16_t count);

void host_window_open(const char *title, int width, int height);
void host_window_present(void);
void host_pump(void);
int host_quit_requested(void);

void host_keys_read(lippus_pad_t *out);
uint16_t host_keys_held(void);

uint32_t host_ticks(void);
void host_sleep(uint32_t ms);
void host_quit(void);
void host_bind_api(int width, int height);
