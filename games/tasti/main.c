#include "lippus.h"

typedef struct {
    int x;
    int y;
    int w;
    int h;
    uint16_t bit;
    const char *label;
} key_t;

static const key_t k_keys[] = {
    {78, 36, 36, 22, LIPPUS_UP, "SU"},
    {36, 64, 36, 22, LIPPUS_LEFT, "SX"},
    {78, 64, 36, 22, LIPPUS_CLICK, "C"},
    {120, 64, 36, 22, LIPPUS_RIGHT, "DX"},
    {78, 92, 36, 22, LIPPUS_DOWN, "GIU"},
    {196, 48, 40, 26, LIPPUS_Y, "Y"},
    {246, 48, 40, 26, LIPPUS_X, "X"},
    {196, 82, 40, 26, LIPPUS_B, "B"},
    {246, 82, 40, 26, LIPPUS_A, "A"},
    {36, 150, 84, 24, LIPPUS_SELECT, "SELECT"},
    {132, 150, 72, 24, LIPPUS_START, "START"},
};

static const lippus_note_t k_motivo[] = {
    {523, 90}, {659, 90}, {784, 90}, {1046, 140}, {784, 80}, {1046, 180},
};

void lippus_main(void) {
    int px = 250;
    int py = 196;
    const uint16_t sprite[8 * 8] = {
        LIPPUS_MAGENTA, LIPPUS_MAGENTA, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_MAGENTA, LIPPUS_MAGENTA,
        LIPPUS_MAGENTA, LIPPUS_AMBER, LIPPUS_WHITE, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_WHITE, LIPPUS_AMBER, LIPPUS_MAGENTA,
        LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER,
        LIPPUS_AMBER, LIPPUS_WHITE, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_WHITE, LIPPUS_AMBER,
        LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_RED, LIPPUS_RED, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER,
        LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_RED, LIPPUS_RED, LIPPUS_RED, LIPPUS_RED, LIPPUS_AMBER, LIPPUS_AMBER,
        LIPPUS_MAGENTA, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_RED, LIPPUS_RED, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_MAGENTA,
        LIPPUS_MAGENTA, LIPPUS_MAGENTA, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_AMBER, LIPPUS_MAGENTA, LIPPUS_MAGENTA,
    };

    for (;;) {
        lippus_pad_t pad = pad_poll();
        if (pad_held(&pad, LIPPUS_LEFT)) {
            px -= 2;
        }
        if (pad_held(&pad, LIPPUS_RIGHT)) {
            px += 2;
        }
        if (pad_held(&pad, LIPPUS_UP)) {
            py -= 2;
        }
        if (pad_held(&pad, LIPPUS_DOWN)) {
            py += 2;
        }
        if (px < 214) {
            px = 214;
        }
        if (py < 186) {
            py = 186;
        }
        if (px > 300) {
            px = 300;
        }
        if (py > 220) {
            py = 220;
        }

        static int holding;
        if (pad_pressed(&pad, LIPPUS_Y)) {
            snd_tune(0, SND_TRIANGLE, 200, k_motivo, 6);
            holding = 0;
        } else if (pad_held(&pad, LIPPUS_A)) {
            snd_voice(0, SND_SQUARE, 880, 220, 0);
            holding = 1;
        } else if (pad_held(&pad, LIPPUS_B)) {
            snd_voice(0, SND_TRIANGLE, 494, 220, 0);
            holding = 1;
        } else if (pad_held(&pad, LIPPUS_X)) {
            snd_voice(0, SND_NOISE, 5000, 180, 0);
            holding = 1;
        } else if (holding) {
            snd_stop();
            holding = 0;
        }

        gfx_clear(LIPPUS_RGB(12, 10, 28));
        gfx_text(12, 8, "TASTI", LIPPUS_AMBER, 2);
        gfx_text(120, 14, "pad, sprite, audio", LIPPUS_CREAM, 1);

        for (unsigned i = 0; i < sizeof k_keys / sizeof k_keys[0]; i++) {
            const key_t *key = &k_keys[i];
            int down = pad_held(&pad, key->bit);
            if (down) {
                gfx_fill_rect(key->x, key->y, key->w, key->h, LIPPUS_AMBER);
            } else {
                gfx_rect(key->x, key->y, key->w, key->h, LIPPUS_CREAM);
            }
            int scale = 1;
            int tw = gfx_text_width(key->label, scale);
            gfx_text(key->x + (key->w - tw) / 2, key->y + (key->h - 8) / 2, key->label,
                     down ? LIPPUS_RGB(32, 18, 36) : LIPPUS_CREAM, scale);
        }

        gfx_rect(208, 180, 100, 48, LIPPUS_RGB(90, 70, 130));
        gfx_blit(px, py, 8, 8, sprite, 1, LIPPUS_COLOR_KEY);
        gfx_text(12, 196, "A quadra  B tri  X rumore  Y motivo", LIPPUS_CREAM, 1);
        gfx_text(12, 214, "START+SELECT esce", LIPPUS_RGB(168, 156, 190), 1);
        gfx_present();
        sys_frame(33);
    }
}
