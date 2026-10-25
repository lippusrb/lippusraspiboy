#include "host.h"

#include "font.h"

#include <string.h>

#define FB_MAX (320 * 320)

static uint16_t fb[FB_MAX];
static int screen_w = 320;
static int screen_h = 240;
static int clip_x;
static int clip_y;
static int clip_w = 320;
static int clip_h = 240;

static void clip_full(void) {
    clip_x = 0;
    clip_y = 0;
    clip_w = screen_w;
    clip_h = screen_h;
}

void host_gfx_init(int width, int height) {
    screen_w = width;
    screen_h = height;
    clip_full();
    memset(fb, 0, sizeof fb);
}

const uint16_t *host_fb(void) { return fb; }
int host_fb_width(void) { return screen_w; }
int host_fb_height(void) { return screen_h; }

static int intersect(int *x, int *y, int *w, int *h) {
    if (*w <= 0 || *h <= 0 || *w > 4096 || *h > 4096) {
        return 0;
    }
    if (*x < -4096 || *y < -4096 || *x > 4096 || *y > 4096) {
        return 0;
    }
    int x0 = *x;
    int y0 = *y;
    int x1 = x0 + *w;
    int y1 = y0 + *h;
    int cx1 = clip_x + clip_w;
    int cy1 = clip_y + clip_h;
    if (x0 < clip_x) {
        x0 = clip_x;
    }
    if (y0 < clip_y) {
        y0 = clip_y;
    }
    if (x1 > cx1) {
        x1 = cx1;
    }
    if (y1 > cy1) {
        y1 = cy1;
    }
    if (x0 >= x1 || y0 >= y1) {
        return 0;
    }
    *x = x0;
    *y = y0;
    *w = x1 - x0;
    *h = y1 - y0;
    return 1;
}

void host_gfx_clear(uint16_t color) {
    int n = screen_w * screen_h;
    for (int i = 0; i < n; i++) {
        fb[i] = color;
    }
}

void host_gfx_pixel(int x, int y, uint16_t color) {
    if ((unsigned)x >= (unsigned)screen_w || (unsigned)y >= (unsigned)screen_h) {
        return;
    }
    if (x < clip_x || y < clip_y || x >= clip_x + clip_w || y >= clip_y + clip_h) {
        return;
    }
    fb[(size_t)y * (size_t)screen_w + (size_t)x] = color;
}

void host_gfx_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (!intersect(&x, &y, &w, &h)) {
        return;
    }
    for (int row = 0; row < h; row++) {
        uint16_t *line = fb + (size_t)(y + row) * (size_t)screen_w + (size_t)x;
        for (int col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

void host_gfx_rect(int x, int y, int w, int h, uint16_t color) {
    if (w < 1 || h < 1) {
        return;
    }
    host_gfx_fill_rect(x, y, w, 1, color);
    if (h > 1) {
        host_gfx_fill_rect(x, y + h - 1, w, 1, color);
    }
    if (h > 2) {
        host_gfx_fill_rect(x, y + 1, 1, h - 2, color);
        if (w > 1) {
            host_gfx_fill_rect(x + w - 1, y + 1, 1, h - 2, color);
        }
    }
}

void host_gfx_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    int err = dx - dy;
    for (;;) {
        host_gfx_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int doubled = err * 2;
        if (doubled > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (doubled < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void host_gfx_circle(int cx, int cy, int r, uint16_t color) {
    if (r < 0) {
        return;
    }
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (y <= x) {
        host_gfx_pixel(cx + x, cy + y, color);
        host_gfx_pixel(cx - x, cy + y, color);
        host_gfx_pixel(cx + x, cy - y, color);
        host_gfx_pixel(cx - x, cy - y, color);
        host_gfx_pixel(cx + y, cy + x, color);
        host_gfx_pixel(cx - y, cy + x, color);
        host_gfx_pixel(cx + y, cy - x, color);
        host_gfx_pixel(cx - y, cy - x, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void host_gfx_fill_circle(int cx, int cy, int r, uint16_t color) {
    if (r < 0) {
        return;
    }
    for (int dy = -r; dy <= r; dy++) {
        int span = r * r - dy * dy;
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= span) {
            dx++;
        }
        host_gfx_fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color);
    }
}

void host_gfx_blit(int x, int y, int w, int h, const uint16_t *pixels, int use_key, uint16_t key) {
    if (!pixels || w <= 0 || h <= 0 || w > 1024 || h > 1024) {
        return;
    }
    for (int row = 0; row < h; row++) {
        const uint16_t *src = pixels + (size_t)row * (size_t)w;
        for (int col = 0; col < w; col++) {
            uint16_t pixel = src[col];
            if (use_key && pixel == key) {
                continue;
            }
            host_gfx_pixel(x + col, y + row, pixel);
        }
    }
}

void host_gfx_text(int x, int y, const char *text, uint16_t color, int scale) {
    if (!text || scale < 1) {
        return;
    }
    if (scale > 8) {
        scale = 8;
    }
    int origin = x;
    for (int i = 0; text[i]; i++) {
        unsigned char code = (unsigned char)text[i];
        if (code == '\n') {
            x = origin;
            y += 8 * scale;
            continue;
        }
        const uint8_t *glyph = font_glyph(code);
        if (glyph) {
            for (int row = 0; row < 8; row++) {
                uint8_t bits = glyph[row];
                for (int col = 0; col < 8; col++) {
                    if (bits & (uint8_t)(1u << col)) {
                        host_gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
        x += 8 * scale;
    }
}

int host_gfx_text_width(const char *text, int scale) {
    if (!text) {
        return 0;
    }
    if (scale < 1) {
        scale = 1;
    }
    int count = 0;
    while (text[count] && text[count] != '\n') {
        count++;
    }
    return count * 8 * scale;
}

void host_gfx_clip(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        clip_full();
        return;
    }
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > screen_w) {
        x1 = screen_w;
    }
    if (y1 > screen_h) {
        y1 = screen_h;
    }
    if (x0 >= x1 || y0 >= y1) {
        clip_x = 0;
        clip_y = 0;
        clip_w = 0;
        clip_h = 0;
        return;
    }
    clip_x = x0;
    clip_y = y0;
    clip_w = x1 - x0;
    clip_h = y1 - y0;
}
