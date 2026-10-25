#include "lippus.h"

const lippus_api_t *g_lippus_api;

/* Ancora .data: tiene viva la sezione e verifica che la copia sia riuscita. */
volatile uint32_t g_lippus_data_anchor = 0x4C5047u;

extern uint8_t __heap_start[];
extern uint8_t __heap_end[];
extern void lippus_main(void);
extern const uint32_t lippus_orientation;

static uint8_t *s_bump;
static uint32_t s_frame_next;

void lippus_runtime_start(void) {
    s_bump = __heap_start;
    s_frame_next = 0;
    /* La lettura tiene il simbolo nel link: il packer ne copia il valore. */
    if (!g_lippus_api || g_lippus_api->version != LIPPUS_API_VERSION ||
        g_lippus_data_anchor != 0x4C5047u || lippus_orientation > LIPPUS_PORTRAIT) {
        if (g_lippus_api && g_lippus_api->exit_to_menu) {
            g_lippus_api->exit_to_menu();
        }
        return;
    }
    lippus_main();
}

void *sys_alloc(size_t bytes) {
    if (!s_bump) {
        s_bump = __heap_start;
    }
    bytes = (bytes + 3u) & ~3u;
    if (s_bump + bytes > __heap_end) {
        return 0;
    }
    void *block = s_bump;
    s_bump += bytes;
    return block;
}

void sys_alloc_reset(void) { s_bump = __heap_start; }

void sys_frame(uint32_t frame_ms) {
    uint32_t now = sys_ticks();
    if (s_frame_next == 0) {
        s_frame_next = now + frame_ms;
        return;
    }
    if ((int32_t)(now - s_frame_next) < 0) {
        sys_sleep(s_frame_next - now);
        now = s_frame_next;
    } else if ((int32_t)(now - s_frame_next) > (int32_t)(frame_ms * 2u)) {
        now = sys_ticks();
    }
    s_frame_next = now + frame_ms;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d == s || n == 0) {
        return dst;
    }
    if (d < s) {
        return memcpy(dst, src, n);
    }
    d += n;
    s += n;
    while (n--) {
        *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int value, size_t n) {
    uint8_t *d = dst;
    uint8_t v = (uint8_t)value;
    while (n--) {
        *d++ = v;
    }
    return dst;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

void __aeabi_memcpy(void *dst, const void *src, size_t n) { memcpy(dst, src, n); }
void __aeabi_memcpy4(void *dst, const void *src, size_t n) { memcpy(dst, src, n); }
void __aeabi_memcpy8(void *dst, const void *src, size_t n) { memcpy(dst, src, n); }
void __aeabi_memmove(void *dst, const void *src, size_t n) { memmove(dst, src, n); }
void __aeabi_memset(void *dst, size_t n, int value) { memset(dst, value, n); }
void __aeabi_memclr(void *dst, size_t n) { memset(dst, 0, n); }
void __aeabi_memclr4(void *dst, size_t n) { memset(dst, 0, n); }
void __aeabi_memclr8(void *dst, size_t n) { memset(dst, 0, n); }
