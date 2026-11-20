#include "lippus.h"
#include "pixel_game_font.h"
#include "sprites.h"

/* Orizzontale: il giardiniere scorre sulla base, lo stick non si gira.
   Sciame a griglia, vasi pixel-art che si sgretolano, farfalla in alto.
   Anteprima e classifica come Maze Mouse: fasi illustrate, poi venti nomi. */

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
#define PLAYER_W PLAYER_SPR_W
#define PLAYER_H PLAYER_SPR_H
/* Il corpo della bomboletta sta in colonna 8..16 dello sprite 24x24. */
#define PLAYER_HIT_X 8
#define PLAYER_HIT_W 9
#define SHOT_W SHOT_SPR_W
#define SHOT_H SHOT_SPR_H

#define DROPS 3
#define HUD_H 16
#define FLY_Y 18
#define RECORD_NAME "GARDEN"
#define SCORE_N 20
#define ATTRACT_LEN 420
#define ATTRACT_SCORE_LEN 150

#define ST_ATTRACT 0
#define ST_READY 1
#define ST_PLAY 2
#define ST_DEAD 3
#define ST_WAVE 4
#define ST_OVER 5
#define ST_NAME 6

#define COL_LAWN LIPPUS_RGB(96, 154, 58)
#define COL_TURF LIPPUS_RGB(78, 138, 52)
#define COL_DROP LIPPUS_RGB(255, 255, 120)
#define COL_DROP_HI LIPPUS_RGB(255, 255, 230)

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
static int g_spray;
static uint32_t g_rng = 0xA5F1C3u;

typedef struct {
    char name[3];
    int score;
} score_t;

static score_t g_scores[SCORE_N];
static int g_score_n;
static int g_rank;
static int g_name_i;
static int g_letter_hold;
static int g_attract_page;
static int g_attract_t;
static char g_initials[3];
static char g_rec[LIPPUS_RECORD_MAX];

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
    g_form_y = 36 + (g_level - 1) * 6;
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
    g_px = g_w / 2 - PLAYER_W / 2;
    g_spray = 0;
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

static int pg_width(const char *text, int scale) {
    int width = 0;
    int i;
    if (!text) {
        return 0;
    }
    if (scale < 1) {
        scale = 1;
    }
    for (i = 0; text[i] && text[i] != '\n'; i++) {
        unsigned char code = (unsigned char)text[i];
        if (code < 32 || code > 126) {
            continue;
        }
        width += pg_font[code - 32].adv * scale;
    }
    return width;
}

static void pg_text(int x, int y, const char *text, uint16_t color, int scale) {
    int origin;
    int line;
    int i;
    if (!text || scale < 1) {
        return;
    }
    if (scale > 3) {
        scale = 3;
    }
    origin = x;
    line = y;
    for (i = 0; text[i]; i++) {
        unsigned char code = (unsigned char)text[i];
        const pg_glyph_t *glyph;
        int row;
        if (code == '\n') {
            x = origin;
            line += PG_LH * scale;
            continue;
        }
        if (code < 32 || code > 126) {
            continue;
        }
        glyph = &pg_font[code - 32];
        for (row = 0; row < glyph->h; row++) {
            uint32_t bits = pg_rows[glyph->row + row];
            int col;
            for (col = 0; col < glyph->w; col++) {
                if (bits & (1u << col)) {
                    gfx_fill_rect(x + (glyph->bearing + col) * scale,
                                  line + (glyph->top + row) * scale, scale, scale, color);
                }
            }
        }
        x += glyph->adv * scale;
    }
}

static void text_center(int y, const char *s, uint16_t color, int scale) {
    int x = (g_w - pg_width(s, scale)) / 2;
    pg_text(x, y, s, color, scale);
}

static int score_qualifies(int score) {
    if (score <= 0) {
        return 0;
    }
    if (g_score_n < SCORE_N) {
        return 1;
    }
    return score > g_scores[g_score_n - 1].score;
}

static void scores_load(void) {
    int got;
    int i = 0;
    g_score_n = 0;
    got = sys_record_read(RECORD_NAME, g_rec, LIPPUS_RECORD_MAX - 1);
    if (got <= 0) {
        return;
    }
    if (got > (int)LIPPUS_RECORD_MAX - 1) {
        got = (int)LIPPUS_RECORD_MAX - 1;
    }
    g_rec[got] = 0;
    while (g_rec[i] && g_score_n < SCORE_N) {
        int used = 0;
        int score;
        char a;
        char b;
        char c;
        if (g_rec[i] == '\n' || g_rec[i] == '\r') {
            i++;
            continue;
        }
        if (i + 3 >= got) {
            break;
        }
        a = g_rec[i];
        b = g_rec[i + 1];
        c = g_rec[i + 2];
        if (a < 'A' || a > 'Z' || b < 'A' || b > 'Z' || c < 'A' || c > 'Z' || g_rec[i + 3] != ' ') {
            while (g_rec[i] && g_rec[i] != '\n') {
                i++;
            }
            continue;
        }
        score = 0;
        i += 4;
        while (g_rec[i] >= '0' && g_rec[i] <= '9' && used < 9) {
            score = score * 10 + (g_rec[i] - '0');
            i++;
            used++;
        }
        if (used > 0 && score > 0) {
            g_scores[g_score_n].name[0] = a;
            g_scores[g_score_n].name[1] = b;
            g_scores[g_score_n].name[2] = c;
            g_scores[g_score_n].score = score;
            g_score_n++;
        }
        while (g_rec[i] && g_rec[i] != '\n') {
            i++;
        }
    }
}

static void scores_save(void) {
    int n = 0;
    int i;
    for (i = 0; i < g_score_n; i++) {
        char num[8];
        int k;
        if (n + 16 >= (int)LIPPUS_RECORD_MAX) {
            break;
        }
        g_rec[n++] = g_scores[i].name[0];
        g_rec[n++] = g_scores[i].name[1];
        g_rec[n++] = g_scores[i].name[2];
        g_rec[n++] = ' ';
        format_u(num, (unsigned)g_scores[i].score);
        for (k = 0; num[k]; k++) {
            g_rec[n++] = num[k];
        }
        g_rec[n++] = '\n';
    }
    sys_record_write(RECORD_NAME, g_rec, n);
}

static void score_insert(int score) {
    int at = g_score_n;
    int last;
    int i;
    for (i = 0; i < g_score_n; i++) {
        if (score > g_scores[i].score) {
            at = i;
            break;
        }
    }
    if (at >= SCORE_N) {
        g_rank = -1;
        return;
    }
    last = g_score_n < SCORE_N ? g_score_n : SCORE_N - 1;
    for (i = last; i > at; i--) {
        g_scores[i] = g_scores[i - 1];
    }
    g_scores[at].name[0] = g_initials[0];
    g_scores[at].name[1] = g_initials[1];
    g_scores[at].name[2] = g_initials[2];
    g_scores[at].score = score;
    if (g_score_n < SCORE_N) {
        g_score_n++;
    }
    g_rank = at;
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
    if (g_px < 0) {
        g_px = 0;
    }
    if (g_px > g_w - PLAYER_W) {
        g_px = g_w - PLAYER_W;
    }
    g_spray = pad_held(pad, LIPPUS_A) || pad_held(pad, LIPPUS_UP);
    if (!g_shot_on && g_spray) {
        g_shot_on = 1;
        g_shot_x = g_px + 12 - SHOT_W / 2;
        g_shot_y = PLAYER_Y - SHOT_H;
        snd_voice(0, SND_NOISE, 1600, 40, 24);
    }

    if (g_shot_on) {
        g_shot_y -= 4;
        if (g_shot_y + SHOT_H <= HUD_H) {
            g_shot_on = 0;
        } else if (g_fly_on &&
                   overlap(g_shot_x, g_shot_y, SHOT_W, SHOT_H, g_fly_x, FLY_Y, BUG16_W, BUG16_H)) {
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
                    if (overlap(g_shot_x, g_shot_y, SHOT_W, SHOT_H, x, y, w, h)) {
                        g_shot_on = 0;
                        kill_bug(r, c);
                        hit = 1;
                        break;
                    }
                }
            }
            if (!hit && g_shot_on && chip_rect(g_shot_x + 2, g_shot_y, 4, 4)) {
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
        if (g_hurt <= 0 && overlap(g_drop_x[i], g_drop_y[i], 2, 4, g_px + PLAYER_HIT_X, PLAYER_Y,
                                   PLAYER_HIT_W, PLAYER_H)) {
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

static int walk_frame(void) {
    return g_anim & 1;
}

static void draw_bug(int row, int x, int y) {
    int frame = walk_frame();
    if (row == 0) {
        blit_key(x, y, BUG8_W, BUG8_H, k_bug5[frame]);
        return;
    }
    if (row == 1) {
        blit_key(x, y, BUG16_W, BUG16_H, k_bug1[frame]);
        return;
    }
    if (row == 2) {
        blit_key(x, y, BUG16_W, BUG16_H, k_bug2[frame]);
        return;
    }
    if (row == 3) {
        blit_key(x, y, BUG16_W, BUG16_H, k_bug3[frame]);
        return;
    }
    blit_key(x, y, BUG16_W, BUG16_H, k_bug4[frame]);
}

static void draw_butterfly(int x, int y) {
    blit_key(x, y, BUG16_W, BUG16_H, k_farfalla[(g_frame / 4) & 1]);
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

static void draw_gardener_at(int x, int spray) {
    blit_key(x, PLAYER_Y, PLAYER_W, PLAYER_H, k_spray[spray ? 1 : 0]);
}

static void draw_gardener(void) {
    if (g_hurt > 0 && (g_frame & 2)) {
        return;
    }
    draw_gardener_at(g_px, g_spray);
}

static void draw_hud(void) {
    char score[8];
    char level[8];
    int i;
    int lives = g_lives;
    int lx;
    int drop_w;
    int right;
    format_u(score, (unsigned)g_score);
    format_u(level, (unsigned)g_level);
    gfx_fill_rect(0, 0, g_w, HUD_H, COL_TURF);
    pg_text(4, 2, "Points ", LIPPUS_AMBER, 1);
    pg_text(4 + pg_width("Points ", 1), 2, score, LIPPUS_CREAM, 1);
    right = g_w - 4;
    pg_text(right - pg_width(level, 1), 2, level, LIPPUS_CREAM, 1);
    pg_text(right - pg_width(level, 1) - pg_width("Level ", 1), 2, "Level ", LIPPUS_AMBER, 1);
    if (lives < 0) {
        lives = 0;
    }
    if (lives > 4) {
        lives = 4;
    }
    drop_w = lives * (SHOT_W + 4) - 4;
    lx = g_w / 2 - drop_w / 2;
    for (i = 0; i < lives; i++) {
        blit_key(lx + i * (SHOT_W + 4), 4, SHOT_W, SHOT_H, k_goccia);
    }
}

static void draw_lawn(void) {
    int ground_y = PLAYER_Y + PLAYER_H;
    gfx_clear(COL_LAWN);
    if (ground_y < g_h) {
        gfx_fill_rect(0, ground_y, g_w, g_h - ground_y, COL_TURF);
    }
}

static void draw_bug_shot(int x, int y) {
    gfx_fill_rect(x - 1, y, 4, 6, COL_DROP);
    gfx_fill_rect(x, y + 1, 2, 3, COL_DROP_HI);
}

static void draw_world(void) {
    draw_lawn();
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
            draw_bug(r, x, y);
        }
    }
    if (g_fly_on) {
        draw_butterfly(g_fly_x, FLY_Y);
    }
    for (int i = 0; i < DROPS; i++) {
        if (g_drop_on[i]) {
            draw_bug_shot(g_drop_x[i], g_drop_y[i]);
        }
    }
    if (g_shot_on) {
        blit_key(g_shot_x, g_shot_y, SHOT_W, SHOT_H, k_goccia);
    }
    draw_gardener();
    draw_hud();
    if (g_state == ST_READY) {
        text_center(100, "READY", LIPPUS_YELLOW, 1);
    }
}

static void draw_game_over(void) {
    char num[8];
    format_u(num, (unsigned)g_score);
    gfx_fill_rect(40, 88, 240, 64, LIPPUS_RGB(10, 22, 16));
    gfx_rect(40, 88, 240, 64, LIPPUS_AMBER);
    text_center(100, "GAME OVER", LIPPUS_AMBER, 1);
    text_center(124, num, LIPPUS_CREAM, 1);
}

static void draw_press_start(void) {
    if ((g_frame & 16) == 0) {
        text_center(g_h - PG_LH - 2, "PRESS START", LIPPUS_YELLOW, 1);
    }
}

static void draw_demo(void) {
    int t = g_attract_t;
    const char *caption = "SLIDE THE SPRAY";
    int px;
    int i;
    gfx_fill_rect(0, 0, g_w, 40, COL_TURF);
    text_center(4, "GARDEN DEFENDER", LIPPUS_AMBER, 1);
    if (t < 105) {
        int swing = t % 70;
        px = 70 + (swing < 35 ? swing * 4 : (70 - swing) * 4);
        caption = "SLIDE THE SPRAY";
        draw_gardener_at(px, 0);
        blit_key(pot_x(0), POT_Y, POT_W, POT_H, k_vasi[0]);
        blit_key(pot_x(1), POT_Y, POT_W, POT_H, k_vasi[1]);
        blit_key(pot_x(2), POT_Y, POT_W, POT_H, k_vasi[2]);
    } else if (t < 210) {
        int u = t - 105;
        int shot_y;
        px = 148;
        caption = u < 24 ? "HOLD A TO SPRAY" : "WATER STOPS THE BUGS";
        draw_gardener_at(px, 1);
        shot_y = PLAYER_Y - SHOT_H - u * 3;
        if (shot_y < 40) {
            shot_y = 40;
        }
        blit_key(px + 12 - SHOT_W / 2, shot_y, SHOT_W, SHOT_H, k_goccia);
        for (i = 0; i < 4; i++) {
            int bx = 90 + i * 36;
            int by = 72;
            if (shot_y > by + 8 || i > 1) {
                blit_key(bx, by, BUG16_W, BUG16_H, k_bug1[(g_frame / 8) & 1]);
            }
        }
        blit_key(pot_x(1), POT_Y, POT_W, POT_H, k_vasi[1]);
    } else if (t < 315) {
        int u = t - 210;
        int bx = 40 + (u % 50) * 2;
        caption = "BUGS EAT THE POTS";
        blit_key(bx, POT_Y - 20, BUG16_W, BUG16_H, k_bug2[(g_frame / 8) & 1]);
        blit_key(bx + 48, POT_Y - 18, BUG16_W, BUG16_H, k_bug3[(g_frame / 8) & 1]);
        blit_key(bx + 96, POT_Y - 16, BUG8_W, BUG8_H, k_bug5[(g_frame / 8) & 1]);
        blit_key(pot_x(0), POT_Y, POT_W, POT_H, k_vasi[0]);
        blit_key(pot_x(1), POT_Y, POT_W, POT_H, k_vasi[1]);
        blit_key(pot_x(2), POT_Y, POT_W, POT_H, k_vasi[2]);
        draw_gardener_at(148, 0);
        if (u > 50) {
            caption = "GUARD THE GARDEN";
        }
    } else {
        int u = t - 315;
        int fly = 20 + u * 3;
        int goo = 48 + u * 2;
        caption = "BUTTERFLY SCORES 200";
        if (fly > 260) {
            fly = 260;
        }
        draw_butterfly(fly, 48);
        if (u > 36) {
            caption = "DROPS COST A LIFE";
            if (goo > PLAYER_Y) {
                goo = PLAYER_Y;
            }
            draw_bug_shot(160, goo);
        }
        blit_key(pot_x(1), POT_Y, POT_W, POT_H, k_vasi[1]);
        draw_gardener_at(148, u > 50);
        blit_key(24, PLAYER_Y + 8, SHOT_W, SHOT_H, k_goccia);
        blit_key(36, PLAYER_Y + 8, SHOT_W, SHOT_H, k_goccia);
        blit_key(48, PLAYER_Y + 8, SHOT_W, SHOT_H, k_goccia);
    }
    text_center(22, caption, LIPPUS_CREAM, 1);
}

static void draw_score_row(int x, int y, int rank, const score_t *row, uint16_t color) {
    char pos[4];
    char name[4];
    char num[8];
    format_u(pos, (unsigned)(rank + 1));
    name[0] = row->name[0];
    name[1] = row->name[1];
    name[2] = row->name[2];
    name[3] = 0;
    format_u(num, (unsigned)row->score);
    pg_text(x, y, pos, color, 1);
    pg_text(x + 28, y, name, color, 1);
    pg_text(x + 148 - pg_width(num, 1), y, num, color, 1);
}

static void draw_scores(void) {
    int i;
    text_center(4, "TOP 20", LIPPUS_AMBER, 1);
    if (g_score_n == 0) {
        text_center(100, "NO SCORES YET", LIPPUS_CREAM, 1);
        return;
    }
    for (i = 0; i < g_score_n; i++) {
        uint16_t color = (i == g_rank) ? LIPPUS_YELLOW : LIPPUS_CREAM;
        int col = i / 10;
        int row = i % 10;
        draw_score_row(8 + col * 160, 26 + row * 18, i, &g_scores[i], color);
    }
}

static void draw_attract(void) {
    draw_lawn();
    if (g_attract_page == 0) {
        draw_demo();
    } else {
        gfx_fill_rect(0, 0, g_w, 22, COL_TURF);
        draw_scores();
    }
    draw_press_start();
}

static void draw_name(void) {
    char shown[4];
    char num[8];
    format_u(num, (unsigned)g_score);
    draw_lawn();
    gfx_fill_rect(0, 0, g_w, 22, COL_TURF);
    text_center(16, "NEW HIGH SCORE", LIPPUS_AMBER, 1);
    text_center(40, num, LIPPUS_CREAM, 1);
    text_center(68, "YOUR NAME", LIPPUS_CREAM, 1);
    shown[0] = g_initials[0];
    shown[1] = g_initials[1];
    shown[2] = g_initials[2];
    shown[3] = 0;
    text_center(104, shown, LIPPUS_YELLOW, 1);
    if ((g_frame & 8) == 0) {
        char one[2];
        int x = (g_w - pg_width(shown, 1)) / 2;
        int i;
        one[1] = 0;
        for (i = 0; i < g_name_i; i++) {
            one[0] = shown[i];
            x += pg_width(one, 1);
        }
        one[0] = shown[g_name_i];
        gfx_fill_rect(x, 124, pg_width(one, 1), 3, LIPPUS_YELLOW);
    }
    text_center(156, "UP DOWN", LIPPUS_CREAM, 1);
    text_center(176, "LEFT RIGHT", LIPPUS_CREAM, 1);
    text_center(196, "START ENTERS", LIPPUS_CREAM, 1);
}

static void tick_name(const lippus_pad_t *pad) {
    int move = 0;
    if (pad_pressed(pad, LIPPUS_UP)) {
        move = 1;
    } else if (pad_pressed(pad, LIPPUS_DOWN)) {
        move = -1;
    }
    if (move == 0 && (pad_held(pad, LIPPUS_UP) || pad_held(pad, LIPPUS_DOWN))) {
        if (++g_letter_hold > 10 && (g_letter_hold & 3) == 0) {
            move = pad_held(pad, LIPPUS_UP) ? 1 : -1;
        }
    } else {
        g_letter_hold = 0;
    }
    if (move != 0) {
        char ch = (char)(g_initials[g_name_i] + move);
        if (ch < 'A') {
            ch = 'Z';
        }
        if (ch > 'Z') {
            ch = 'A';
        }
        g_initials[g_name_i] = ch;
    }
    if (pad_pressed(pad, LIPPUS_LEFT) && g_name_i > 0) {
        g_name_i--;
    }
    if (pad_pressed(pad, LIPPUS_RIGHT) && g_name_i < 2) {
        g_name_i++;
    }
    if (pad_pressed(pad, LIPPUS_A) || pad_pressed(pad, LIPPUS_START)) {
        score_insert(g_score);
        scores_save();
        g_attract_page = 1;
        g_attract_t = 0;
        g_state = ST_ATTRACT;
    }
}

static void enter_attract(int page) {
    g_state = ST_ATTRACT;
    g_attract_page = page;
    g_attract_t = 0;
}

static void enter_name(void) {
    g_initials[0] = 'A';
    g_initials[1] = 'A';
    g_initials[2] = 'A';
    g_name_i = 0;
    g_letter_hold = 0;
    g_state = ST_NAME;
}

static void enter_game_over(void) {
    g_state = ST_OVER;
    g_timer = 120;
}

void lippus_main(void) {
    g_w = gfx_width();
    g_h = gfx_height();
    g_rank = -1;
    scores_load();
    enter_attract(0);
    for (;;) {
        lippus_pad_t pad = pad_poll();
        g_frame++;
        if (g_state == ST_ATTRACT) {
            if (pad_pressed(&pad, LIPPUS_START)) {
                g_rank = -1;
                new_game();
            } else {
                int attract_len = (g_attract_page == 0) ? ATTRACT_LEN : ATTRACT_SCORE_LEN;
                if (++g_attract_t >= attract_len) {
                    g_attract_t = 0;
                    g_attract_page ^= 1;
                }
                draw_attract();
            }
            if (g_state != ST_ATTRACT) {
                draw_world();
            }
            gfx_present();
            sys_frame(33);
            continue;
        }
        if (g_state == ST_NAME) {
            tick_name(&pad);
            if (g_state == ST_NAME) {
                draw_name();
            } else {
                draw_attract();
            }
            gfx_present();
            sys_frame(33);
            continue;
        }
        if (g_state == ST_OVER) {
            draw_world();
            draw_game_over();
            gfx_present();
            if (--g_timer <= 0) {
                if (score_qualifies(g_score)) {
                    enter_name();
                } else {
                    g_rank = -1;
                    enter_attract(1);
                }
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
                    enter_game_over();
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
        if (g_state != ST_PLAY) {
            g_spray = 0;
        }
        draw_world();
        gfx_present();
        sys_frame(33);
    }
}
