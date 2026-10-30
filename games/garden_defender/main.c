#include "lippus.h"
#include "sprites.h"

/* Orizzontale: il giardiniere scorre sulla base, lo stick non si gira.
   Sciame a griglia, vasi pixel-art che si sgretolano, farfalla in alto. */

#define BUG_COLS 8
#define BUG_ROWS 5
#define BUG_N (BUG_COLS * BUG_ROWS)
#define PITCH_X 28
#define PITCH_Y 18

#define POTS 3
#define POT_W POT_SPR_W
#define POT_H POT_SPR_H
#define POT_Y 168
#define PLAYER_Y 206

#define DROPS 3

#define ST_READY 0
#define ST_PLAY 1
#define ST_DEAD 2
#define ST_WAVE 3
#define ST_OVER 4

static int g_w;
static int g_h;
static int g_form_x;
static int g_form_y;
static int g_dir;
static int g_march;
static int g_anim;
static uint8_t g_alive[BUG_ROWS][BUG_COLS];
static int g_alive_n;
static uint8_t g_pot[POTS][POT_H][POT_W];
static int g_px;
static int g_shot_on;
static int g_shot_x;
static int g_shot_y;
static int g_drop_on[DROPS];
static int g_drop_x[DROPS];
static int g_drop_y[DROPS];
static int g_fly_on;
static int g_fly_x;
static int g_fly_dir;
static int g_fly_wait;
static int g_score;
static int g_lives;
static int g_level;
static int g_state;
static int g_timer;
static int g_hurt;
static int g_overrun;
static int g_frame;
static uint32_t g_rng = 0xA5F1C3u;

static unsigned rnd(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 16;
}

static int overlap(int x, int y, int w, int h, int x2, int y2, int w2, int h2) {
    return x < x2 + w2 && x + w > x2 && y < y2 + h2 && y + h > y2;
}

static int pot_x(int index) {
    int gap = (g_w - POTS * POT_W) / (POTS + 1);
    return gap + index * (POT_W + gap);
}

static void chip_pot(int index, int x, int y) {
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int xx = x + dx;
            int yy = y + dy;
            if ((unsigned)xx < POT_W && (unsigned)yy < POT_H) {
                g_pot[index][yy][xx] = 0;
            }
        }
    }
}

static int pot_at(int x, int y) {
    if (y < POT_Y || y >= POT_Y + POT_H) {
        return -1;
    }
    for (int i = 0; i < POTS; i++) {
        int lx = x - pot_x(i);
        int ly = y - POT_Y;
        if ((unsigned)lx >= POT_W || (unsigned)ly >= POT_H) {
            continue;
        }
        if (g_pot[i][ly][lx]) {
            return i;
        }
    }
    return -1;
}

static int chip_rect(int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            int pot = pot_at(xx, yy);
            if (pot >= 0) {
                chip_pot(pot, xx - pot_x(pot), yy - POT_Y);
                return 1;
            }
        }
    }
    return 0;
}

static void stamp_pots(void) {
    for (int p = 0; p < POTS; p++) {
        const uint16_t *spr = k_vasi[p];
        for (int y = 0; y < POT_H; y++) {
            for (int x = 0; x < POT_W; x++) {
                g_pot[p][y][x] = (uint8_t)(spr[y * POT_W + x] != LIPPUS_COLOR_KEY);
            }
        }
    }
}

static void bug_box(int row, int col, int *x, int *y, int *w, int *h) {
    *x = g_form_x + col * PITCH_X;
    *y = g_form_y + row * PITCH_Y;
    if (row == 0) {
        *w = BUG8_W;
        *h = BUG8_H;
    } else {
        *w = BUG16_W;
        *h = BUG16_H;
    }
    *x += (PITCH_X - *w) / 2;
}

static int bug_points(int row) {
    if (row == 0) {
        return 30;
    }
    if (row < 3) {
        return 20;
    }
    return 10;
}

static void clear_shots(void) {
    g_shot_on = 0;
    for (int i = 0; i < DROPS; i++) {
        g_drop_on[i] = 0;
    }
}

static void reset_wave(void) {
    g_alive_n = 0;
    for (int r = 0; r < BUG_ROWS; r++) {
        for (int c = 0; c < BUG_COLS; c++) {
            g_alive[r][c] = 1;
            g_alive_n++;
        }
    }
    int span = (BUG_COLS - 1) * PITCH_X + 12;
    g_form_x = (g_w - span) / 2;
    g_form_y = 32 + (g_level - 1) * 6;
    if (g_form_y > 68) {
        g_form_y = 68;
    }
    g_dir = 1;
    g_march = 10;
    g_anim = 0;
    g_fly_on = 0;
    g_fly_wait = 160;
    clear_shots();
    stamp_pots();
    g_px = g_w / 2 - 8;
}

static void new_game(void) {
    g_score = 0;
    g_lives = 3;
    g_level = 1;
    g_hurt = 0;
    g_overrun = 0;
    reset_wave();
    g_state = ST_READY;
    g_timer = 40;
}

static void format_u(char *dst, unsigned value) {
    char tmp[8];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && n < 8);
    int used = 0;
    while (n > 0) {
        dst[used++] = tmp[--n];
    }
    dst[used] = 0;
}

static void lose_life(int overrun) {
    g_lives--;
    g_overrun = overrun;
    clear_shots();
    g_hurt = 50;
    g_state = ST_DEAD;
    g_timer = 36;
    snd_voice(0, SND_NOISE, 180, 150, 140);
}

static void step_formation(void) {
    int left = g_w;
    int right = 0;
    int any = 0;
    for (int r = 0; r < BUG_ROWS; r++) {
        for (int c = 0; c < BUG_COLS; c++) {
            int x;
            int y;
            int w;
            int h;
            if (!g_alive[r][c]) {
                continue;
            }
            bug_box(r, c, &x, &y, &w, &h);
            if (x < left) {
                left = x;
            }
            if (x + w > right) {
                right = x + w;
            }
            any = 1;
        }
    }
    if (!any) {
        return;
    }
    int delta = g_dir * 2;
    if (left + delta < 2 || right + delta > g_w - 2) {
        g_dir = -g_dir;
        g_form_y += 8;
    } else {
        g_form_x += delta;
    }
    g_anim ^= 1;
}

static void spawn_drop(void) {
    int slot = -1;
    for (int i = 0; i < DROPS; i++) {
        if (!g_drop_on[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0 || g_alive_n <= 0) {
        return;
    }
    int pick = (int)(rnd() % (unsigned)g_alive_n);
    for (int r = 0; r < BUG_ROWS; r++) {
        for (int c = 0; c < BUG_COLS; c++) {
            int x;
            int y;
            int w;
            int h;
            if (!g_alive[r][c]) {
                continue;
            }
            if (pick > 0) {
                pick--;
                continue;
            }
            bug_box(r, c, &x, &y, &w, &h);
            g_drop_on[slot] = 1;
            g_drop_x[slot] = x + w / 2;
            g_drop_y[slot] = y + h;
            return;
        }
    }
}

static void kill_bug(int row, int col) {
    if (!g_alive[row][col]) {
        return;
    }
    g_alive[row][col] = 0;
    g_alive_n--;
    g_score += bug_points(row);
    snd_voice(1, SND_SQUARE, (unsigned)(520 + bug_points(row) * 8), 70, 30);
    if (g_alive_n == 0) {
        g_state = ST_WAVE;
        g_timer = 36;
        clear_shots();
        snd_voice(0, SND_TRIANGLE, 640, 100, 80);
    }
}

static void chew_pots(void) {
    if ((g_frame & 3) != 0) {
        return;
    }
    for (int r = 0; r < BUG_ROWS; r++) {
        for (int c = 0; c < BUG_COLS; c++) {
            int x;
            int y;
            int w;
            int h;
            if (!g_alive[r][c]) {
                continue;
            }
            bug_box(r, c, &x, &y, &w, &h);
            chip_rect(x + w / 2, y + h - 1, 1, 1);
        }
    }
}

static int lowest_bug(void) {
    int bottom = 0;
    for (int r = 0; r < BUG_ROWS; r++) {
        for (int c = 0; c < BUG_COLS; c++) {
            int x;
            int y;
            int w;
            int h;
            if (!g_alive[r][c]) {
                continue;
            }
            bug_box(r, c, &x, &y, &w, &h);
            if (y + h > bottom) {
                bottom = y + h;
            }
        }
    }
    return bottom;
}

static void update_play(const lippus_pad_t *pad) {
    if (pad_held(pad, LIPPUS_LEFT)) {
        g_px -= 3;
    }
    if (pad_held(pad, LIPPUS_RIGHT)) {
        g_px += 3;
    }
    if (g_px < 4) {
        g_px = 4;
    }
    if (g_px > g_w - 20) {
        g_px = g_w - 20;
    }
    if (!g_shot_on && (pad_held(pad, LIPPUS_A) || pad_held(pad, LIPPUS_UP))) {
        g_shot_on = 1;
        g_shot_x = g_px + 11;
        g_shot_y = PLAYER_Y - 6;
        snd_voice(0, SND_NOISE, 1600, 40, 24);
    }

    if (g_shot_on) {
        g_shot_y -= 4;
        if (g_shot_y < 12) {
            g_shot_on = 0;
        } else if (g_fly_on &&
                   overlap(g_shot_x, g_shot_y, 2, 8, g_fly_x, 16, BUG16_W, BUG16_H)) {
            g_shot_on = 0;
            g_fly_on = 0;
            g_score += 200;
            snd_voice(1, SND_TRIANGLE, 980, 110, 70);
        } else {
            int hit = 0;
            for (int r = 0; r < BUG_ROWS && !hit; r++) {
                for (int c = 0; c < BUG_COLS; c++) {
                    int x;
                    int y;
                    int w;
                    int h;
                    if (!g_alive[r][c]) {
                        continue;
                    }
                    bug_box(r, c, &x, &y, &w, &h);
                    if (overlap(g_shot_x, g_shot_y, 2, 8, x, y, w, h)) {
                        g_shot_on = 0;
                        kill_bug(r, c);
                        hit = 1;
                        break;
                    }
                }
            }
            if (!hit && g_shot_on && chip_rect(g_shot_x, g_shot_y, 2, 4)) {
                g_shot_on = 0;
                snd_voice(2, SND_NOISE, 240, 36, 20);
            }
        }
    }

    if (g_state != ST_PLAY) {
        return;
    }

    for (int i = 0; i < DROPS; i++) {
        if (!g_drop_on[i]) {
            continue;
        }
        g_drop_y[i] += 3;
        if (g_drop_y[i] > g_h - 8) {
            g_drop_on[i] = 0;
            continue;
        }
        if (g_hurt <= 0 && overlap(g_drop_x[i], g_drop_y[i], 2, 4, g_px, PLAYER_Y, 16, 12)) {
            g_drop_on[i] = 0;
            lose_life(0);
            return;
        }
        if (chip_rect(g_drop_x[i], g_drop_y[i] + 3, 2, 2)) {
            g_drop_on[i] = 0;
            snd_voice(2, SND_NOISE, 200, 28, 16);
        }
    }

    int every = 30 - g_level * 2;
    if (every < 8) {
        every = 8;
    }
    if ((g_frame % every) == 0) {
        spawn_drop();
    }

    if (--g_march <= 0) {
        int pace = 12 - g_level;
        pace -= (BUG_N - g_alive_n) / 5;
        if (pace < 2) {
            pace = 2;
        }
        g_march = pace;
        step_formation();
    }
    chew_pots();
    if (lowest_bug() >= PLAYER_Y) {
        lose_life(1);
        return;
    }

    if (!g_fly_on) {
        if (--g_fly_wait <= 0) {
            g_fly_on = 1;
            g_fly_dir = (rnd() & 1u) ? 1 : -1;
            g_fly_x = g_fly_dir > 0 ? -16 : g_w + 4;
            g_fly_wait = 180 + (int)(rnd() % 220u);
        }
    } else {
        g_fly_x += g_fly_dir * 4;
        if (g_fly_x < -24 || g_fly_x > g_w + 16) {
            g_fly_on = 0;
        }
    }
    if (g_hurt > 0) {
        g_hurt--;
    }
}

static void blit_key(int x, int y, int w, int h, const uint16_t *pixels) {
    gfx_blit(x, y, w, h, pixels, 1, LIPPUS_COLOR_KEY);
}

static void draw_aphid(int x, int y) {
    blit_key(x, y, BUG8_W, BUG8_H, k_afide);
}

static void draw_mosquito(int x, int y) {
    blit_key(x, y, BUG16_W, BUG16_H, k_zanzara);
}

static void draw_locust(int x, int y) {
    blit_key(x, y, BUG16_W, BUG16_H, k_cavalletta);
}

static void draw_butterfly(int x, int y) {
    blit_key(x, y, BUG16_W, BUG16_H, k_farfalla);
}

static void draw_pots(void) {
    for (int p = 0; p < POTS; p++) {
        int ox = pot_x(p);
        const uint16_t *spr = k_vasi[p];
        for (int y = 0; y < POT_H; y++) {
            for (int x = 0; x < POT_W; x++) {
                if (!g_pot[p][y][x]) {
                    continue;
                }
                uint16_t c = spr[y * POT_W + x];
                if (c == LIPPUS_COLOR_KEY) {
                    continue;
                }
                gfx_pixel(ox + x, POT_Y + y, c);
            }
        }
    }
}

static void draw_gardener(void) {
    if (g_hurt > 0 && (g_frame & 2)) {
        return;
    }
    int x = g_px;
    int y = PLAYER_Y;
    gfx_fill_rect(x + 2, y, 10, 2, LIPPUS_AMBER);
    gfx_fill_rect(x + 4, y + 2, 6, 3, LIPPUS_RGB(230, 180, 140));
    gfx_fill_rect(x + 3, y + 5, 8, 6, LIPPUS_RGB(40, 120, 60));
    gfx_fill_rect(x + 10, y + 6, 5, 5, LIPPUS_RGB(150, 156, 160));
    gfx_fill_rect(x + 11, y - 2, 2, 8, LIPPUS_RGB(90, 96, 100));
}

static void draw_world(void) {
    char score[8];
    char level[4];
    gfx_clear(LIPPUS_RGB(14, 32, 22));
    gfx_fill_rect(0, g_h - 14, g_w, 14, LIPPUS_RGB(34, 92, 40));
    draw_pots();
    for (int r = 0; r < BUG_ROWS; r++) {
        for (int c = 0; c < BUG_COLS; c++) {
            int x;
            int y;
            int w;
            int h;
            if (!g_alive[r][c]) {
                continue;
            }
            bug_box(r, c, &x, &y, &w, &h);
            if (r == 0) {
                draw_aphid(x, y);
            } else if (r < 3) {
                draw_mosquito(x, y);
            } else {
                draw_locust(x, y);
            }
        }
    }
    if (g_fly_on) {
        draw_butterfly(g_fly_x, 16);
    }
    for (int i = 0; i < DROPS; i++) {
        if (g_drop_on[i]) {
            gfx_fill_rect(g_drop_x[i], g_drop_y[i], 2, 4, LIPPUS_RGB(90, 140, 40));
        }
    }
    if (g_shot_on) {
        gfx_fill_rect(g_shot_x, g_shot_y, 2, 5, LIPPUS_RGB(190, 240, 255));
    }
    draw_gardener();
    format_u(score, (unsigned)g_score);
    format_u(level, (unsigned)g_level);
    gfx_text(6, 4, score, LIPPUS_CREAM, 1);
    gfx_text(g_w - 8 - gfx_text_width(level, 1), 4, level, LIPPUS_AMBER, 1);
    for (int i = 0; i < g_lives - 1 && i < 4; i++) {
        gfx_fill_rect(g_w / 2 - 20 + i * 12, 4, 8, 3, LIPPUS_AMBER);
    }
    if (g_state == ST_READY) {
        gfx_text((g_w - gfx_text_width("VIA", 2)) / 2, 100, "VIA", LIPPUS_YELLOW, 2);
    } else if (g_state == ST_OVER) {
        gfx_text((g_w - gfx_text_width("FINE", 2)) / 2, 96, "FINE", LIPPUS_RED, 2);
        gfx_text((g_w - gfx_text_width("A ANCORA", 1)) / 2, 120, "A ANCORA", LIPPUS_CREAM, 1);
    }
}

void lippus_main(void) {
    g_w = gfx_width();
    g_h = gfx_height();
    new_game();
    for (;;) {
        lippus_pad_t pad = pad_poll();
        g_frame++;
        if (g_state == ST_OVER) {
            draw_world();
            gfx_present();
            if (pad_pressed(&pad, LIPPUS_A) || pad_pressed(&pad, LIPPUS_START)) {
                new_game();
            }
            sys_frame(33);
            continue;
        }
        if (g_state == ST_READY) {
            if (--g_timer <= 0) {
                g_state = ST_PLAY;
            }
        } else if (g_state == ST_DEAD) {
            if (--g_timer <= 0) {
                if (g_lives <= 0) {
                    g_state = ST_OVER;
                } else if (g_overrun) {
                    reset_wave();
                    g_state = ST_READY;
                    g_timer = 30;
                } else {
                    g_state = ST_PLAY;
                }
            }
        } else if (g_state == ST_WAVE) {
            if (--g_timer <= 0) {
                g_level++;
                reset_wave();
                g_state = ST_READY;
                g_timer = 30;
            }
        } else {
            update_play(&pad);
        }
        draw_world();
        gfx_present();
        sys_frame(33);
    }
}
