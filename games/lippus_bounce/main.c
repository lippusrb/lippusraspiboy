#include "lippus.h"
#include "title.h"
#include "court_font.h"

/* Landscape court. Tennis points, first to 3 games.
   One lost game ends the match. Start runs it from the attract mode. */

#define PAD_W 6
#define PAD_H 42
#define BALL_R 3
#define FIELD_TOP (COURT_SM_H * 2 + 10)
#define MARGIN 14
#define PLAYER_X MARGIN
#define CPU_X (LIPPUS_SCREEN_W - MARGIN - PAD_W)
#define SCORE_N 20
#define RECORD_NAME "BOUNCE"
#define ATTRACT_LEN 500
#define SCORE_LEN 80
#define TITLE_SKY 14
#define CAPTION_Y (LIPPUS_SCREEN_H - COURT_SM_H * 2 - 8)
#define DEMO_TOP (TITLE_SKY + TITLE_H + COURT_SM_H + 6)
#define DEMO_BOT (CAPTION_Y - 8)

#define ST_ATTRACT 0
#define ST_NAME 1
#define ST_READY 2
#define ST_SERVE 3
#define ST_PLAY 4
#define ST_GAME 5
#define ST_OVER 6

#define COL_GRASS LIPPUS_RGB(24, 76, 40)
#define COL_BAND LIPPUS_RGB(14, 46, 24)
#define COL_LINE LIPPUS_RGB(232, 220, 196)
#define COL_CPU LIPPUS_RGB(72, 206, 186)

static const lippus_note_t g_win_notes[] = {
    {523, 90}, {659, 90}, {784, 160}, {1046, 240},
};
static const lippus_note_t g_lose_notes[] = {
    {349, 150}, {294, 150}, {220, 280},
};

typedef struct {
    char name[3];
    int score;
} score_t;

static int g_py;
static int g_cy;
static int g_bx;
static int g_by;
static int g_vx;
static int g_vy;
static int g_pp;
static int g_cp;
static int g_adv;
static int g_pg;
static int g_points;
static int g_hits;
static int g_heat;
static int g_server;
static int g_serve_y;
static int g_state;
static int g_timer;
static int g_won;
static int g_flash;
static int g_last_player;
static int g_frame;
static int g_score_n;
static int g_rank;
static int g_name_i;
static int g_letter_hold;
static int g_attract_page;
static int g_attract_t;
static char g_initials[3];
static char g_rec[LIPPUS_RECORD_MAX];
static score_t g_scores[SCORE_N];
static uint32_t g_rng = 0xC0FFEEu;

static unsigned rnd(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 16;
}

static void write_u(char *dst, int *used, unsigned value) {
    char tmp[8];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && n < 8);
    while (n > 0) {
        dst[(*used)++] = tmp[--n];
    }
}

static void format_u(char *dst, unsigned value) {
    int used = 0;
    write_u(dst, &used, value);
    dst[used] = 0;
}

static const court_glyph_t *court_glyph(int size, char ch) {
    const court_glyph_t *tab = size == COURT_LG ? k_court_lg : k_court_sm;
    int i = 37;
    if (ch >= 'A' && ch <= 'Z') {
        i = ch - 'A';
    } else if (ch >= '0' && ch <= '9') {
        i = 26 + (ch - '0');
    } else if (ch == ',') {
        i = 36;
    }
    return &tab[i];
}

static const uint8_t *court_px(int size) {
    return size == COURT_LG ? k_court_lg_px : k_court_sm_px;
}

static void court_mix(int x, int y, int a, uint16_t color, uint16_t bg) {
    int cr;
    int cg;
    int cb;
    int br;
    int bgc;
    int bb;
    if (a < 8) {
        return;
    }
    if ((unsigned)x >= (unsigned)LIPPUS_SCREEN_W || (unsigned)y >= (unsigned)LIPPUS_SCREEN_H) {
        return;
    }
    if (a >= 248) {
        gfx_pixel(x, y, color);
        return;
    }
    cr = (color >> 11) << 3;
    cg = ((color >> 5) & 63) << 2;
    cb = (color & 31) << 3;
    br = (bg >> 11) << 3;
    bgc = ((bg >> 5) & 63) << 2;
    bb = (bg & 31) << 3;
    gfx_pixel(x, y, LIPPUS_RGB((br * (255 - a) + cr * a) / 255,
                               (bgc * (255 - a) + cg * a) / 255,
                               (bb * (255 - a) + cb * a) / 255));
}

static void court_edges(const char *s, int size, int *lo, int *hi) {
    int x = 0;
    int any = 0;
    int minx = 0;
    int maxx = 0;
    for (; *s; s++) {
        const court_glyph_t *g = court_glyph(size, *s);
        if (g->w) {
            int left = x + g->dx;
            int right = left + g->w;
            if (!any || left < minx) {
                minx = left;
            }
            if (!any || right > maxx) {
                maxx = right;
            }
            any = 1;
        }
        x += g->adv;
    }
    if (!any) {
        *lo = 0;
        *hi = x;
        return;
    }
    *lo = minx;
    *hi = maxx;
}

static void court_text(int x, int y, const char *s, uint16_t color, int size, uint16_t bg) {
    const uint8_t *px = court_px(size);
    for (; *s; s++) {
        const court_glyph_t *g = court_glyph(size, *s);
        const uint8_t *src = px + g->off;
        int row;
        int col;
        for (row = 0; row < g->h; row++) {
            for (col = 0; col < g->w; col++) {
                court_mix(x + g->dx + col, y + g->dy + row, src[row * g->w + col], color, bg);
            }
        }
        x += g->adv;
    }
}

static void court_center(int y, const char *s, uint16_t color, int size, uint16_t bg) {
    int lo;
    int hi;
    court_edges(s, size, &lo, &hi);
    court_text((LIPPUS_SCREEN_W - (hi - lo)) / 2 - lo, y, s, color, size, bg);
}

static void court_right(int right, int y, const char *s, uint16_t color, int size, uint16_t bg) {
    int lo;
    int hi;
    court_edges(s, size, &lo, &hi);
    (void)lo;
    court_text(right - hi, y, s, color, size, bg);
}

static int board_score(void) {
    int points = g_points;
    if (points > 999) {
        points = 999;
    }
    return g_pg * 1000 + points;
}

static const char *point_name(int mine, int theirs, int who) {
    if (mine >= 3 && theirs >= 3) {
        if (g_adv == who) {
            return "AD";
        }
        return "40";
    }
    if (mine <= 0) {
        return "0";
    }
    if (mine == 1) {
        return "15";
    }
    if (mine == 2) {
        return "30";
    }
    return "40";
}

static void quiet(void) {
    unsigned i;
    for (i = 0; i < SND_VOICES; i++) {
        snd_voice(i, SND_OFF, 0, 0, 0);
    }
}

static void clamp_pad(int *y) {
    int top = FIELD_TOP + 2;
    int bot = LIPPUS_SCREEN_H - PAD_H - 2;
    if (*y < top) {
        *y = top;
    }
    if (*y > bot) {
        *y = bot;
    }
}

static void place_ball(void) {
    g_bx = LIPPUS_SCREEN_W / 2;
    g_by = (FIELD_TOP + LIPPUS_SCREEN_H) / 2;
    g_vx = 0;
    g_vy = 0;
}

/* Speed climbs during the game. Later games start faster and climb sooner. */
static int pace_now(void) {
    int base = 3 + g_pg;
    int step = 40 - g_pg * 13;
    int gain;
    int cap = 7 + g_pg * 2;
    int pace;
    if (step < 10) {
        step = 10;
    }
    gain = g_heat / step + g_hits * (1 + g_pg) / 4;
    pace = base + gain;
    if (pace > cap) {
        pace = cap;
    }
    return pace;
}

static void pick_serve(void) {
    int top = FIELD_TOP + 4;
    int bot = LIPPUS_SCREEN_H - PAD_H - 4;
    int span = bot - top;
    int edge;
    int chance;
    if (span < 1) {
        span = 1;
    }
    edge = span / 5;
    if (edge < 6) {
        edge = 6;
    }
    if (!g_server) {
        g_serve_y = (top + bot) / 2;
        return;
    }
    chance = 30 + g_pg * 30;
    if ((int)(rnd() % 100u) < chance) {
        if (rnd() & 1u) {
            g_serve_y = top + (int)(rnd() % (unsigned)edge);
        } else {
            g_serve_y = bot - (int)(rnd() % (unsigned)edge);
        }
    } else {
        g_serve_y = top + (int)(rnd() % (unsigned)span);
    }
}

static void stick_serve(void) {
    if (g_server) {
        g_bx = CPU_X - BALL_R;
        g_by = g_cy + PAD_H / 2;
    } else {
        g_bx = PLAYER_X + PAD_W + BALL_R;
        g_by = g_py + PAD_H / 2;
    }
    g_vx = 0;
    g_vy = 0;
}

static void launch(void) {
    int mid = (FIELD_TOP + LIPPUS_SCREEN_H) / 2;
    int off = g_by - mid;
    int denom = 12 - g_pg * 2;
    int vy_cap = 3 + g_pg;
    int pace = pace_now();
    int vy;
    if (denom < 8) {
        denom = 8;
    }
    vy = off / denom;
    if (vy > vy_cap) {
        vy = vy_cap;
    }
    if (vy < -vy_cap) {
        vy = -vy_cap;
    }
    g_vx = g_server ? -pace : pace;
    g_vy = vy;
    g_state = ST_PLAY;
    snd_voice(0, SND_SQUARE, (unsigned)(480 + pace * 40), 150, 30);
}

static void bounce(int y_hit, int pad_y, int dir) {
    int off = y_hit - (pad_y + PAD_H / 2);
    int vy = off / 3;
    int vy_cap = 4 + g_pg;
    int pace;
    if (vy_cap > 6) {
        vy_cap = 6;
    }
    if (vy > vy_cap) {
        vy = vy_cap;
    }
    if (vy < -vy_cap) {
        vy = -vy_cap;
    }
    if (vy == 0) {
        vy = off < 0 ? -1 : 1;
    }
    g_hits++;
    pace = pace_now();
    g_vx = dir * pace;
    g_vy = vy;
    snd_voice(0, SND_SQUARE, (unsigned)(620 + pace * 40), 190, 40);
}

static int crossed(int x0, int y0, int x1, int y1, int plane, int toward_left, int *y_hit) {
    int e0 = toward_left ? x0 - BALL_R : x0 + BALL_R;
    int e1 = toward_left ? x1 - BALL_R : x1 + BALL_R;
    int denom;
    int hit = toward_left ? (e0 >= plane && e1 <= plane) : (e0 <= plane && e1 >= plane);
    if (!hit) {
        return 0;
    }
    denom = e0 - e1;
    if (denom == 0) {
        *y_hit = y1;
    } else {
        *y_hit = y0 + (y1 - y0) * (e0 - plane) / denom;
    }
    return 1;
}

static int on_paddle(int y_hit, int pad_y) {
    return y_hit >= pad_y - BALL_R && y_hit <= pad_y + PAD_H + BALL_R;
}

static void take_game(int player) {
    g_hits = 0;
    g_heat = 0;
    g_flash = 0;
    if (!player) {
        place_ball();
        g_won = 0;
        g_state = ST_OVER;
        g_timer = 100;
        snd_tune(2, SND_TRIANGLE, 140, g_lose_notes, 3);
        return;
    }
    if (g_pg < 3) {
        g_pg++;
    }
    if (g_pg >= 3) {
        place_ball();
        g_won = 1;
        g_state = ST_OVER;
        g_timer = 100;
        snd_tune(2, SND_TRIANGLE, 170, g_win_notes, 4);
        return;
    }
    g_pp = 0;
    g_cp = 0;
    g_adv = 0;
    g_server ^= 1;
    g_state = ST_GAME;
    g_timer = 70;
    pick_serve();
    stick_serve();
    snd_voice(2, SND_TRIANGLE, 784, 170, 180);
}

static void win_point(int player) {
    int side = player ? 1 : 2;
    g_last_player = player;
    g_flash = 22;
    snd_voice(1, SND_NOISE, player ? 880u : 150u, 160, 90);
    if (player && g_points < 999) {
        g_points++;
    }
    if (g_pp >= 3 && g_cp >= 3) {
        if (g_adv == side) {
            take_game(player);
            return;
        }
        if (g_adv == 0) {
            g_adv = side;
        } else {
            g_adv = 0;
        }
    } else if (player) {
        if (g_pp >= 3) {
            take_game(1);
            return;
        }
        g_pp++;
    } else if (g_cp >= 3) {
        take_game(0);
        return;
    } else {
        g_cp++;
    }
    g_state = ST_SERVE;
    g_timer = 50;
    pick_serve();
    stick_serve();
}

static void step_ball(void) {
    int x0 = g_bx;
    int y0 = g_by;
    int y_hit = g_by;
    int bounced = 0;

    g_bx += g_vx;
    g_by += g_vy;

    if (g_vx < 0) {
        if (crossed(x0, y0, g_bx, g_by, PLAYER_X + PAD_W, 1, &y_hit) && on_paddle(y_hit, g_py)) {
            g_bx = PLAYER_X + PAD_W + BALL_R;
            g_by = y_hit;
            bounce(y_hit, g_py, 1);
            bounced = 1;
        }
    } else if (g_vx > 0) {
        if (crossed(x0, y0, g_bx, g_by, CPU_X, 0, &y_hit) && on_paddle(y_hit, g_cy)) {
            g_bx = CPU_X - BALL_R;
            g_by = y_hit;
            bounce(y_hit, g_cy, -1);
            bounced = 1;
        }
    }

    if (g_by - BALL_R < FIELD_TOP) {
        g_by = FIELD_TOP + BALL_R;
        if (g_vy < 0) {
            g_vy = -g_vy;
            if (!bounced) {
                snd_voice(0, SND_SQUARE, 420, 120, 28);
            }
        }
    } else if (g_by + BALL_R >= LIPPUS_SCREEN_H) {
        g_by = LIPPUS_SCREEN_H - BALL_R - 1;
        if (g_vy > 0) {
            g_vy = -g_vy;
            if (!bounced) {
                snd_voice(0, SND_SQUARE, 420, 120, 28);
            }
        }
    }

    if (bounced) {
        return;
    }
    if (g_bx + BALL_R < PLAYER_X) {
        win_point(0);
    } else if (g_bx - BALL_R > CPU_X + PAD_W) {
        win_point(1);
    }
}

static void move_cpu(void) {
    int target;
    int speed;
    int dead;
    int skill = g_pg;
    int serving = g_state == ST_READY || g_state == ST_GAME || g_state == ST_SERVE;
    if (serving && g_server) {
        target = g_serve_y;
        speed = 4;
        dead = 1;
    } else if (serving) {
        target = (FIELD_TOP + LIPPUS_SCREEN_H) / 2 - PAD_H / 2;
        speed = 2 + skill;
        dead = 4;
    } else if (g_state == ST_PLAY && (g_vx > 0 || skill >= 2)) {
        target = g_by - PAD_H / 2;
        if (skill >= 1 && g_vx > 0) {
            target += g_vy * skill;
        }
        speed = 2 + skill;
        if (g_vx <= 0) {
            speed = 1 + skill;
        } else if (g_hits >= 6) {
            speed++;
        }
        if (speed > 5) {
            speed = 5;
        }
        dead = 10 - skill * 4;
        if (dead < 2) {
            dead = 2;
        }
    } else {
        target = (FIELD_TOP + LIPPUS_SCREEN_H) / 2 - PAD_H / 2;
        speed = 1 + (skill > 0 ? 1 : 0);
        dead = 8;
    }
    if (target - g_cy > dead) {
        g_cy += speed;
    } else if (target - g_cy < -dead) {
        g_cy -= speed;
    }
    clamp_pad(&g_cy);
}

static void draw_pips(int x, int y, int filled) {
    int i;
    for (i = 0; i < 3; i++) {
        if (i < filled) {
            gfx_fill_rect(x + i * 16, y, 10, 8, LIPPUS_AMBER);
        } else {
            gfx_rect(x + i * 16, y, 10, 8, COL_LINE);
        }
    }
}

static void draw_paddles(int py, int cy) {
    gfx_fill_rect(PLAYER_X, py, PAD_W, PAD_H, LIPPUS_AMBER);
    gfx_fill_rect(PLAYER_X + PAD_W - 2, py + 2, 2, PAD_H - 4, LIPPUS_CREAM);
    gfx_fill_rect(CPU_X, cy, PAD_W, PAD_H, COL_CPU);
    gfx_fill_rect(CPU_X, cy + 2, 2, PAD_H - 4, LIPPUS_WHITE);
}

static void draw_ball(int cx, int cy) {
    gfx_blit(cx - BALL_W / 2, cy - BALL_H / 2, BALL_W, BALL_H, k_ball, 1, TITLE_BALL_KEY);
}

static void draw_marks(int top, int bot) {
    int y;
    gfx_fill_rect(0, top, LIPPUS_SCREEN_W, 2, COL_LINE);
    gfx_fill_rect(0, bot - 2, LIPPUS_SCREEN_W, 2, COL_LINE);
    for (y = top + 8; y < bot - 8; y += 14) {
        gfx_fill_rect(LIPPUS_SCREEN_W / 2 - 1, y, 2, 6, COL_LINE);
    }
}

static void draw_hud(void) {
    char num[8];
    const char *lp = point_name(g_pp, g_cp, 1);
    const char *rp = point_name(g_cp, g_pp, 2);
    uint16_t pc = (g_flash && g_last_player) ? LIPPUS_WHITE : LIPPUS_AMBER;
    uint16_t cc = (g_flash && !g_last_player) ? LIPPUS_WHITE : COL_CPU;
    format_u(num, (unsigned)board_score());
    court_text(8, 4, "YOU", LIPPUS_AMBER, COURT_SM, COL_GRASS);
    court_text(8, 6 + COURT_SM_H, lp, pc, COURT_SM, COL_GRASS);
    court_center(4, num, LIPPUS_CREAM, COURT_SM, COL_GRASS);
    draw_pips((LIPPUS_SCREEN_W - 42) / 2, 6 + COURT_SM_H + (COURT_SM_H - 8) / 2, g_pg);
    court_right(LIPPUS_SCREEN_W - 8, 4, "CPU", COL_CPU, COURT_SM, COL_GRASS);
    court_right(LIPPUS_SCREEN_W - 8, 6 + COURT_SM_H, rp, cc, COURT_SM, COL_GRASS);
}

static void draw_court(void) {
    gfx_clear(COL_GRASS);
    draw_marks(FIELD_TOP - 2, LIPPUS_SCREEN_H);
    draw_hud();
    draw_paddles(g_py, g_cy);
    draw_ball(g_bx, g_by);
}

static void draw_banner(const char *title, const char *sub, uint16_t color) {
    int w = 250;
    int h = COURT_LG_H + COURT_SM_H + 28;
    int x = (LIPPUS_SCREEN_W - w) / 2;
    int y = FIELD_TOP + 24;
    gfx_fill_rect(x, y, w, h, COL_BAND);
    gfx_rect(x, y, w, h, color);
    court_center(y + 10, title, color, COURT_LG, COL_BAND);
    court_center(y + 16 + COURT_LG_H, sub, LIPPUS_CREAM, COURT_SM, COL_BAND);
}

static void draw_match(void) {
    char sub[16];
    int used;
    const char *serve = g_server ? "CPU SERVE" : "MOVE TO AIM";
    draw_court();
    if (g_state == ST_READY) {
        draw_banner("READY", serve, LIPPUS_AMBER);
    } else if (g_state == ST_GAME) {
        draw_banner("GAME", serve, LIPPUS_AMBER);
    } else if (g_state == ST_OVER) {
        used = 0;
        sub[used++] = 'S';
        sub[used++] = 'C';
        sub[used++] = 'O';
        sub[used++] = 'R';
        sub[used++] = 'E';
        sub[used++] = ' ';
        write_u(sub, &used, (unsigned)board_score());
        sub[used] = 0;
        draw_banner(g_won ? "YOU WIN" : "GAME OVER", sub, g_won ? LIPPUS_AMBER : LIPPUS_RED);
    } else if (g_state == ST_SERVE && g_pp >= 3 && g_cp >= 3) {
        const char *call = "DEUCE";
        if (g_adv == 1) {
            call = "ADVANTAGE YOU";
        } else if (g_adv == 2) {
            call = "ADVANTAGE CPU";
        }
        court_center(FIELD_TOP + 8, call, LIPPUS_CREAM, COURT_LG, COL_GRASS);
    }
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

static void draw_score_row(int x, int right, int y, int rank, const score_t *row, uint16_t color) {
    char pos[4];
    char name[4];
    char num[8];
    format_u(pos, (unsigned)(rank + 1));
    name[0] = row->name[0];
    name[1] = row->name[1];
    name[2] = row->name[2];
    name[3] = 0;
    format_u(num, (unsigned)row->score);
    court_right(x + 22, y, pos, color, COURT_SM, COL_GRASS);
    court_text(x + 28, y, name, color, COURT_SM, COL_GRASS);
    court_right(right, y, num, color, COURT_SM, COL_GRASS);
}

static void draw_scores(void) {
    int i;
    int top = 6 + COURT_LG_H + 6;
    court_center(6, "TOP 20", LIPPUS_AMBER, COURT_LG, COL_GRASS);
    if (g_score_n == 0) {
        court_center(108, "NO SCORES YET", LIPPUS_CREAM, COURT_SM, COL_GRASS);
        return;
    }
    for (i = 0; i < g_score_n; i++) {
        int col = i / 10;
        int row = i % 10;
        uint16_t color = (i == g_rank) ? LIPPUS_YELLOW : LIPPUS_CREAM;
        draw_score_row(col ? 168 : 8, col ? 312 : 156, top + row * (COURT_SM_H + 2), i, &g_scores[i], color);
    }
}

static void draw_press_start(void) {
    if ((g_frame & 16) == 0) {
        court_center(LIPPUS_SCREEN_H - COURT_SM_H - 4, "PRESS START", LIPPUS_YELLOW, COURT_SM, COL_GRASS);
    }
}

static int wave(int t, int period, int amp) {
    int half = period / 2;
    int p = t % period;
    int up = p <= half ? p : period - p;
    if (half <= 0) {
        return 0;
    }
    return up * amp / half;
}

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void draw_demo_court(int py, int cy, int bx, int by, int ball) {
    int span = DEMO_BOT - DEMO_TOP - PAD_H - 8;
    py = clamp_i(py, DEMO_TOP + 2, DEMO_TOP + 2 + span);
    cy = clamp_i(cy, DEMO_TOP + 2, DEMO_TOP + 2 + span);
    draw_marks(DEMO_TOP, DEMO_BOT);
    court_text(8, DEMO_TOP - COURT_SM_H - 2, "YOU", LIPPUS_AMBER, COURT_SM, COL_GRASS);
    court_right(LIPPUS_SCREEN_W - 8, DEMO_TOP - COURT_SM_H - 2, "CPU", COL_CPU, COURT_SM, COL_GRASS);
    draw_paddles(py, cy);
    if (ball) {
        bx = clamp_i(bx, BALL_R + 2, LIPPUS_SCREEN_W - BALL_R - 2);
        by = clamp_i(by, DEMO_TOP + BALL_R, DEMO_BOT - BALL_R);
        draw_ball(bx, by);
    }
}

/* The ball comes in from the left, clears the name, drops into
   the gap after the last letter, and settles with one bounce. */
static void draw_title_ball(int t) {
    const int rise = 10;
    const int cross = 24;
    const int drop = 12;
    const int settle = 10;
    int x;
    int y;
    int peak = TITLE_BALL_Y - TITLE_ARC;
    int left = TITLE_INK_L - BALL_W;

    if (t >= rise + cross + drop + settle) {
        x = TITLE_BALL_X;
        y = TITLE_BALL_Y;
    } else if (t < rise) {
        x = -BALL_W + (left + BALL_W) * t / rise;
        y = TITLE_BALL_Y + (peak - TITLE_BALL_Y) * t / rise;
    } else if (t < rise + cross) {
        int u = t - rise;
        x = left + (TITLE_BALL_X - left) * u / cross;
        y = peak;
    } else if (t < rise + cross + drop) {
        int u = t - rise - cross;
        int remain = drop - u;
        x = TITLE_BALL_X;
        y = TITLE_BALL_Y + (peak - TITLE_BALL_Y) * remain * remain / (drop * drop);
    } else {
        int u = t - rise - cross - drop;
        int lift = 5 * 4 * u * (settle - u) / (settle * settle);
        x = TITLE_BALL_X;
        y = TITLE_BALL_Y - lift;
    }
    gfx_blit(x, y + TITLE_SKY, BALL_W, BALL_H, k_ball, 1, TITLE_BALL_KEY);
}

static void draw_demo(void) {
    int scene = g_attract_t / 100;
    int u = g_attract_t % 100;
    int span = DEMO_BOT - DEMO_TOP - PAD_H - 8;
    int mid = DEMO_TOP + 4 + span / 2;
    const char *caption = "UP AND DOWN MOVE THE PADDLE";
    if (scene > 4) {
        scene = 4;
    }
    gfx_blit(0, TITLE_SKY, TITLE_W, TITLE_H, k_title, 0, 0);
    draw_title_ball(g_attract_t);
    if (scene == 0) {
        draw_demo_court(DEMO_TOP + 4 + wave(u, 80, span), mid, 0, 0, 0);
    } else if (scene == 1) {
        int bx;
        int by;
        int py;
        caption = "RETURN THE BALL";
        if (u < 50) {
            bx = 269 - u * 5;
            by = 120;
            py = by - PAD_H / 2;
        } else {
            int v = u - 50;
            bx = 24 + v * 4;
            by = 120 - v;
            py = 96;
        }
        draw_demo_court(py, by - PAD_H / 2, bx, by, 1);
    } else if (scene == 2) {
        int bx;
        int by;
        int py = 124;
        caption = "THE EDGE CHANGES THE ANGLE";
        if (u < 36) {
            bx = 274 - u * 7;
            by = py + 4;
        } else {
            int v = u - 36;
            bx = 29 + v * 4;
            by = py + 4 - v * 3;
            if (by < DEMO_TOP + 14) {
                by = DEMO_TOP + 14;
            }
        }
        draw_demo_court(py, mid, bx, by, 1);
    } else if (scene == 3) {
        const char *you = "0";
        const char *cpu = "0";
        caption = "0  15  30  40, THEN DEUCE AND AD";
        if (u >= 80) {
            you = "AD";
            cpu = "40";
            court_center(70, "ADVANTAGE", LIPPUS_CREAM, COURT_LG, COL_GRASS);
        } else if (u >= 64) {
            you = "40";
            cpu = "40";
            court_center(70, "DEUCE", LIPPUS_CREAM, COURT_LG, COL_GRASS);
        } else if (u >= 48) {
            you = "40";
        } else if (u >= 32) {
            you = "30";
        } else if (u >= 16) {
            you = "15";
        }
        court_text(48, 108, "YOU", LIPPUS_AMBER, COURT_LG, COL_GRASS);
        court_text(48, 112 + COURT_LG_H, you, LIPPUS_AMBER, COURT_LG, COL_GRASS);
        court_text(196, 108, "CPU", COL_CPU, COURT_LG, COL_GRASS);
        court_text(196, 112 + COURT_LG_H, cpu, COL_CPU, COURT_LG, COL_GRASS);
    } else {
        int got = u / 28;
        caption = "ONE LOST GAME ENDS THE MATCH";
        if (got > 3) {
            got = 3;
        }
        court_center(84, "WIN 3 GAMES", LIPPUS_AMBER, COURT_LG, COL_GRASS);
        draw_pips((LIPPUS_SCREEN_W - 42) / 2, 84 + COURT_LG_H + 8, got);
    }
    court_center(CAPTION_Y, caption, LIPPUS_CREAM, COURT_SM, COL_GRASS);
}

static void draw_attract(void) {
    gfx_clear(COL_GRASS);
    if (g_attract_page == 0) {
        draw_demo();
    } else {
        draw_scores();
    }
    draw_press_start();
}

static void draw_name(void) {
    char shown[4];
    char num[8];
    int y = 22;
    int init_y;
    int lo;
    int hi;
    int pen;
    format_u(num, (unsigned)board_score());
    gfx_clear(COL_GRASS);
    court_center(y, "NEW HIGH SCORE", LIPPUS_AMBER, COURT_LG, COL_GRASS);
    y += COURT_LG_H + 4;
    court_center(y, num, LIPPUS_CREAM, COURT_LG, COL_GRASS);
    y += COURT_LG_H + 14;
    court_center(y, "YOUR NAME", LIPPUS_CREAM, COURT_SM, COL_GRASS);
    init_y = y + COURT_SM_H + 8;
    shown[0] = g_initials[0];
    shown[1] = g_initials[1];
    shown[2] = g_initials[2];
    shown[3] = 0;
    court_edges(shown, COURT_LG, &lo, &hi);
    pen = (LIPPUS_SCREEN_W - (hi - lo)) / 2 - lo;
    court_text(pen, init_y, shown, LIPPUS_YELLOW, COURT_LG, COL_GRASS);
    if ((g_frame & 8) == 0) {
        int i;
        int at = pen;
        const court_glyph_t *g;
        for (i = 0; i < g_name_i; i++) {
            at += court_glyph(COURT_LG, shown[i])->adv;
        }
        g = court_glyph(COURT_LG, shown[g_name_i]);
        gfx_fill_rect(at + g->dx, init_y + COURT_LG_H - 3, g->w > 0 ? g->w : g->adv, 2, LIPPUS_YELLOW);
    }
    y = init_y + COURT_LG_H + 16;
    court_center(y, "UP DOWN", LIPPUS_CREAM, COURT_SM, COL_GRASS);
    court_center(y + COURT_SM_H + 3, "LEFT RIGHT", LIPPUS_CREAM, COURT_SM, COL_GRASS);
    court_center(y + (COURT_SM_H + 3) * 2, "START ENTERS", LIPPUS_CREAM, COURT_SM, COL_GRASS);
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
        score_insert(board_score());
        scores_save();
        g_attract_page = 1;
        g_attract_t = 0;
        g_state = ST_ATTRACT;
        quiet();
    }
}

static void enter_attract(int page) {
    g_state = ST_ATTRACT;
    g_attract_page = page;
    g_attract_t = 0;
    quiet();
}

static void enter_name(void) {
    g_initials[0] = 'A';
    g_initials[1] = 'A';
    g_initials[2] = 'A';
    g_name_i = 0;
    g_letter_hold = 0;
    g_state = ST_NAME;
    quiet();
}

static void new_game(void) {
    g_pp = 0;
    g_cp = 0;
    g_adv = 0;
    g_pg = 0;
    g_points = 0;
    g_hits = 0;
    g_heat = 0;
    g_flash = 0;
    g_won = 0;
    g_server = 0;
    g_last_player = 0;
    g_py = (FIELD_TOP + LIPPUS_SCREEN_H) / 2 - PAD_H / 2;
    g_cy = g_py;
    clamp_pad(&g_py);
    clamp_pad(&g_cy);
    pick_serve();
    stick_serve();
    g_state = ST_READY;
    g_timer = 70;
    quiet();
}

void lippus_main(void) {
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
                if (g_attract_page == 0) {
                    if (++g_attract_t >= ATTRACT_LEN) {
                        g_attract_t = 0;
                        g_attract_page = 1;
                    }
                } else if (++g_attract_t >= SCORE_LEN) {
                    g_attract_t = 0;
                    g_attract_page = 0;
                }
                draw_attract();
            }
            if (g_state == ST_READY) {
                draw_match();
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
            draw_match();
            gfx_present();
            if (--g_timer <= 0) {
                if (score_qualifies(board_score())) {
                    enter_name();
                } else {
                    g_rank = -1;
                    enter_attract(1);
                }
            }
            sys_frame(33);
            continue;
        }

        if (pad_held(&pad, LIPPUS_UP)) {
            g_py -= 5;
        }
        if (pad_held(&pad, LIPPUS_DOWN)) {
            g_py += 5;
        }
        clamp_pad(&g_py);

        if (g_state == ST_READY || g_state == ST_GAME || g_state == ST_SERVE) {
            int hit = !g_server && (pad_pressed(&pad, LIPPUS_A) || pad_pressed(&pad, LIPPUS_CLICK));
            move_cpu();
            stick_serve();
            g_timer--;
            if (g_timer <= 0 || hit) {
                launch();
            }
        } else if (g_state == ST_PLAY) {
            g_heat++;
            step_ball();
            if (g_state == ST_PLAY) {
                move_cpu();
            }
        }

        if (g_flash > 0) {
            g_flash--;
        }
        draw_match();
        gfx_present();
        sys_frame(33);
    }
}
