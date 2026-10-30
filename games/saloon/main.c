#include "lippus.h"

#include "sprites.h"

/* Orizzontale. Tre aperture: A finestra sinistra, B porta, X finestra destra.
   Gli sportelli si aprono e l'omino entra di lato. Il civile esce dall'altra parte.
   Alle finestre il personaggio scorre dentro l'apertura, senza la cornice.
   Il bandito va colpito prima che spari. Il civile no. */

#define ST_TITLE 0
#define ST_INTRO 1
#define ST_PLAY 2
#define ST_HURT 3
#define ST_CLEAR 4
#define ST_OVER 5

#define SLOTS 3

#define WHO_NONE 0
#define WHO_BANDIT 1
#define WHO_WOMAN 2
#define WHO_MAN 3

#define PH_EMPTY 0
#define PH_IN 1
#define PH_AIM 2
#define PH_HOLD 3
#define PH_OUT 4
#define PH_HIT 5
#define PH_ANGRY 6
#define PH_BANG 7

static const int k_sx[SLOTS] = {68, 137, 220};
static const int k_sy[SLOTS] = {102, 98, 102};
static const int k_sw[SLOTS] = {34, 46, 34};
static const int k_sh[SLOTS] = {58, 94, 58};
static const uint16_t k_btn[SLOTS] = {LIPPUS_A, LIPPUS_B, LIPPUS_X};
static const char k_key[SLOTS] = {'A', 'B', 'X'};

static const lippus_note_t k_theme[] = {
    {196, 240}, {0, 40},  {247, 160}, {294, 240}, {0, 80},
    {220, 160}, {262, 160}, {294, 240}, {330, 320}, {0, 120},
    {247, 160}, {294, 160}, {262, 160}, {220, 280}, {0, 80},
    {196, 160}, {247, 400}, {0, 280},
};

typedef struct slot {
    int who;
    int phase;
    int shown;
    int tall;
    int step;
    int timer;
    int bounty;
    int flash;
} slot_t;

static slot_t g_slot[SLOTS];
static int g_state;
static int g_timer;
static int g_score;
static int g_lives;
static int g_level;
static int g_quota;
static int g_got;
static int g_band_left;
static int g_civ_left;
static int g_aim;
static int g_sim;
static int g_gap;
static int g_spawn_wait;
static int g_pop_slot;
static int g_pop_val;
static int g_pop_t;
static int g_cause;
static int g_music_on;
static int g_music;
static uint32_t g_rng = 0x51A10Eu;

static unsigned rnd(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 16;
}

static void clear_slot(slot_t *s) {
    s->who = WHO_NONE;
    s->phase = PH_EMPTY;
    s->shown = 0;
    s->tall = 0;
    s->step = 0;
    s->timer = 0;
    s->bounty = 0;
    s->flash = 0;
}

static int pose_w(int slot, int who) {
    if (slot == 1) {
        if (who == WHO_BANDIT) {
            return SPR_BANDITO_PORTA_W;
        }
        if (who == WHO_WOMAN) {
            return SPR_DONNA_PORTA_W;
        }
        return SPR_UOMO_PORTA_W;
    }
    if (who == WHO_BANDIT) {
        return SPR_BANDITO_FINESTRA_W;
    }
    if (who == WHO_WOMAN) {
        return SPR_DONNA_FINESTRA_W;
    }
    return SPR_UOMO_FINESTRA_W;
}

/* Da fuori dall'apertura fino al centro. */
static int travel(int slot, int who) {
    int dist = (k_sw[slot] + pose_w(slot, who)) / 2;
    if (dist < 1) {
        dist = 1;
    }
    return dist;
}

static const uint16_t *const k_anta_sx[ANTA_N] = {
    spr_anta_sx0, spr_anta_sx1, spr_anta_sx2, spr_anta_sx3, spr_anta_sx4,
};
static const uint16_t *const k_anta_dx[ANTA_N] = {
    spr_anta_dx0, spr_anta_dx1, spr_anta_dx2, spr_anta_dx3, spr_anta_dx4,
};

static void actor_sprite(int slot, int who, int phase, int walking, const uint16_t **pix, int *w,
                         int *h) {
    if (slot == 1 && who == WHO_BANDIT && phase == PH_HIT) {
        *pix = spr_bandito_colpito;
        *w = SPR_BANDITO_COLPITO_W;
        *h = SPR_BANDITO_COLPITO_H;
        return;
    }
    if (slot == 1 && who == WHO_BANDIT && phase == PH_BANG) {
        *pix = spr_bandito_rinculo;
        *w = SPR_BANDITO_RINCULO_W;
        *h = SPR_BANDITO_RINCULO_H;
        return;
    }
    if (slot == 1 && who == WHO_WOMAN && phase == PH_ANGRY) {
        *pix = spr_donna_arrabbiata;
        *w = SPR_DONNA_ARRABBIATA_W;
        *h = SPR_DONNA_ARRABBIATA_H;
        return;
    }
    if (slot == 1 && who == WHO_MAN && phase == PH_ANGRY) {
        *pix = spr_uomo_arrabbiato;
        *w = SPR_UOMO_ARRABBIATO_W;
        *h = SPR_UOMO_ARRABBIATO_H;
        return;
    }
    if (slot == 1) {
        if (walking && who == WHO_BANDIT) {
            *pix = spr_bandito_passo;
            *w = SPR_BANDITO_PASSO_W;
            *h = SPR_BANDITO_PASSO_H;
            return;
        }
        if (walking && who == WHO_WOMAN) {
            *pix = spr_donna_passo;
            *w = SPR_DONNA_PASSO_W;
            *h = SPR_DONNA_PASSO_H;
            return;
        }
        if (walking && who == WHO_MAN) {
            *pix = spr_uomo_passo;
            *w = SPR_UOMO_PASSO_W;
            *h = SPR_UOMO_PASSO_H;
            return;
        }
        if (who == WHO_BANDIT) {
            *pix = spr_bandito_porta;
            *w = SPR_BANDITO_PORTA_W;
            *h = SPR_BANDITO_PORTA_H;
        } else if (who == WHO_WOMAN) {
            *pix = spr_donna_porta;
            *w = SPR_DONNA_PORTA_W;
            *h = SPR_DONNA_PORTA_H;
        } else {
            *pix = spr_uomo_porta;
            *w = SPR_UOMO_PORTA_W;
            *h = SPR_UOMO_PORTA_H;
        }
        return;
    }
    if (who == WHO_BANDIT) {
        *pix = spr_bandito_finestra;
        *w = SPR_BANDITO_FINESTRA_W;
        *h = SPR_BANDITO_FINESTRA_H;
    } else if (who == WHO_WOMAN) {
        *pix = spr_donna_finestra;
        *w = SPR_DONNA_FINESTRA_W;
        *h = SPR_DONNA_FINESTRA_H;
    } else {
        *pix = spr_uomo_finestra;
        *w = SPR_UOMO_FINESTRA_W;
        *h = SPR_UOMO_FINESTRA_H;
    }
}

static int level_quota(int level) {
    int n = 4 + (level - 1) * 2;
    if (n > 16) {
        n = 16;
    }
    return n;
}

static int level_civs(int level) {
    int n = 2 + (level - 1);
    if (n > 8) {
        n = 8;
    }
    return n;
}

static int level_sim(int level) {
    if (level < 3) {
        return 1;
    }
    if (level < 6) {
        return 2;
    }
    return 3;
}

static int level_aim(int level) {
    int t = 90 - (level - 1) * 6;
    if (t < 36) {
        t = 36;
    }
    return t;
}

static int level_gap(int level) {
    int t = 30 - (level - 1) * 2;
    if (t < 10) {
        t = 10;
    }
    return t;
}

static int slide_px(void) {
    int s = 2 + (g_level - 1) / 2;
    if (s > 4) {
        s = 4;
    }
    return s;
}

static int swing_time(void) {
    int t = 20 - (g_level - 1) * 2;
    if (t < 8) {
        t = 8;
    }
    return t;
}

static int door_frame(int step) {
    int span = swing_time();
    int frame;
    if (step >= span) {
        return ANTA_N - 1;
    }
    frame = step * (ANTA_N - 1) / span;
    if (frame < 0) {
        frame = 0;
    }
    if (frame > ANTA_N - 1) {
        frame = ANTA_N - 1;
    }
    return frame;
}

static int hold_time(void) {
    int t = 64 - (g_level - 1) * 4;
    if (t < 28) {
        t = 28;
    }
    return t + (int)(rnd() % 24u);
}

static void music_start(void) {
    int ms = 0;
    int i;
    int n = (int)(sizeof(k_theme) / sizeof(k_theme[0]));
    snd_tune(0, SND_TRIANGLE, 58, k_theme, (unsigned)n);
    for (i = 0; i < n; i++) {
        ms += (int)k_theme[i].ms;
    }
    g_music = ms / 33 + 1;
}

static void pump_music(void) {
    if (!g_music_on) {
        return;
    }
    if (g_music > 0) {
        g_music--;
        return;
    }
    music_start();
}

static int open_count(void) {
    int i;
    int n = 0;
    for (i = 0; i < SLOTS; i++) {
        if (g_slot[i].phase != PH_EMPTY) {
            n++;
        }
    }
    return n;
}

static int bandit_visible(void) {
    int i;
    for (i = 0; i < SLOTS; i++) {
        if (g_slot[i].who == WHO_BANDIT && g_slot[i].phase != PH_EMPTY) {
            return 1;
        }
    }
    return 0;
}

static void refund_and_clear(void) {
    int i;
    for (i = 0; i < SLOTS; i++) {
        slot_t *s = &g_slot[i];
        if (s->phase == PH_EMPTY) {
            continue;
        }
        if (s->who == WHO_BANDIT && s->phase != PH_HIT) {
            g_band_left++;
        } else if (s->who != WHO_BANDIT && s->phase != PH_ANGRY) {
            g_civ_left++;
        }
        clear_slot(s);
    }
}

static void lose_life(void) {
    if (g_state != ST_PLAY) {
        return;
    }
    g_lives--;
    snd_voice(3, SND_TRIANGLE, 110, 140, 220);
    if (g_lives <= 0) {
        g_state = ST_OVER;
        g_music_on = 0;
        snd_tune(0, SND_TRIANGLE, 0, 0, 0);
        return;
    }
    g_state = ST_HURT;
    g_timer = 42;
}

static int shootable(const slot_t *s) {
    if (s->who == WHO_NONE || s->tall <= 0) {
        return 0;
    }
    if (s->phase == PH_HIT || s->phase == PH_ANGRY || s->phase == PH_BANG || s->phase == PH_EMPTY) {
        return 0;
    }
    /* Finche' non e' entrato almeno a meta', il colpo non conta. */
    if (s->shown < s->tall / 2) {
        return 0;
    }
    return 1;
}

static void fire(int i) {
    slot_t *s = &g_slot[i];
    s->flash = 5;
    snd_voice(1, SND_NOISE, 980, 210, 36);
    if (!shootable(s)) {
        return;
    }
    s->shown = s->tall;
    if (i == 1) {
        s->step = swing_time();
    }
    if (s->who == WHO_BANDIT) {
        s->phase = PH_HIT;
        s->timer = 18;
        g_score += s->bounty;
        if (g_score > 999999) {
            g_score = 999999;
        }
        g_got++;
        g_pop_slot = i;
        g_pop_val = s->bounty;
        g_pop_t = 24;
        snd_voice(2, SND_SQUARE, 880, 100, 50);
        snd_voice(3, SND_SQUARE, 1320, 70, 70);
        return;
    }
    s->phase = PH_ANGRY;
    s->timer = 30;
    snd_voice(2, SND_TRIANGLE, 146, 170, 220);
}

static void tick_slot(int i) {
    slot_t *s = &g_slot[i];
    int sp = slide_px();
    if (s->flash > 0) {
        s->flash--;
    }
    if (s->phase == PH_IN) {
        if (i == 1 && s->step < swing_time()) {
            s->step++;
            return;
        }
        s->shown += sp;
        if (s->shown >= s->tall) {
            s->shown = s->tall;
            if (s->who == WHO_BANDIT) {
                s->phase = PH_AIM;
                s->timer = g_aim;
            } else {
                s->phase = PH_HOLD;
                s->timer = hold_time();
            }
        }
        return;
    }
    if (s->phase == PH_AIM) {
        if (s->timer > 0) {
            s->timer--;
        }
        if (s->timer <= 0) {
            s->phase = PH_BANG;
            s->shown = s->tall;
            snd_voice(1, SND_NOISE, 160, 190, 80);
            snd_voice(2, SND_SQUARE, 98, 150, 180);
            g_cause = 0;
            lose_life();
        }
        return;
    }
    if (s->phase == PH_HOLD) {
        if (s->timer > 0) {
            s->timer--;
        }
        if (s->timer <= 0) {
            s->phase = PH_OUT;
        }
        return;
    }
    if (s->phase == PH_OUT) {
        if (s->shown > 0) {
            s->shown -= sp;
            if (s->shown < 0) {
                s->shown = 0;
            }
            return;
        }
        if (i == 1 && s->step > 0) {
            s->step--;
            return;
        }
        clear_slot(s);
        return;
    }
    if (s->phase == PH_HIT) {
        if (s->timer > 0) {
            s->timer--;
        }
        if (s->timer <= 0) {
            if (i == 1) {
                s->phase = PH_OUT;
                s->shown = 0;
            } else {
                clear_slot(s);
            }
        }
        return;
    }
    if (s->phase == PH_ANGRY) {
        if (s->timer > 0) {
            s->timer--;
        }
        if (s->timer <= 0) {
            g_cause = 1;
            lose_life();
        }
    }
}

static int try_spawn(void) {
    int empty[SLOTS];
    int n = 0;
    int i;
    int pick;
    int bandit;
    int who;
    slot_t *s;
    if (g_got >= g_quota) {
        return 0;
    }
    if (g_band_left <= 0 && g_civ_left <= 0) {
        return 0;
    }
    if (open_count() >= g_sim) {
        return 0;
    }
    for (i = 0; i < SLOTS; i++) {
        if (g_slot[i].phase == PH_EMPTY) {
            empty[n++] = i;
        }
    }
    if (n <= 0) {
        return 0;
    }
    pick = empty[(int)(rnd() % (unsigned)n)];
    if (g_band_left <= 0) {
        bandit = 0;
    } else if (g_civ_left <= 0) {
        bandit = 1;
    } else {
        bandit = (rnd() % 5u) < 3u;
    }
    if (bandit) {
        who = WHO_BANDIT;
        g_band_left--;
    } else {
        who = (rnd() & 1u) ? WHO_WOMAN : WHO_MAN;
        g_civ_left--;
    }
    s = &g_slot[pick];
    s->who = who;
    s->phase = PH_IN;
    s->shown = 0;
    s->step = (pick == 1) ? 0 : swing_time();
    s->tall = travel(pick, who);
    s->timer = 0;
    s->flash = 0;
    s->bounty = 0;
    if (bandit) {
        int bonus = (g_level - 1) * 50;
        if (bonus > 600) {
            bonus = 600;
        }
        s->bounty = 100 * (1 + (int)(rnd() % 3u)) + bonus;
    }
    return 1;
}

static void begin_level(void) {
    int i;
    for (i = 0; i < SLOTS; i++) {
        clear_slot(&g_slot[i]);
    }
    g_quota = level_quota(g_level);
    g_got = 0;
    g_band_left = g_quota;
    g_civ_left = level_civs(g_level);
    g_aim = level_aim(g_level);
    g_sim = level_sim(g_level);
    g_gap = level_gap(g_level);
    g_spawn_wait = 16;
    g_pop_t = 0;
    g_state = ST_INTRO;
    g_timer = 36;
}

static void new_game(void) {
    g_score = 0;
    g_lives = 3;
    g_level = 1;
    g_music_on = 1;
    music_start();
    begin_level();
}

static void update(const lippus_pad_t *pad) {
    int i;
    pump_music();
    if (g_state == ST_TITLE) {
        if (pad_pressed(pad, LIPPUS_A)) {
            new_game();
        }
        return;
    }
    if (g_state == ST_INTRO) {
        if (g_timer > 0) {
            g_timer--;
        }
        if (g_timer <= 0 || pad_pressed(pad, LIPPUS_A)) {
            g_state = ST_PLAY;
        }
        return;
    }
    if (g_state == ST_HURT) {
        if (g_timer > 0) {
            g_timer--;
        }
        if (g_timer <= 0) {
            refund_and_clear();
            g_state = ST_PLAY;
            g_spawn_wait = g_gap;
        }
        return;
    }
    if (g_state == ST_CLEAR) {
        if (g_timer > 0) {
            g_timer--;
        }
        if (g_timer <= 0) {
            if (g_level < 99) {
                g_level++;
            }
            begin_level();
        }
        return;
    }
    if (g_state == ST_OVER) {
        if (pad_pressed(pad, LIPPUS_A)) {
            new_game();
        }
        return;
    }
    for (i = 0; i < SLOTS; i++) {
        if (pad_pressed(pad, k_btn[i])) {
            fire(i);
        }
    }
    if (g_pop_t > 0) {
        g_pop_t--;
    }
    for (i = 0; i < SLOTS; i++) {
        tick_slot(i);
        if (g_state != ST_PLAY) {
            return;
        }
    }
    if (g_spawn_wait > 0) {
        g_spawn_wait--;
    } else if (try_spawn()) {
        g_spawn_wait = g_gap;
    }
    if (g_got >= g_quota && !bandit_visible()) {
        g_state = ST_CLEAR;
        g_timer = 50;
        snd_voice(2, SND_TRIANGLE, 523, 120, 140);
        snd_voice(3, SND_TRIANGLE, 784, 90, 200);
    }
}

static void fmt_n(char *dst, int value, int digits) {
    int i;
    if (value < 0) {
        value = 0;
    }
    for (i = digits - 1; i >= 0; i--) {
        dst[i] = (char)('0' + (value % 10));
        value /= 10;
    }
    dst[digits] = 0;
}

static int put_str(char *dst, int n, const char *s) {
    int i = 0;
    while (s[i] && n < 15) {
        dst[n++] = s[i++];
    }
    dst[n] = 0;
    return n;
}

static int put_u(char *dst, int n, int value) {
    char tmp[8];
    int k = 0;
    if (value < 0) {
        value = 0;
    }
    do {
        tmp[k++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value && k < 8);
    while (k > 0 && n < 15) {
        dst[n++] = tmp[--k];
    }
    dst[n] = 0;
    return n;
}

static void center(int y, const char *s, uint16_t color, int scale) {
    int w = gfx_text_width(s, scale);
    int x = (320 - w) / 2;
    gfx_text(x + 1, y + 1, s, LIPPUS_BLACK, scale);
    gfx_text(x, y, s, color, scale);
}

static void panel(int y, int h) {
    gfx_fill_rect(24, y, 272, h, LIPPUS_RGB(48, 26, 14));
    gfx_rect(24, y, 272, h, LIPPUS_AMBER);
}

static void draw_hole(int cx, int cy);

static void draw_doors(int step) {
    int frame;
    if (step <= 0) {
        return;
    }
    frame = door_frame(step);
    gfx_blit(ANTA_SX_X, ANTA_Y, SPR_ANTA_BUCO_W, SPR_ANTA_BUCO_H, spr_anta_buco, 1, LIPPUS_COLOR_KEY);
    gfx_blit(ANTA_SX_X, ANTA_Y, SPR_ANTA_SX0_W, SPR_ANTA_SX0_H, k_anta_sx[frame], 1, LIPPUS_COLOR_KEY);
    gfx_blit(ANTA_DX_X, ANTA_Y, SPR_ANTA_DX0_W, SPR_ANTA_DX0_H, k_anta_dx[frame], 1, LIPPUS_COLOR_KEY);
}

static void draw_actor(int i) {
    slot_t *s = &g_slot[i];
    const uint16_t *pix;
    int w;
    int h;
    int x;
    int y;
    int center;
    int from_left;
    int start;
    int end;
    int moving;
    int walking;
    if (s->phase == PH_EMPTY || s->who == WHO_NONE || s->shown <= 0) {
        return;
    }
    moving = (s->phase == PH_IN || s->phase == PH_OUT);
    walking = moving && i == 1 && ((s->shown / 4) & 1);
    actor_sprite(i, s->who, s->phase, walking, &pix, &w, &h);
    center = k_sx[i] + (k_sw[i] - w) / 2;
    from_left = (i != 2);
    start = from_left ? (k_sx[i] - w) : (k_sx[i] + k_sw[i]);
    end = from_left ? (k_sx[i] + k_sw[i]) : (k_sx[i] - w);
    if (!moving || s->tall <= 0) {
        x = center;
    } else if (s->phase == PH_IN) {
        x = start + (center - start) * s->shown / s->tall;
    } else {
        x = center + (end - center) * (s->tall - s->shown) / s->tall;
    }
    y = k_sy[i] + k_sh[i] - h;
    if (i != 1 && moving) {
        y -= (s->shown / 3) & 1;
    }
    gfx_clip(k_sx[i], k_sy[i], k_sw[i], k_sh[i]);
    gfx_blit(x, y, w, h, pix, 1, LIPPUS_COLOR_KEY);
    if (s->who == WHO_BANDIT && (s->phase == PH_HIT || s->phase == PH_BANG)) {
        int hx = x + w / 2;
        int hy = y + ((i == 1) ? (h * 2 / 5) : (h * 3 / 5));
        draw_hole(hx, hy);
    }
    gfx_clip(0, 0, 0, 0);
    if (i != 1 && s->phase == PH_ANGRY) {
        int x0 = k_sx[i] + 4;
        int y0 = k_sy[i] + 8;
        int x1 = k_sx[i] + k_sw[i] - 5;
        int y1 = k_sy[i] + k_sh[i] - 8;
        gfx_line(x0, y0, x1, y1, LIPPUS_RED);
        gfx_line(x1, y0, x0, y1, LIPPUS_RED);
    }
}

static void draw_hole(int cx, int cy) {
    gfx_fill_rect(cx - 1, cy - 1, 3, 3, LIPPUS_RGB(16, 10, 8));
    gfx_pixel(cx, cy, LIPPUS_BLACK);
    gfx_pixel(cx - 3, cy, LIPPUS_RGB(32, 18, 12));
    gfx_pixel(cx - 4, cy - 1, LIPPUS_RGB(32, 18, 12));
    gfx_pixel(cx + 3, cy, LIPPUS_RGB(32, 18, 12));
    gfx_pixel(cx + 4, cy + 1, LIPPUS_RGB(32, 18, 12));
    gfx_pixel(cx, cy - 3, LIPPUS_RGB(32, 18, 12));
    gfx_pixel(cx - 1, cy - 4, LIPPUS_RGB(32, 18, 12));
    gfx_pixel(cx + 1, cy + 3, LIPPUS_RGB(32, 18, 12));
    gfx_pixel(cx, cy + 4, LIPPUS_RGB(32, 18, 12));
}

static void draw_timer(int i) {
    slot_t *s = &g_slot[i];
    int bw;
    uint16_t col;
    if (s->phase != PH_AIM || g_aim <= 0) {
        return;
    }
    bw = (k_sw[i] * s->timer) / g_aim;
    if (bw < 1) {
        bw = 1;
    }
    col = (s->timer * 3 < g_aim) ? LIPPUS_RED : LIPPUS_AMBER;
    gfx_fill_rect(k_sx[i], k_sy[i] - 4, k_sw[i], 2, LIPPUS_RGB(20, 12, 8));
    gfx_fill_rect(k_sx[i], k_sy[i] - 4, bw, 2, col);
}

static void draw_bounty(int i) {
    char buf[8];
    int n = 0;
    int w;
    int x;
    if (g_slot[i].phase != PH_AIM) {
        return;
    }
    buf[n++] = '$';
    n = put_u(buf, n, g_slot[i].bounty);
    w = gfx_text_width(buf, 1);
    x = k_sx[i] + (k_sw[i] - w) / 2;
    gfx_text(x + 1, k_sy[i] - 13, buf, LIPPUS_BLACK, 1);
    gfx_text(x, k_sy[i] - 14, buf, LIPPUS_AMBER, 1);
    (void)n;
}

static void draw_flash(int i) {
    int cx;
    int cy;
    if (g_slot[i].flash <= 0) {
        return;
    }
    cx = k_sx[i] + k_sw[i] / 2;
    cy = k_sy[i] + k_sh[i] - 10;
    gfx_pixel(cx, cy, LIPPUS_WHITE);
    gfx_pixel(cx - 2, cy, LIPPUS_YELLOW);
    gfx_pixel(cx + 2, cy, LIPPUS_YELLOW);
    gfx_pixel(cx, cy - 2, LIPPUS_YELLOW);
}

static void draw_key(int i) {
    char label[2];
    int x;
    uint16_t col = LIPPUS_CREAM;
    slot_t *s = &g_slot[i];
    label[0] = k_key[i];
    label[1] = 0;
    x = k_sx[i] + (k_sw[i] - 8) / 2;
    if (s->phase == PH_AIM && g_aim > 0 && s->timer * 3 < g_aim && ((s->timer / 4) & 1)) {
        col = LIPPUS_RED;
    }
    gfx_text(x + 1, 227, label, LIPPUS_BLACK, 1);
    gfx_text(x, 226, label, col, 1);
}

static void draw_popup(void) {
    char buf[10];
    int n = 0;
    int w;
    int x;
    int y;
    if (g_pop_t <= 0) {
        return;
    }
    buf[n++] = '+';
    buf[n++] = '$';
    n = put_u(buf, n, g_pop_val);
    w = gfx_text_width(buf, 1);
    x = k_sx[g_pop_slot] + (k_sw[g_pop_slot] - w) / 2;
    y = k_sy[g_pop_slot] - 26;
    gfx_text(x + 1, y + 1, buf, LIPPUS_BLACK, 1);
    gfx_text(x, y, buf, LIPPUS_WHITE, 1);
    (void)n;
}

static void draw_lives(void) {
    int i;
    for (i = 0; i < 3; i++) {
        uint16_t col = (i < g_lives) ? LIPPUS_RED : LIPPUS_RGB(70, 36, 36);
        gfx_fill_rect(272 + i * 12, 6, 6, 6, col);
    }
}

static void draw_hud(void) {
    char buf[16];
    int n;
    int left;
    if (g_state == ST_TITLE) {
        return;
    }
    buf[0] = '$';
    fmt_n(buf + 1, g_score, 6);
    gfx_text(5, 5, buf, LIPPUS_BLACK, 1);
    gfx_text(4, 4, buf, LIPPUS_CREAM, 1);
    draw_lives();
    n = put_str(buf, 0, "LV ");
    put_u(buf, n, g_level);
    gfx_text(5, 17, buf, LIPPUS_BLACK, 1);
    gfx_text(4, 16, buf, LIPPUS_CREAM, 1);
    left = g_quota - g_got;
    if (left < 0) {
        left = 0;
    }
    n = put_str(buf, 0, "ANCORA ");
    put_u(buf, n, left);
    gfx_text(69, 17, buf, LIPPUS_BLACK, 1);
    gfx_text(68, 16, buf, LIPPUS_AMBER, 1);
}

static void draw_banner(void) {
    char a[16];
    char b[16];
    int n;
    if (g_state == ST_TITLE) {
        panel(58, 112);
        center(68, "SALOON", LIPPUS_CREAM, 2);
        center(96, "A  FINESTRA SINISTRA", LIPPUS_CREAM, 1);
        center(108, "B  LA PORTA", LIPPUS_CREAM, 1);
        center(120, "X  FINESTRA DESTRA", LIPPUS_AMBER, 1);
        center(146, "PREMI A", LIPPUS_AMBER, 1);
        return;
    }
    if (g_state == ST_INTRO) {
        n = put_str(a, 0, "LIVELLO ");
        put_u(a, n, g_level);
        n = put_u(b, 0, g_quota);
        put_str(b, n, " BANDITI");
        panel(78, 44);
        center(86, a, LIPPUS_CREAM, 1);
        center(102, b, LIPPUS_AMBER, 1);
        return;
    }
    if (g_state == ST_HURT || (g_state == ST_OVER && g_cause == 0) ||
        (g_state == ST_OVER && g_cause == 1)) {
        gfx_rect(0, 0, 320, 240, LIPPUS_RED);
        gfx_rect(1, 1, 318, 238, LIPPUS_RED);
    }
    if (g_state == ST_HURT) {
        panel(78, 44);
        if (g_cause == 0) {
            center(86, "COLPITO", LIPPUS_RED, 1);
            center(102, "SPARA PRIMA", LIPPUS_CREAM, 1);
        } else {
            center(86, "ERRORE", LIPPUS_RED, 1);
            center(102, "ERA UN CIVILE", LIPPUS_CREAM, 1);
        }
        return;
    }
    if (g_state == ST_CLEAR) {
        panel(84, 28);
        center(92, "LIVELLO FINITO", LIPPUS_CREAM, 1);
        return;
    }
    if (g_state == ST_OVER) {
        panel(68, 68);
        center(76, "FINE", LIPPUS_CREAM, 2);
        a[0] = '$';
        fmt_n(a + 1, g_score, 6);
        center(100, a, LIPPUS_AMBER, 1);
        center(116, "PREMI A", LIPPUS_CREAM, 1);
    }
}

static void draw(void) {
    int i;
    gfx_blit(0, 0, SPR_SFONDO_W, SPR_SFONDO_H, spr_sfondo, 0, 0);
    draw_doors(g_slot[1].step);
    for (i = 0; i < SLOTS; i++) {
        draw_actor(i);
    }
    for (i = 0; i < SLOTS; i++) {
        draw_timer(i);
        draw_bounty(i);
        draw_flash(i);
        draw_key(i);
    }
    draw_popup();
    draw_hud();
    draw_banner();
}

void lippus_main(void) {
    g_state = ST_TITLE;
    g_music_on = 1;
    g_rng = 0x51A10Eu;
    for (;;) {
        lippus_pad_t pad = pad_poll();
        update(&pad);
        draw();
        gfx_present();
        sys_frame(33);
    }
}
