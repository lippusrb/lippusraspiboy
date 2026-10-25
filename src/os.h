#pragma once

#include "lippus/abi.h"
#include "lippus/lbg.h"
#include "lippus/layout.h"

void log_init(void);
void lippus_log(const char *msg);

void lippus_exit(void);
void lippus_service(void);
void lippus_service_reset(void);
const lippus_api_t *lippus_api_table(void);
int lippus_enter_game(void (*entry)(const lippus_api_t *api));

void gfx_init(void);
void lippus_gfx_clear(uint16_t color);
void lippus_gfx_pixel(int x, int y, uint16_t color);
void lippus_gfx_fill_rect(int x, int y, int w, int h, uint16_t color);
void lippus_gfx_rect(int x, int y, int w, int h, uint16_t color);
void lippus_gfx_line(int x0, int y0, int x1, int y1, uint16_t color);
void lippus_gfx_circle(int cx, int cy, int r, uint16_t color);
void lippus_gfx_fill_circle(int cx, int cy, int r, uint16_t color);
void lippus_gfx_blit(int x, int y, int w, int h, const uint16_t *pixels, int use_key,
                     uint16_t key);
void lippus_gfx_text(int x, int y, const char *s, uint16_t color, int scale);
int lippus_gfx_text_width(const char *s, int scale);
void lippus_gfx_clip(int x, int y, int w, int h);
void lippus_gfx_present(void);
void lippus_gfx_orientation(unsigned mode);
int lippus_gfx_width(void);
int lippus_gfx_height(void);
void gfx_pause_for_flash(void);
void gfx_resume_after_flash(void);
void lippus_api_set_size(uint16_t width, uint16_t height);

void pad_init(void);
uint16_t pad_raw(void);
void pad_reset(void);
void lippus_pad_read(lippus_pad_t *out);

void audio_init(void);
void lippus_tone(uint16_t hz);
void lippus_sound(uint8_t voice, uint8_t wave, uint16_t hz, uint8_t volume, uint16_t ms);
void lippus_tune(uint8_t voice, uint8_t wave, uint8_t volume, const lippus_note_t *notes,
                 uint16_t count);

uint32_t lippus_ticks(void);
void lippus_sleep(uint32_t ms);
