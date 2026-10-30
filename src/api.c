#include "os.h"

#include "fat.h"

#include "pico/stdlib.h"

#include <setjmp.h>

static jmp_buf s_exit;
static volatile int s_in_game;
static uint32_t s_combo_since;

const lippus_api_t *lippus_api_table(void);

static lippus_api_t s_api = {
    .version = LIPPUS_API_VERSION,
    .width = LIPPUS_SCREEN_W,
    .height = LIPPUS_SCREEN_H,
    .clear = lippus_gfx_clear,
    .pixel = lippus_gfx_pixel,
    .fill_rect = lippus_gfx_fill_rect,
    .rect = lippus_gfx_rect,
    .line = lippus_gfx_line,
    .circle = lippus_gfx_circle,
    .fill_circle = lippus_gfx_fill_circle,
    .blit = lippus_gfx_blit,
    .text = lippus_gfx_text,
    .text_width = lippus_gfx_text_width,
    .clip = lippus_gfx_clip,
    .present = lippus_gfx_present,
    .pad = lippus_pad_read,
    .ticks = lippus_ticks,
    .sleep_ms = lippus_sleep,
    .tone = lippus_tone,
    .exit_to_menu = lippus_exit,
    .sound = lippus_sound,
    .tune = lippus_tune,
    .record_read = lippus_record_read,
    .record_write = lippus_record_write,
};

const lippus_api_t *lippus_api_table(void) { return &s_api; }

void lippus_api_set_size(uint16_t width, uint16_t height) {
    s_api.width = width;
    s_api.height = height;
}

uint32_t lippus_ticks(void) { return (uint32_t)(time_us_64() / 1000u); }

int lippus_record_read(const char *name, void *dst, uint32_t cap) {
    return fat_record_read(name, dst, cap);
}

int lippus_record_write(const char *name, const void *src, uint32_t len) {
    return fat_record_write(name, src, len);
}

void lippus_sleep(uint32_t ms) {
    while (ms) {
        uint32_t step = ms > 10u ? 10u : ms;
        sleep_ms(step);
        ms -= step;
        lippus_service();
    }
}

void lippus_exit(void) {
    if (s_in_game) {
        s_in_game = 0;
        longjmp(s_exit, 1);
    }
}

void lippus_service_reset(void) { s_combo_since = 0; }

void lippus_service(void) {
    if (!s_in_game) {
        return;
    }
    uint16_t raw = pad_raw();
    bool both = (raw & LIPPUS_START) && (raw & LIPPUS_SELECT);
    uint32_t now = lippus_ticks();
    if (!both) {
        s_combo_since = 0;
        return;
    }
    if (s_combo_since == 0) {
        s_combo_since = now | 1u;
    }
    if ((uint32_t)(now - (s_combo_since & ~1u)) > 700u) {
        lippus_exit();
    }
}

/* Il loader chiama questa funzione: setjmp deve vivere nello stesso modulo
   che esegue longjmp solo attraverso lippus_exit, ma il buffer va riempito
   qui perche' launch e' in un altro file. */
int lippus_enter_game(void (*entry)(const lippus_api_t *)) {
    lippus_service_reset();
    s_in_game = 1;
    if (setjmp(s_exit) == 0) {
        entry(lippus_api_table());
    }
    s_in_game = 0;
    lippus_service_reset();
    return 1;
}
