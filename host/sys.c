#include "host.h"

#include "lippus/layout.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static lippus_api_t api;
const lippus_api_t *g_lippus_api;

static uint8_t heap[72 * 1024];
static uint8_t *bump;
static uint32_t frame_next;
static uint32_t combo_since;

static void service(void) {
    if (host_quit_requested()) {
        host_quit();
    }
    uint16_t held = host_keys_held();
    int both = (held & LIPPUS_START) && (held & LIPPUS_SELECT);
    uint32_t now = host_ticks();
    if (!both) {
        combo_since = 0;
        return;
    }
    if (combo_since == 0) {
        combo_since = now | 1u;
    }
    if ((uint32_t)(now - (combo_since & ~1u)) > 700u) {
        host_quit();
    }
}

static void api_present(void) {
    service();
    host_pump();
    host_window_present();
}

static void api_pad(lippus_pad_t *out) {
    service();
    host_keys_read(out);
}

static void api_sleep(uint32_t ms) { host_sleep(ms); }

void host_sleep(uint32_t ms) {
    uint32_t start = host_ticks();
    while ((uint32_t)(host_ticks() - start) < ms) {
        service();
        host_pump();
        usleep(2000);
    }
}

void *sys_alloc(size_t bytes) {
    if (!bump) {
        bump = heap;
    }
    bytes = (bytes + 3u) & ~3u;
    if (bump + bytes > heap + sizeof heap) {
        return 0;
    }
    void *block = bump;
    bump += bytes;
    return block;
}

void sys_alloc_reset(void) { bump = heap; }

void sys_frame(uint32_t frame_ms) {
    uint32_t now = host_ticks();
    if (frame_next == 0) {
        frame_next = now + frame_ms;
        return;
    }
    if ((int32_t)(now - frame_next) < 0) {
        host_sleep(frame_next - now);
        now = frame_next;
    } else if ((int32_t)(now - frame_next) > (int32_t)(frame_ms * 2u)) {
        now = host_ticks();
    }
    frame_next = now + frame_ms;
}

static int record_sfn(const char *name, char *out) {
    int n = 0;
    if (!name) {
        return 0;
    }
    for (const char *p = name; *p; p++) {
        char ch = *p;
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 32);
        }
        int ok = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
        if (!ok || n >= 8) {
            return 0;
        }
        out[n++] = ch;
    }
    out[n] = 0;
    return n > 0;
}

static int host_record_read(const char *name, void *dst, uint32_t cap) {
    char base[9];
    char path[32];
    FILE *file;
    size_t got;
    if (!record_sfn(name, base) || !dst) {
        return -1;
    }
    memcpy(path, "record/", 7);
    memcpy(path + 7, base, strlen(base) + 1);
    file = fopen(path, "rb");
    if (!file) {
        return 0;
    }
    got = fread(dst, 1, cap, file);
    if (ferror(file)) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return (int)got;
}

static int host_record_write(const char *name, const void *src, uint32_t len) {
    char base[9];
    char path[32];
    FILE *file;
    size_t got;
    if (!record_sfn(name, base) || (len > 0 && !src)) {
        return -1;
    }
    if (mkdir("record", 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    memcpy(path, "record/", 7);
    memcpy(path + 7, base, strlen(base) + 1);
    file = fopen(path, "wb");
    if (!file) {
        return -1;
    }
    got = len ? fwrite(src, 1, len, file) : 0;
    if (ferror(file) || got != len) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

void host_bind_api(int width, int height) {
    api.version = LIPPUS_API_VERSION;
    api.width = (uint16_t)width;
    api.height = (uint16_t)height;
    api.clear = host_gfx_clear;
    api.pixel = host_gfx_pixel;
    api.fill_rect = host_gfx_fill_rect;
    api.rect = host_gfx_rect;
    api.line = host_gfx_line;
    api.circle = host_gfx_circle;
    api.fill_circle = host_gfx_fill_circle;
    api.blit = host_gfx_blit;
    api.text = host_gfx_text;
    api.text_width = host_gfx_text_width;
    api.clip = host_gfx_clip;
    api.present = api_present;
    api.pad = api_pad;
    api.ticks = host_ticks;
    api.sleep_ms = api_sleep;
    api.tone = host_tone;
    api.exit_to_menu = host_quit;
    api.sound = host_sound;
    api.tune = host_tune;
    api.record_read = host_record_read;
    api.record_write = host_record_write;
    g_lippus_api = &api;
    host_gfx_init(width, height);
}
