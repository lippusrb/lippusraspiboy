#include "os.h"

#include "board.h"
#include "font.h"
#include "lippus/gfx.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

static uint16_t fb[LIPPUS_SCREEN_W * LIPPUS_SCREEN_H] __attribute__((aligned(4)));

static int screen_w = LIPPUS_SCREEN_W;
static int screen_h = LIPPUS_SCREEN_H;
static uint8_t panel_madctl = LIPPUS_LCD_MADCTL_LANDSCAPE;
static int panel_ready;
static int clip_x;
static int clip_y;
static int clip_w;
static int clip_h;
static int dma_ch;
static bool dma_ready;
static volatile int present_busy;
static volatile int present_go;
static volatile int worker_ready;
static volatile int frame_w;
static volatile int frame_h;

static void present_core(void);

/* Il disegno aspetta la fine dell'invio: un solo framebuffer. */
static void fb_wait(void) {
    while (present_busy) {
        __wfe();
    }
}

static uint16_t swap16(uint16_t color) {
    return (uint16_t)((color << 8) | (color >> 8));
}

static void clip_full(void) {
    clip_x = 0;
    clip_y = 0;
    clip_w = screen_w;
    clip_h = screen_h;
}

static void cs_low(void) { gpio_put(LIPPUS_PIN_LCD_CS, 0); }
static void cs_high(void) { gpio_put(LIPPUS_PIN_LCD_CS, 1); }
static void dc_cmd(void) { gpio_put(LIPPUS_PIN_LCD_DC, 0); }
static void dc_data(void) { gpio_put(LIPPUS_PIN_LCD_DC, 1); }

static void lcd_write(uint8_t cmd, const uint8_t *data, size_t len) {
    cs_low();
    dc_cmd();
    spi_write_blocking(spi0, &cmd, 1);
    if (len) {
        dc_data();
        spi_write_blocking(spi0, data, len);
    }
    cs_high();
}

static void lcd_cmd(uint8_t cmd) { lcd_write(cmd, 0, 0); }

static void lcd_window(int x, int y, int w, int h) {
    int x0 = x + LIPPUS_LCD_X_OFFSET;
    int y0 = y + LIPPUS_LCD_Y_OFFSET;
    int x1 = x0 + w - 1;
    int y1 = y0 + h - 1;
    uint8_t col[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t row[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_write(0x2A, col, 4);
    lcd_write(0x2B, row, 4);
}

static void lcd_init_panel(void) {
    static const uint8_t gamma_p[15] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                                         0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
    static const uint8_t gamma_n[15] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                                         0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
    static const uint8_t pixfmt[] = {0x55};
    static const uint8_t pwc1[] = {0x23};
    static const uint8_t pwc2[] = {0x10};
    static const uint8_t vcm1[] = {0x3E, 0x28};
    static const uint8_t vcm2[] = {0x86};
    static const uint8_t frm[] = {0x00, 0x18};
    static const uint8_t dfn[] = {0x08, 0x82, 0x27};
    static const uint8_t zero[] = {0x00};
    static const uint8_t gamma_on[] = {0x01};

    gpio_put(LIPPUS_PIN_LCD_RST, 1);
    sleep_ms(5);
    gpio_put(LIPPUS_PIN_LCD_RST, 0);
    sleep_ms(20);
    gpio_put(LIPPUS_PIN_LCD_RST, 1);
    sleep_ms(120);

    lcd_cmd(0x01);
    sleep_ms(150);
    lcd_write(0xC0, pwc1, 1);
    lcd_write(0xC1, pwc2, 1);
    lcd_write(0xC5, vcm1, 2);
    lcd_write(0xC7, vcm2, 1);
    lcd_write(0x36, &panel_madctl, 1);
    lcd_write(0x3A, pixfmt, 1);
    lcd_write(0xB1, frm, 2);
    lcd_write(0xB6, dfn, 3);
    lcd_write(0xF2, zero, 1);
    lcd_write(0x26, gamma_on, 1);
    lcd_write(0xE0, gamma_p, 15);
    lcd_write(0xE1, gamma_n, 15);
    lcd_cmd(0x11);
    sleep_ms(120);
    if (LIPPUS_LCD_INVERT) {
        lcd_cmd(0x21);
    }
    lcd_cmd(0x29);
    sleep_ms(20);
    panel_ready = 1;
}

/* 12 mA e fronti rapidi servono a 62.5 MHz sulle piste corte. Sulla
   breadboard quegli stessi fronti si accoppiano sul reset del pannello:
   il primo quadro si vede, poi il controllore si azzera e resta bianco. */
static void drive_lcd(uint pin) {
    if (LIPPUS_LCD_BAUD >= 40000000u) {
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
    } else {
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
    }
}

void gfx_init(void) {
    clip_full();
    gpio_init(LIPPUS_PIN_LCD_CS);
    gpio_set_dir(LIPPUS_PIN_LCD_CS, GPIO_OUT);
    gpio_put(LIPPUS_PIN_LCD_CS, 1);
    gpio_init(LIPPUS_PIN_LCD_DC);
    gpio_set_dir(LIPPUS_PIN_LCD_DC, GPIO_OUT);
    gpio_init(LIPPUS_PIN_LCD_RST);
    gpio_set_dir(LIPPUS_PIN_LCD_RST, GPIO_OUT);
    gpio_put(LIPPUS_PIN_LCD_RST, 1);

    spi_init(spi0, LIPPUS_LCD_BAUD);
    gpio_set_function(LIPPUS_PIN_LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LIPPUS_PIN_LCD_MOSI, GPIO_FUNC_SPI);
    drive_lcd(LIPPUS_PIN_LCD_SCK);
    drive_lcd(LIPPUS_PIN_LCD_MOSI);

    dma_ch = dma_claim_unused_channel(true);
    dma_ready = true;
    lcd_init_panel();
    lippus_gfx_clear(LIPPUS_BLACK);
    lippus_gfx_present();
    multicore_launch_core1(present_core);
    while (!worker_ready) {
        __wfe();
    }
}

static bool intersect(int *x, int *y, int *w, int *h) {
    if (*w <= 0 || *h <= 0 || *w > 4096 || *h > 4096) {
        return false;
    }
    if (*x < -4096 || *y < -4096 || *x > 4096 || *y > 4096) {
        return false;
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
        return false;
    }
    *x = x0;
    *y = y0;
    *w = x1 - x0;
    *h = y1 - y0;
    return true;
}

void lippus_gfx_clear(uint16_t color) {
    fb_wait();
    uint16_t swapped = swap16(color);
    uint32_t pair = (uint32_t)swapped | ((uint32_t)swapped << 16);
    uint32_t *dst = (uint32_t *)fb;
    uint32_t words = (uint32_t)(screen_w * screen_h) / 2u;
    for (uint32_t i = 0; i < words; i++) {
        dst[i] = pair;
    }
}

void lippus_gfx_pixel(int x, int y, uint16_t color) {
    if ((unsigned)x >= (unsigned)screen_w || (unsigned)y >= (unsigned)screen_h) {
        return;
    }
    if (x < clip_x || y < clip_y || x >= clip_x + clip_w || y >= clip_y + clip_h) {
        return;
    }
    fb_wait();
    fb[(size_t)y * (size_t)screen_w + (size_t)x] = swap16(color);
}

void lippus_gfx_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (!intersect(&x, &y, &w, &h)) {
        return;
    }
    fb_wait();
    uint16_t swapped = swap16(color);
    uint32_t pair = (uint32_t)swapped | ((uint32_t)swapped << 16);
    for (int row = 0; row < h; row++) {
        uint16_t *line = fb + (size_t)(y + row) * (size_t)screen_w + (size_t)x;
        int left = w;
        if (((uintptr_t)line & 3u) && left) {
            *line++ = swapped;
            left--;
        }
        uint32_t *wide = (uint32_t *)line;
        while (left >= 2) {
            *wide++ = pair;
            left -= 2;
        }
        if (left) {
            *(uint16_t *)wide = swapped;
        }
    }
}

void lippus_gfx_rect(int x, int y, int w, int h, uint16_t color) {
    if (w < 1 || h < 1) {
        return;
    }
    lippus_gfx_fill_rect(x, y, w, 1, color);
    if (h > 1) {
        lippus_gfx_fill_rect(x, y + h - 1, w, 1, color);
    }
    if (h > 2) {
        lippus_gfx_fill_rect(x, y + 1, 1, h - 2, color);
        if (w > 1) {
            lippus_gfx_fill_rect(x + w - 1, y + 1, 1, h - 2, color);
        }
    }
}

void lippus_gfx_line(int x0, int y0, int x1, int y1, uint16_t color) {
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
        lippus_gfx_pixel(x0, y0, color);
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

void lippus_gfx_circle(int cx, int cy, int r, uint16_t color) {
    if (r < 0) {
        return;
    }
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (y <= x) {
        lippus_gfx_pixel(cx + x, cy + y, color);
        lippus_gfx_pixel(cx - x, cy + y, color);
        lippus_gfx_pixel(cx + x, cy - y, color);
        lippus_gfx_pixel(cx - x, cy - y, color);
        lippus_gfx_pixel(cx + y, cy + x, color);
        lippus_gfx_pixel(cx - y, cy + x, color);
        lippus_gfx_pixel(cx + y, cy - x, color);
        lippus_gfx_pixel(cx - y, cy - x, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void lippus_gfx_fill_circle(int cx, int cy, int r, uint16_t color) {
    if (r < 0) {
        return;
    }
    for (int dy = -r; dy <= r; dy++) {
        int span = r * r - dy * dy;
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= span) {
            dx++;
        }
        lippus_gfx_fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color);
    }
}

void lippus_gfx_blit(int x, int y, int w, int h, const uint16_t *pixels, int use_key,
                     uint16_t key) {
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
            lippus_gfx_pixel(x + col, y + row, pixel);
        }
    }
}

void lippus_gfx_text(int x, int y, const char *text, uint16_t color, int scale) {
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
                        lippus_gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
        x += 8 * scale;
    }
}

int lippus_gfx_text_width(const char *text, int scale) {
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

void lippus_gfx_clip(int x, int y, int w, int h) {
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

static void send_frame(int w, int h) {
    lcd_window(0, 0, w, h);
    while (spi_is_busy(spi0)) {
    }
    cs_low();
    dc_cmd();
    uint8_t cmd = 0x2C;
    spi_write_blocking(spi0, &cmd, 1);
    while (spi_is_busy(spi0)) {
    }
    dc_data();

    dma_channel_config config = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_dreq(&config, spi_get_dreq(spi0, true));
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    dma_channel_configure(dma_ch, &config, &spi_get_hw(spi0)->dr, fb,
                          (uint32_t)w * (uint32_t)h * 2u, true);
    dma_channel_wait_for_finish_blocking(dma_ch);
    while (spi_is_busy(spi0)) {
    }
    cs_high();
}

/* Core 1: solo l'invio del quadro. Gli interrupt restano accesi per il lockout della flash. */
static void present_core(void) {
    multicore_lockout_victim_init();
    worker_ready = 1;
    __sev();
    for (;;) {
        while (!present_go) {
            __wfe();
        }
        __dmb();
        int w = frame_w;
        int h = frame_h;
        present_go = 0;
        send_frame(w, h);
        __dmb();
        present_busy = 0;
        __sev();
    }
}

void lippus_gfx_present(void) {
    lippus_service();
    if (!dma_ready) {
        return;
    }
    fb_wait();
    if (!worker_ready) {
        send_frame(screen_w, screen_h);
        return;
    }
    frame_w = screen_w;
    frame_h = screen_h;
    __dmb();
    present_busy = 1;
    __dmb();
    present_go = 1;
    __sev();
}

void gfx_pause_for_flash(void) {
    if (!worker_ready) {
        return;
    }
    while (present_busy) {
        __wfe();
    }
    multicore_lockout_start_blocking();
}

void gfx_resume_after_flash(void) {
    if (!worker_ready) {
        return;
    }
    multicore_lockout_end_blocking();
}

int lippus_gfx_width(void) { return screen_w; }

int lippus_gfx_height(void) { return screen_h; }

void lippus_gfx_orientation(unsigned mode) {
    int portrait = mode == LIPPUS_PORTRAIT;
    int want_w = portrait ? LIPPUS_SCREEN_H : LIPPUS_SCREEN_W;
    int want_h = portrait ? LIPPUS_SCREEN_W : LIPPUS_SCREEN_H;
    if (want_w == screen_w && want_h == screen_h) {
        return;
    }
    fb_wait();
    screen_w = want_w;
    screen_h = want_h;
    panel_madctl = portrait ? LIPPUS_LCD_MADCTL_PORTRAIT : LIPPUS_LCD_MADCTL_LANDSCAPE;
    clip_full();
    lippus_api_set_size((uint16_t)screen_w, (uint16_t)screen_h);
    if (!panel_ready) {
        return;
    }
    lcd_write(0x36, &panel_madctl, 1);
    lippus_gfx_clear(LIPPUS_BLACK);
    lippus_gfx_present();
}
