#include "lippus.h"

#include "sprites.h"
#include "westfont.h"
#include "shot.h"

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
#define ST_NAME 6
/* Il bandito spara per primo: schermo dello sceriffo, poi si continua. */
#define ST_BANG 7

#define SCORE_N 20
#define RECORD_NAME "SALOON"
#define CIV_FINE 100
#define QUICK_PAY 10
#define ATTRACT_DEMO 240
#define ATTRACT_BONUS 240
#define ATTRACT_SCORE 180
#define ATTRACT_PAGE_PLAY 0
#define ATTRACT_PAGE_BONUS 1
#define ATTRACT_PAGE_SCORE 2
#define ATTRACT_PAGES 3

/* Due soli premi, una volta per partita. Oltre la seconda soglia
   non si guadagnano altre vite. */
#define LIFE_GOAL_1 10000
#define LIFE_GOAL_2 40000
#define LIFE_AWARD_MAX 2

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
#define HIT1_FRAMES 12
#define HIT2_FRAMES 12
#define POSTER_FRAMES 30
/* Circa un secondo e mezzo a sys_frame(33). */
#define BANG_FRAMES 45
#define BANG_SHOT_GAP 9

/* Bonus ogni due livelli. Il primo (g_bonus_round pari) sono gli insetti
   sulle travi. Il successivo e' il banco: il barista lancia i bicchieri. */
#define BONUS_Y 20
#define SIGHT_Y (99 + BONUS_Y)
/* Tutto il cerchio grande, non solo la croce interna. */
#define SIGHT_HIT 16
#define BONUS_TIME (15 * 30)
#define BUG_PAY 100
#define FAST_PAY 250
#define BUG_N 6

/* Banco 320x240. Il mirino e' l'anello gia' disegnato sul piano.
   Il barista (72x64) sta a destra: la mano del rilascio e' sul piano,
   e il fronte del banco da BAR_FRONT_Y in giu' gli copre le gambe. */
#define GLASS_PAY 30
/* In volo insieme, al lancio piu' fitto e col banco ancora lento. */
#define GLASS_N 12
#define BAR_SIGHT_X 114
#define BAR_SIGHT_Y 133
#define BAR_SIGHT_R 18
#define BAR_FALL_X 88
#define BAR_HAND_X 210
#define BAR_HAND_Y 133
#define BARMAN_X 198
#define BARMAN_Y 97
#define BAR_FRONT_Y 145
#define THROW_WIND 5
#define THROW_REL 5
/* Pausa casuale fra un bicchiere e il successivo, in fotogrammi.
   Il minimo e' il gesto intero: piu' stretto, il prossimo nasce
   dentro il mirino insieme a quello appena partito.
   Il massimo, nel caso peggiore, ne lascia almeno 28 nei 15 secondi. */
#define GLASS_GAP_MIN 10
#define GLASS_GAP_MAX 16

static const int k_sx[SLOTS] = {68, 137, 220};
static const int k_sy[SLOTS] = {102, 98, 102};
static const int k_sw[SLOTS] = {34, 46, 34};
static const int k_sh[SLOTS] = {58, 94, 58};
static const uint16_t k_btn[SLOTS] = {LIPPUS_A, LIPPUS_B, LIPPUS_X};
static const char k_key[SLOTS] = {'A', 'B', 'X'};
static const int k_lane_x[SLOTS] = {56, 161, 262};

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
static int g_life_awards;
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
static int g_lv_hits;
static int g_lv_cash;
static int g_lv_civs;
static int g_lv_time;
static int g_lv_quick;
static int g_after_over;
static uint32_t g_rng = 0x51A10Eu;

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

typedef struct bug {
    int alive;
    int lane;
    int kind;
    int y;
} bug_t;

static bug_t g_bug[BUG_N];
static int g_bonus;
static int g_bonus_round;
static int g_bug_wait;
static int g_fast_hits;
static int g_fast_cash;
static int g_sight_flash[SLOTS];

typedef struct glass {
    int alive;
    int x;
    int y;
    int fall;
} glass_t;

static glass_t g_glass[GLASS_N];
static int g_bar;
static int g_glass_wait;
static int g_throw_t;
static int g_bar_flash;

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

static void actor_sprite(int slot, int who, int phase, int walking, int frame, int poster,
                         const uint16_t **pix, int *w, int *h) {
    if (who == WHO_BANDIT && phase == PH_HIT) {
        if (poster) {
            if (slot == 1) {
                *pix = spr_taglia_porta;
                *w = SPR_TAGLIA_PORTA_W;
                *h = SPR_TAGLIA_PORTA_H;
            } else {
                *pix = spr_taglia_finestra;
                *w = SPR_TAGLIA_FINESTRA_W;
                *h = SPR_TAGLIA_FINESTRA_H;
            }
        } else if (slot == 1) {
            if (frame) {
                *pix = spr_bandito_porta_hit2;
                *w = SPR_BANDITO_PORTA_HIT2_W;
                *h = SPR_BANDITO_PORTA_HIT2_H;
            } else {
                *pix = spr_bandito_porta_hit1;
                *w = SPR_BANDITO_PORTA_HIT1_W;
                *h = SPR_BANDITO_PORTA_HIT1_H;
            }
        } else if (frame) {
            *pix = spr_bandito_finestra_hit2;
            *w = SPR_BANDITO_FINESTRA_HIT2_W;
            *h = SPR_BANDITO_FINESTRA_HIT2_H;
        } else {
            *pix = spr_bandito_finestra_hit1;
            *w = SPR_BANDITO_FINESTRA_HIT1_W;
            *h = SPR_BANDITO_FINESTRA_HIT1_H;
        }
        return;
    }
    if (who == WHO_WOMAN && phase == PH_ANGRY) {
        if (slot == 1) {
            if (frame) {
                *pix = spr_donna_porta_arrabbiata2;
                *w = SPR_DONNA_PORTA_ARRABBIATA2_W;
                *h = SPR_DONNA_PORTA_ARRABBIATA2_H;
            } else {
                *pix = spr_donna_porta_arrabbiata1;
                *w = SPR_DONNA_PORTA_ARRABBIATA1_W;
                *h = SPR_DONNA_PORTA_ARRABBIATA1_H;
            }
        } else if (frame) {
            *pix = spr_donna_finestra_arrabbiata2;
            *w = SPR_DONNA_FINESTRA_ARRABBIATA2_W;
            *h = SPR_DONNA_FINESTRA_ARRABBIATA2_H;
        } else {
            *pix = spr_donna_finestra_arrabbiata1;
            *w = SPR_DONNA_FINESTRA_ARRABBIATA1_W;
            *h = SPR_DONNA_FINESTRA_ARRABBIATA1_H;
        }
        return;
    }
    if (who == WHO_MAN && phase == PH_ANGRY) {
        if (slot == 1) {
            if (frame) {
                *pix = spr_uomo_porta_arrabbiato2;
                *w = SPR_UOMO_PORTA_ARRABBIATO2_W;
                *h = SPR_UOMO_PORTA_ARRABBIATO2_H;
            } else {
                *pix = spr_uomo_porta_arrabbiato1;
                *w = SPR_UOMO_PORTA_ARRABBIATO1_W;
                *h = SPR_UOMO_PORTA_ARRABBIATO1_H;
            }
        } else if (frame) {
            *pix = spr_uomo_finestra_arrabbiato2;
            *w = SPR_UOMO_FINESTRA_ARRABBIATO2_W;
            *h = SPR_UOMO_FINESTRA_ARRABBIATO2_H;
        } else {
            *pix = spr_uomo_finestra_arrabbiato1;
            *w = SPR_UOMO_FINESTRA_ARRABBIATO1_W;
            *h = SPR_UOMO_FINESTRA_ARRABBIATO1_H;
        }
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

/* Finestra per sparare, in frame. Cala di un terzo di secondo a livello.
   Al 7 resta circa 0.8 s, dall'8 in poi circa mezzo secondo. */
static int level_aim(int level) {
    int t = 90 - (level - 1) * 11;
    if (t < 16) {
        t = 16;
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

static void grant_lives(void);

static void lose_life(void) {
    if (g_state != ST_PLAY) {
        return;
    }
    g_lives--;
    /* Bandito piu' veloce: interludio, senza la carta HIT. */
    if (g_cause == 0) {
        g_state = ST_BANG;
        g_timer = BANG_FRAMES;
        return;
    }
    snd_voice(3, SND_TRIANGLE, 110, 140, 220);
    if (g_lives <= 0) {
        g_state = ST_CLEAR;
        g_timer = 0;
        g_after_over = 1;
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
    snd_play(k_shot, K_SHOT_N, 220);
    if (!shootable(s)) {
        return;
    }
    s->shown = s->tall;
    if (i == 1) {
        s->step = swing_time();
    }
    if (s->who == WHO_BANDIT) {
        int used = 0;
        if (s->phase == PH_AIM) {
            used = g_aim - s->timer;
            if (used < 0) {
                used = 0;
            }
        }
        s->phase = PH_HIT;
        s->timer = HIT1_FRAMES + HIT2_FRAMES + POSTER_FRAMES;
        g_score += s->bounty;
        if (g_score > 999999) {
            g_score = 999999;
        }
        grant_lives();
        g_got++;
        g_lv_hits++;
        g_lv_cash += s->bounty;
        g_lv_time += used;
        if (used * 2 < g_aim) {
            g_lv_quick++;
        }
        return;
    }
    g_lv_civs++;
    s->phase = PH_ANGRY;
    s->timer = 30;
    snd_voice(2, SND_TRIANGLE, 146, 170, 220);
}

static const lippus_note_t k_step[2] = {
    {110, 28},
    {64, 42},
};

static void footstep(int before, int after) {
    if (after <= 0 || (before / 16) == (after / 16)) {
        return;
    }
    snd_tune(1, SND_TRIANGLE, 72, k_step, 2);
}

static void tick_slot(int i) {
    slot_t *s = &g_slot[i];
    int sp = slide_px();
    int before;
    if (s->flash > 0) {
        s->flash--;
    }
    if (s->phase == PH_IN) {
        if (i == 1 && s->step < swing_time()) {
            s->step++;
            return;
        }
        before = s->shown;
        s->shown += sp;
        footstep(before, s->shown);
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
            /* Primo sparo dell'interludio. Gli altri partono in ST_BANG. */
            snd_play(k_shot, K_SHOT_N, 220);
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
            before = s->shown;
            s->shown -= sp;
            if (s->shown < 0) {
                s->shown = 0;
            }
            footstep(s->shown, before);
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
    g_lv_hits = 0;
    g_lv_cash = 0;
    g_lv_civs = 0;
    g_lv_time = 0;
    g_lv_quick = 0;
    g_after_over = 0;
    g_bonus = 0;
    g_bar = 0;
    g_state = ST_INTRO;
    g_timer = 36;
}

static int bug_h(int kind) {
    if (kind == 2) {
        return SPR_BUG5_1_H;
    }
    return SPR_BUG3_1_H;
}

static int bug_speed(int kind) {
    int s;
    if (kind == 2) {
        s = 2 + g_bonus_round * 2;
        if (s > 8) {
            s = 8;
        }
        return s;
    }
    s = 1 + g_bonus_round;
    if (s > 4) {
        s = 4;
    }
    return s;
}

static void begin_bonus(void) {
    int i;
    for (i = 0; i < SLOTS; i++) {
        clear_slot(&g_slot[i]);
        g_sight_flash[i] = 0;
    }
    for (i = 0; i < BUG_N; i++) {
        g_bug[i].alive = 0;
    }
    for (i = 0; i < GLASS_N; i++) {
        g_glass[i].alive = 0;
    }
    /* Pari: insetti. Dispari: il banco. g_bonus_round sale a fine bonus. */
    g_bar = g_bonus_round & 1;
    g_bonus = 1;
    g_lv_hits = 0;
    g_lv_cash = 0;
    g_lv_civs = 0;
    g_lv_time = 0;
    g_lv_quick = 0;
    g_fast_hits = 0;
    g_fast_cash = 0;
    g_after_over = 0;
    g_pop_t = 0;
    g_bug_wait = 10;
    g_glass_wait = 0;
    g_throw_t = 0;
    g_bar_flash = 0;
    g_state = ST_INTRO;
    g_timer = 50;
}

static int next_life_goal(void) {
    if (g_life_awards <= 0) {
        return LIFE_GOAL_1;
    }
    if (g_life_awards == 1) {
        return LIFE_GOAL_2;
    }
    return 0;
}

/* Controlla dopo ogni aggiornamento del punteggio. Non toglie la vita
   se poi una multa lo fa scendere sotto la soglia. */
static void grant_lives(void) {
    int gained = 0;
    while (g_life_awards < LIFE_AWARD_MAX) {
        int goal = next_life_goal();
        if (goal <= 0 || g_score < goal) {
            break;
        }
        g_life_awards++;
        g_lives++;
        gained++;
    }
    if (gained) {
        snd_voice(2, SND_TRIANGLE, 988, 110, 160);
    }
}

static void new_game(void) {
    g_score = 0;
    g_lives = 3;
    g_life_awards = 0;
    g_level = 1;
    g_rank = -1;
    g_bonus_round = 0;
    begin_level();
}

static int lane_busy(int lane) {
    int i;
    for (i = 0; i < BUG_N; i++) {
        if (g_bug[i].alive && g_bug[i].lane == lane && g_bug[i].y < 20) {
            return 1;
        }
    }
    return 0;
}

static void spawn_bug(void) {
    int tries;
    int lane;
    int slot;
    int i;
    int kind;
    slot = -1;
    for (i = 0; i < BUG_N; i++) {
        if (!g_bug[i].alive) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return;
    }
    lane = -1;
    for (tries = 0; tries < 6; tries++) {
        int pick = (int)(rnd() % 3u);
        if (!lane_busy(pick)) {
            lane = pick;
            break;
        }
    }
    if (lane < 0) {
        return;
    }
    kind = (int)(rnd() % 3u);
    g_bug[slot].alive = 1;
    g_bug[slot].lane = lane;
    g_bug[slot].kind = kind;
    g_bug[slot].y = -bug_h(kind);
}

static void bonus_shot(int lane) {
    int i;
    int best;
    int best_d;
    int kind;
    int pay;
    snd_play(k_shot, K_SHOT_N, 220);
    g_sight_flash[lane] = 5;
    best = -1;
    best_d = SIGHT_HIT + 1;
    for (i = 0; i < BUG_N; i++) {
        int h;
        int cy;
        int d;
        if (!g_bug[i].alive || g_bug[i].lane != lane) {
            continue;
        }
        h = bug_h(g_bug[i].kind);
        cy = g_bug[i].y + h / 2;
        d = cy - SIGHT_Y;
        if (d < 0) {
            d = -d;
        }
        if (d <= SIGHT_HIT && d < best_d) {
            best = i;
            best_d = d;
        }
    }
    if (best < 0) {
        return;
    }
    kind = g_bug[best].kind;
    g_bug[best].alive = 0;
    pay = (kind == 2) ? FAST_PAY : BUG_PAY;
    g_score += pay;
    if (g_score > 999999) {
        g_score = 999999;
    }
    grant_lives();
    if (kind == 2) {
        g_fast_hits++;
        g_fast_cash += pay;
    } else {
        g_lv_hits++;
        g_lv_cash += pay;
    }
    g_pop_slot = lane;
    g_pop_val = pay;
    g_pop_t = 24;
}

static void tick_bonus(const lippus_pad_t *pad) {
    int i;
    int gap;
    for (i = 0; i < SLOTS; i++) {
        if (g_sight_flash[i] > 0) {
            g_sight_flash[i]--;
        }
        if (pad_pressed(pad, k_btn[i])) {
            bonus_shot(i);
        }
    }
    if (g_pop_t > 0) {
        g_pop_t--;
    }
    for (i = 0; i < BUG_N; i++) {
        if (!g_bug[i].alive) {
            continue;
        }
        g_bug[i].y += bug_speed(g_bug[i].kind);
        if (g_bug[i].y > 240) {
            g_bug[i].alive = 0;
        }
    }
    if (g_timer < BONUS_TIME - 45) {
        if (g_bug_wait > 0) {
            g_bug_wait--;
        } else {
            spawn_bug();
            if (g_bonus_round > 0 && (rnd() & 1u)) {
                spawn_bug();
            }
            gap = 20 - g_bonus_round * 3;
            if (gap < 8) {
                gap = 8;
            }
            g_bug_wait = gap;
        }
    }
    g_timer++;
    if (g_timer >= BONUS_TIME) {
        g_state = ST_CLEAR;
        g_timer = 0;
        g_after_over = 0;
        snd_voice(2, SND_TRIANGLE, 523, 120, 140);
        snd_voice(3, SND_TRIANGLE, 784, 90, 200);
    }
}

/* Il primo banco e' g_bonus_round 1: indice 0, lento. Poi accelera. */
static int glass_speed(void) {
    int s = 2 + g_bonus_round / 2;
    if (s > 6) {
        s = 6;
    }
    return s;
}

static int glass_gap(void) {
    int span;
    span = GLASS_GAP_MAX - GLASS_GAP_MIN + 1;
    return GLASS_GAP_MIN + (int)(rnd() % (unsigned)span);
}

static int spawn_glass(void) {
    int i;
    for (i = 0; i < GLASS_N; i++) {
        if (g_glass[i].alive) {
            continue;
        }
        g_glass[i].alive = 1;
        g_glass[i].x = BAR_HAND_X;
        g_glass[i].y = BAR_HAND_Y;
        g_glass[i].fall = 0;
        return 1;
    }
    return 0;
}

static void bar_shot(void) {
    int i;
    int best;
    int best_d;
    int limit;
    snd_play(k_shot, K_SHOT_N, 220);
    g_bar_flash = 5;
    best = -1;
    limit = BAR_SIGHT_R * BAR_SIGHT_R;
    best_d = limit + 1;
    for (i = 0; i < GLASS_N; i++) {
        int dx;
        int dy;
        int d;
        if (!g_glass[i].alive || g_glass[i].fall) {
            continue;
        }
        dx = g_glass[i].x - BAR_SIGHT_X;
        dy = g_glass[i].y - BAR_SIGHT_Y;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        d = dx * dx + dy * dy;
        if (d <= limit && d < best_d) {
            best = i;
            best_d = d;
        }
    }
    if (best < 0) {
        return;
    }
    g_glass[best].alive = 0;
    g_score += GLASS_PAY;
    if (g_score > 999999) {
        g_score = 999999;
    }
    grant_lives();
    g_lv_hits++;
    g_lv_cash += GLASS_PAY;
    g_pop_val = GLASS_PAY;
    g_pop_t = 24;
}

static void tick_bar(const lippus_pad_t *pad) {
    int i;
    int speed;
    int prev;
    if (g_bar_flash > 0) {
        g_bar_flash--;
    }
    if (g_pop_t > 0) {
        g_pop_t--;
    }
    if (pad_pressed(pad, LIPPUS_A)) {
        bar_shot();
    }
    if (pad_pressed(pad, LIPPUS_B)) {
        bar_shot();
    }
    if (pad_pressed(pad, LIPPUS_X)) {
        bar_shot();
    }
    speed = glass_speed();
    for (i = 0; i < GLASS_N; i++) {
        if (!g_glass[i].alive) {
            continue;
        }
        if (g_glass[i].fall) {
            g_glass[i].x -= 1;
            g_glass[i].y += speed + 2;
            if (g_glass[i].y > 240) {
                g_glass[i].alive = 0;
            }
        } else {
            g_glass[i].x -= speed;
            if (g_glass[i].x < BAR_FALL_X) {
                g_glass[i].fall = 1;
            }
        }
    }
    if (g_glass_wait > 0) {
        g_glass_wait--;
    }
    if (g_throw_t > 0) {
        prev = g_throw_t;
        g_throw_t--;
        if (prev == THROW_REL) {
            spawn_glass();
        }
    }
    /* Stesso fotogramma in cui il gesto si chiude: la pausa minima
       resta il lancio intero, senza un fotogramma vuoto in mezzo. */
    if (g_throw_t == 0 && g_glass_wait == 0) {
        g_throw_t = THROW_WIND + THROW_REL;
        g_glass_wait = glass_gap();
    }
    /* Quindici secondi, come gli insetti. Il bicchiere ancora sul banco
       non allunga il round: se non e' stato colpito, non paga. */
    g_timer++;
    if (g_timer >= BONUS_TIME) {
        g_throw_t = 0;
        g_state = ST_CLEAR;
        g_timer = 0;
        g_after_over = 0;
        snd_voice(2, SND_TRIANGLE, 523, 120, 140);
        snd_voice(3, SND_TRIANGLE, 784, 90, 200);
    }
}

static int score_qualifies(int score);
static void enter_name(void);
static void enter_attract(int page);
static void tick_name(const lippus_pad_t *pad);

static void update(const lippus_pad_t *pad) {
    int i;
    int span;
    g_attract_t++;
    if (g_state == ST_TITLE) {
        if (pad_pressed(pad, LIPPUS_START)) {
            new_game();
            return;
        }
        span = ATTRACT_DEMO;
        if (g_attract_page == ATTRACT_PAGE_BONUS) {
            span = ATTRACT_BONUS;
        } else if (g_attract_page == ATTRACT_PAGE_SCORE) {
            span = ATTRACT_SCORE;
        }
        if (g_attract_t >= span) {
            g_attract_t = 0;
            g_attract_page++;
            if (g_attract_page >= ATTRACT_PAGES) {
                g_attract_page = 0;
            }
        }
        return;
    }
    if (g_state == ST_INTRO) {
        if (g_timer > 0) {
            g_timer--;
        }
        if (g_timer <= 0 || pad_pressed(pad, LIPPUS_A)) {
            g_state = ST_PLAY;
            if (g_bonus) {
                g_timer = 0;
            }
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
    if (g_state == ST_BANG) {
        if (g_timer > 0) {
            g_timer--;
        }
        /* Il colpo del bandito e' il primo. Gli altri ogni 9 frame. */
        if (g_timer > 0 && (g_timer % BANG_SHOT_GAP) == 0) {
            snd_play(k_shot, K_SHOT_N, 220);
        }
        if (g_timer <= 0) {
            if (g_lives <= 0) {
                g_state = ST_CLEAR;
                g_timer = 0;
                g_after_over = 1;
            } else {
                refund_and_clear();
                g_state = ST_PLAY;
                g_spawn_wait = g_gap;
            }
        }
        return;
    }
    if (g_state == ST_CLEAR) {
        g_timer++;
        if (g_timer >= (g_bonus ? 130 : 190) || pad_pressed(pad, LIPPUS_A)) {
            if (!g_bonus) {
                int end = g_score - g_lv_civs * CIV_FINE + g_lv_quick * QUICK_PAY;
                if (end < 0) {
                    end = 0;
                }
                if (end > 999999) {
                    end = 999999;
                }
                g_score = end;
                grant_lives();
                /* La vita vinta sul riepilogo annulla il game over. */
                if (g_lives > 0) {
                    g_after_over = 0;
                }
            }
            if (g_after_over) {
                g_state = ST_OVER;
                g_timer = 90;
            } else if (g_bonus) {
                g_bonus = 0;
                if (g_bonus_round < 20) {
                    g_bonus_round++;
                }
                if (g_level < 99) {
                    g_level++;
                }
                begin_level();
            } else if ((g_level % 2) == 0) {
                begin_bonus();
            } else {
                if (g_level < 99) {
                    g_level++;
                }
                begin_level();
            }
        }
        return;
    }
    if (g_state == ST_OVER) {
        if (g_timer > 0) {
            g_timer--;
        }
        if (g_timer <= 0 || pad_pressed(pad, LIPPUS_A)) {
            if (score_qualifies(g_score)) {
                enter_name();
            } else {
                g_rank = -1;
                enter_attract(ATTRACT_PAGE_SCORE);
            }
        }
        return;
    }
    if (g_state == ST_NAME) {
        tick_name(pad);
        return;
    }
    if (g_bonus) {
        if (g_bar) {
            tick_bar(pad);
        } else {
            tick_bonus(pad);
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
        g_timer = 0;
        g_after_over = 0;
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

static void west_text(int x, int y, const char *s, uint16_t color, int scale) {
    int i = 0;
    if (scale < 1) {
        scale = 1;
    }
    while (s[i]) {
        int code = (unsigned char)s[i];
        int row;
        int col;
        if (code >= 32 && code <= 90) {
            const uint8_t *g = k_west[code - 32];
            for (row = 0; row < 8; row++) {
                uint8_t bits = g[row];
                for (col = 0; col < 8; col++) {
                    if (bits & (1u << col)) {
                        gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
        x += 8 * scale;
        i++;
    }
}

static int west_width(const char *s, int scale) {
    int n = 0;
    if (scale < 1) {
        scale = 1;
    }
    while (s[n]) {
        n++;
    }
    return n * 8 * scale;
}

static void center(int y, const char *s, uint16_t color, int scale) {
    int w = west_width(s, scale);
    int x = (320 - w) / 2;
    west_text(x + scale, y + scale, s, LIPPUS_BLACK, scale);
    west_text(x, y, s, color, scale);
}

static void panel(int y, int h) {
    gfx_fill_rect(24, y, 272, h, LIPPUS_RGB(48, 26, 14));
    gfx_rect(24, y, 272, h, LIPPUS_AMBER);
}

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
    int frame;
    int poster;
    if (s->phase == PH_EMPTY || s->who == WHO_NONE || s->shown <= 0) {
        return;
    }
    moving = (s->phase == PH_IN || s->phase == PH_OUT);
    walking = moving && i == 1 && ((s->shown / 4) & 1);
    poster = (s->who == WHO_BANDIT && s->phase == PH_HIT && s->timer <= POSTER_FRAMES);
    if (s->who == WHO_BANDIT && s->phase == PH_HIT) {
        frame = s->timer <= POSTER_FRAMES + HIT2_FRAMES;
    } else {
        frame = (s->timer / 4) & 1;
    }
    actor_sprite(i, s->who, s->phase, walking, frame, poster, &pix, &w, &h);
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
    if (poster) {
        char buf[8];
        int n = 0;
        int tw;
        int tx;
        int ty;
        buf[n++] = '$';
        n = put_u(buf, n, s->bounty);
        tw = west_width(buf, 1);
        tx = x + (w - tw) / 2;
        ty = y + h - ((i == 1) ? 12 : 10);
        west_text(tx + 1, ty + 1, buf, LIPPUS_BLACK, 1);
        west_text(tx, ty, buf, LIPPUS_YELLOW, 1);
        (void)n;
    }
    gfx_clip(0, 0, 0, 0);
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
    w = west_width(buf, 1);
    x = k_sx[i] + (k_sw[i] - w) / 2;
    west_text(x + 1, k_sy[i] - 13, buf, LIPPUS_BLACK, 1);
    west_text(x, k_sy[i] - 14, buf, LIPPUS_AMBER, 1);
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
    west_text(x + 1, 227, label, LIPPUS_BLACK, 1);
    west_text(x, 226, label, col, 1);
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
    w = west_width(buf, 1);
    if (g_bar) {
        x = BAR_SIGHT_X - w / 2;
        y = BAR_SIGHT_Y - 24;
    } else if (g_bonus) {
        x = k_lane_x[g_pop_slot] - w / 2;
        y = SIGHT_Y - 24;
    } else {
        x = k_sx[g_pop_slot] + (k_sw[g_pop_slot] - w) / 2;
        y = k_sy[g_pop_slot] - 26;
    }
    west_text(x + 1, y + 1, buf, LIPPUS_BLACK, 1);
    west_text(x, y, buf, LIPPUS_WHITE, 1);
    (void)n;
}

static void draw_gun(int x, int y, int alive) {
    int row;
    int col;
    for (row = 0; row < SPR_PISTOLA_H; row++) {
        for (col = 0; col < SPR_PISTOLA_W; col++) {
            uint16_t c = spr_pistola[row * SPR_PISTOLA_W + col];
            int r;
            int g;
            int b;
            if (c == LIPPUS_COLOR_KEY) {
                continue;
            }
            if (!alive) {
                r = ((c >> 11) & 31) / 3;
                g = ((c >> 5) & 63) / 3;
                b = (c & 31) / 3;
                c = (uint16_t)((r << 11) | (g << 5) | b);
            }
            gfx_pixel(x + col, y + row, c);
        }
    }
}

static void draw_lives(void) {
    int i;
    int gap = 4;
    int span = SPR_PISTOLA_W * 3 + gap * 2;
    int x0 = (320 - span) / 2;
    for (i = 0; i < 3; i++) {
        draw_gun(x0 + i * (SPR_PISTOLA_W + gap), 0, i < g_lives);
    }
    if (g_lives > 3) {
        char extra[3];
        extra[0] = '+';
        extra[1] = (char)('0' + (g_lives - 3));
        extra[2] = 0;
        west_text(x0 + span + 2, 4, extra, LIPPUS_CREAM, 1);
    }
}

static int shown_num(int start, int span, int target) {
    int t;
    if (g_timer < start) {
        return -1;
    }
    t = g_timer - start;
    if (span <= 0 || t >= span) {
        return target;
    }
    return target * t / span;
}

static int live_score(void) {
    int pen;
    int bon;
    int s;
    if (g_state != ST_CLEAR) {
        return g_score;
    }
    pen = shown_num(44, 28, g_lv_civs * CIV_FINE);
    bon = shown_num(116, 28, g_lv_quick * QUICK_PAY);
    if (pen < 0) {
        pen = 0;
    }
    if (bon < 0) {
        bon = 0;
    }
    s = g_score - pen + bon;
    if (s < 0) {
        s = 0;
    }
    if (s > 999999) {
        s = 999999;
    }
    return s;
}

static int avg_tenths(void) {
    if (g_lv_hits <= 0) {
        return 0;
    }
    return g_lv_time * 10 / (g_lv_hits * 30);
}

static void money_str(char *dst, int sign, int value) {
    int n = 0;
    if (sign < 0) {
        dst[n++] = '-';
    } else if (sign > 0) {
        dst[n++] = '+';
    }
    dst[n++] = '$';
    put_u(dst, n, value);
}

static void draw_sum_row(int y, const char *label, int count, const char *extra) {
    char num[8];
    put_u(num, 0, count);
    west_text(44, y, label, LIPPUS_CREAM, 1);
    west_text(148, y, num, LIPPUS_AMBER, 1);
    if (extra) {
        west_text(188, y, extra, LIPPUS_AMBER, 1);
    }
}

static void draw_hud(void) {
    char buf[16];
    int n;
    int w;
    int shown;
    int goal;
    if (g_state == ST_TITLE || g_state == ST_NAME) {
        return;
    }
    shown = live_score();
    n = put_u(buf, 0, shown);
    goal = next_life_goal();
    if (goal > 0) {
        buf[n++] = '/';
        put_u(buf, n, goal);
    }
    west_text(5, 5, buf, LIPPUS_BLACK, 1);
    west_text(4, 4, buf, LIPPUS_CREAM, 1);
    draw_lives();
    if (g_bonus) {
        n = put_str(buf, 0, "BONUS");
    } else {
        n = put_str(buf, 0, "LEVEL ");
        put_u(buf, n, g_level);
    }
    w = west_width(buf, 1);
    west_text(321 - w, 5, buf, LIPPUS_BLACK, 1);
    west_text(320 - w, 4, buf, LIPPUS_CREAM, 1);
}

static void blit_fit(int slot, const uint16_t *pix, int w, int h) {
    int x = k_sx[slot] + (k_sw[slot] - w) / 2;
    int y = k_sy[slot] + k_sh[slot] - h;
    gfx_clip(k_sx[slot], k_sy[slot], k_sw[slot], k_sh[slot]);
    gfx_blit(x, y, w, h, pix, 1, LIPPUS_COLOR_KEY);
    gfx_clip(0, 0, 0, 0);
}

static void draw_demo(void) {
    int beat = (g_attract_t / 60) % 4;
    const char *line = "A LEFT   B DOOR   X RIGHT";
    panel(8, 28);
    center(14, "SALOON", LIPPUS_CREAM, 1);
    if (beat == 1) {
        line = "SHOOT THE OUTLAW";
        blit_fit(1, spr_bandito_porta, SPR_BANDITO_PORTA_W, SPR_BANDITO_PORTA_H);
    } else if (beat == 2) {
        line = "SPARE THE TOWNSFOLK";
        blit_fit(0, spr_donna_finestra, SPR_DONNA_FINESTRA_W, SPR_DONNA_FINESTRA_H);
        blit_fit(2, spr_uomo_finestra, SPR_UOMO_FINESTRA_W, SPR_UOMO_FINESTRA_H);
    } else if (beat == 3) {
        line = "A FAST SHOT PAYS $10";
        blit_fit(1, spr_taglia_porta, SPR_TAGLIA_PORTA_W, SPR_TAGLIA_PORTA_H);
    }
    panel(196, 22);
    center(202, line, LIPPUS_AMBER, 1);
    if ((g_attract_t & 16) == 0) {
        center(222, "PRESS START", LIPPUS_CREAM, 1);
    }
}

static void draw_score_row(int x, int y, int rank, const score_t *row, uint16_t color) {
    char pos[4];
    char name[4];
    char num[8];
    put_u(pos, 0, rank + 1);
    name[0] = row->name[0];
    name[1] = row->name[1];
    name[2] = row->name[2];
    name[3] = 0;
    put_u(num, 0, row->score);
    west_text(x, y, pos, color, 1);
    west_text(x + 28, y, name, color, 1);
    west_text(x + 112 - west_width(num, 1), y, num, color, 1);
}

static void draw_scores(void) {
    int i;
    panel(4, 232);
    center(12, "TOP 20", LIPPUS_AMBER, 1);
    if (g_score_n == 0) {
        center(100, "NO SCORES YET", LIPPUS_CREAM, 1);
    } else {
        for (i = 0; i < g_score_n; i++) {
            uint16_t color = (i == g_rank) ? LIPPUS_YELLOW : LIPPUS_CREAM;
            int col = i / 10;
            int row = i % 10;
            draw_score_row(36 + col * 136, 32 + row * 18, i, &g_scores[i], color);
        }
    }
    if ((g_attract_t & 16) == 0) {
        center(218, "PRESS START", LIPPUS_CREAM, 1);
    }
}

static void draw_bonus_summary(void) {
    int hits = shown_num(8, 28, g_lv_hits);
    int cash = shown_num(8, 28, g_lv_cash);
    int fast = shown_num(44, 28, g_fast_hits);
    int pay = shown_num(44, 28, g_fast_cash);
    char money[12];
    if (g_bar) {
        panel(70, 48);
        center(76, "BONUS CLEAR", LIPPUS_AMBER, 1);
        if (hits >= 0) {
            money_str(money, 0, cash);
            draw_sum_row(94, "GLASSES", hits, money);
        }
        return;
    }
    panel(70, 64);
    center(76, "BONUS CLEAR", LIPPUS_AMBER, 1);
    if (hits >= 0) {
        money_str(money, 0, cash);
        draw_sum_row(94, "BUGS", hits, money);
    }
    if (fast >= 0) {
        money_str(money, 0, pay);
        draw_sum_row(110, "FAST", fast, money);
    }
}

static void draw_summary(void) {
    int hits = shown_num(8, 28, g_lv_hits);
    int cash = shown_num(8, 28, g_lv_cash);
    int civs = shown_num(44, 28, g_lv_civs);
    int fine = shown_num(44, 28, g_lv_civs * CIV_FINE);
    int tenths = shown_num(80, 28, avg_tenths());
    int quick = shown_num(116, 28, g_lv_quick);
    int bonus = shown_num(116, 28, g_lv_quick * QUICK_PAY);
    char money[12];
    char clock[8];
    panel(62, 112);
    center(68, g_after_over ? "TALLY" : "LEVEL CLEAR", LIPPUS_AMBER, 1);
    if (hits >= 0) {
        money_str(money, 0, cash);
        draw_sum_row(86, "OUTLAWS", hits, money);
    }
    if (civs >= 0) {
        money_str(money, -1, fine);
        draw_sum_row(102, "CIVILIANS", civs, money);
    }
    if (tenths >= 0) {
        if (g_lv_hits <= 0) {
            clock[0] = '-';
            clock[1] = '-';
            clock[2] = 0;
        } else {
            clock[0] = (char)('0' + tenths / 10);
            clock[1] = '.';
            clock[2] = (char)('0' + tenths % 10);
            clock[3] = 'S';
            clock[4] = 0;
        }
        west_text(44, 118, "AVG TIME", LIPPUS_CREAM, 1);
        west_text(148, 118, clock, LIPPUS_AMBER, 1);
    }
    if (quick >= 0) {
        money_str(money, 1, bonus);
        draw_sum_row(134, "QUICK", quick, money);
    }
}

static void draw_name(void) {
    char shown[4];
    char num[8];
    int x;
    int i;
    put_u(num, 0, g_score);
    panel(36, 150);
    center(44, "NEW HIGH SCORE", LIPPUS_AMBER, 1);
    center(62, num, LIPPUS_CREAM, 1);
    center(84, "YOUR NAME", LIPPUS_CREAM, 1);
    shown[0] = g_initials[0];
    shown[1] = g_initials[1];
    shown[2] = g_initials[2];
    shown[3] = 0;
    center(108, shown, LIPPUS_YELLOW, 2);
    if ((g_attract_t & 8) == 0) {
        char one[2];
        x = (320 - west_width(shown, 2)) / 2;
        one[1] = 0;
        for (i = 0; i < g_name_i; i++) {
            one[0] = shown[i];
            x += west_width(one, 2);
        }
        one[0] = shown[g_name_i];
        gfx_fill_rect(x, 128, west_width(one, 2), 3, LIPPUS_YELLOW);
    }
    center(148, "UP DOWN", LIPPUS_CREAM, 1);
    center(162, "A ENTERS", LIPPUS_CREAM, 1);
}

static void draw_banner(void) {
    char a[16];
    char b[16];
    int n;
    if (g_state == ST_TITLE) {
        if (g_attract_page == ATTRACT_PAGE_SCORE) {
            draw_scores();
        } else {
            draw_demo();
        }
        return;
    }
    if (g_state == ST_NAME) {
        draw_name();
        return;
    }
    if (g_state == ST_INTRO) {
        panel(78, 44);
        if (g_bonus) {
            center(86, "BONUS LEVEL", LIPPUS_CREAM, 1);
            if (g_bar) {
                center(102, "SHOOT THE GLASSES", LIPPUS_AMBER, 1);
            } else {
                center(102, "SHOOT THE BUGS", LIPPUS_AMBER, 1);
            }
        } else {
            n = put_str(a, 0, "LEVEL ");
            put_u(a, n, g_level);
            n = put_u(b, 0, g_quota);
            put_str(b, n, " OUTLAWS");
            center(86, a, LIPPUS_CREAM, 1);
            center(102, b, LIPPUS_AMBER, 1);
        }
        return;
    }
    if (g_state == ST_HURT) {
        gfx_rect(0, 0, 320, 240, LIPPUS_RED);
        gfx_rect(1, 1, 318, 238, LIPPUS_RED);
        panel(78, 44);
        if (g_cause == 0) {
            center(86, "HIT", LIPPUS_RED, 1);
            center(102, "SHOOT FIRST", LIPPUS_CREAM, 1);
        } else {
            center(86, "WRONG", LIPPUS_RED, 1);
            center(102, "THAT WAS A CIVILIAN", LIPPUS_CREAM, 1);
        }
        return;
    }
    if (g_state == ST_CLEAR) {
        if (g_bonus) {
            draw_bonus_summary();
        } else {
            draw_summary();
        }
        return;
    }
    if (g_state == ST_OVER) {
        panel(68, 52);
        center(76, "GAME OVER", LIPPUS_CREAM, 1);
        a[0] = '$';
        fmt_n(a + 1, g_score, 6);
        center(96, a, LIPPUS_AMBER, 1);
    }
}

static void bug_sprite(int kind, int frame, const uint16_t **pix, int *w, int *h) {
    if (kind == 2) {
        *pix = frame ? spr_bug5_2 : spr_bug5_1;
        *w = SPR_BUG5_1_W;
        *h = SPR_BUG5_1_H;
        return;
    }
    if (kind == 1) {
        *pix = frame ? spr_bug4_2 : spr_bug4_1;
        *w = SPR_BUG4_1_W;
        *h = SPR_BUG4_1_H;
        return;
    }
    *pix = frame ? spr_bug3_2 : spr_bug3_1;
    *w = SPR_BUG3_1_W;
    *h = SPR_BUG3_1_H;
}

static void draw_bonus(void) {
    int i;
    int frame;
    int left;
    int bw;
    gfx_fill_rect(0, 0, 320, 240, LIPPUS_RGB(70, 22, 12));
    gfx_blit(0, BONUS_Y, SPR_INTERNO_W, SPR_INTERNO_H, spr_interno, 0, 0);
    if (BONUS_Y + SPR_INTERNO_H < 240) {
        gfx_fill_rect(0, BONUS_Y + SPR_INTERNO_H, 320, 240 - (BONUS_Y + SPR_INTERNO_H),
                      LIPPUS_RGB(120, 48, 16));
    }
    frame = (g_timer / 5) & 1;
    for (i = 0; i < BUG_N; i++) {
        const uint16_t *pix;
        int w;
        int h;
        if (!g_bug[i].alive || g_state != ST_PLAY) {
            continue;
        }
        bug_sprite(g_bug[i].kind, frame, &pix, &w, &h);
        gfx_blit(k_lane_x[g_bug[i].lane] - w / 2, g_bug[i].y, w, h, pix, 1, LIPPUS_COLOR_KEY);
    }
    for (i = 0; i < SLOTS; i++) {
        int cx;
        int cy;
        if (g_sight_flash[i] <= 0) {
            continue;
        }
        cx = k_lane_x[i];
        cy = SIGHT_Y;
        gfx_pixel(cx, cy, LIPPUS_WHITE);
        gfx_pixel(cx - 2, cy, LIPPUS_YELLOW);
        gfx_pixel(cx + 2, cy, LIPPUS_YELLOW);
        gfx_pixel(cx, cy - 2, LIPPUS_YELLOW);
        gfx_pixel(cx, cy + 2, LIPPUS_YELLOW);
    }
    if (g_state == ST_PLAY) {
        left = BONUS_TIME - g_timer;
        if (left < 0) {
            left = 0;
        }
        bw = left * 304 / BONUS_TIME;
        gfx_fill_rect(8, 232, 304, 3, LIPPUS_RGB(40, 20, 12));
        if (bw > 0) {
            gfx_fill_rect(8, 232, bw, 3, LIPPUS_AMBER);
        }
        if (g_timer < 70) {
            center(208, "BONUS LEVEL", LIPPUS_YELLOW, 1);
        }
    }
}

static void draw_bar(void) {
    int i;
    int frame;
    const uint16_t *man;
    int mw;
    int mh;
    int left;
    int bw;
    gfx_blit(0, 0, SPR_INTERNOBAR_W, SPR_INTERNOBAR_H, spr_internobar, 0, 0);
    frame = g_throw_t > 0 && g_throw_t <= THROW_REL;
    if (frame) {
        man = spr_barrista2;
        mw = SPR_BARRISTA2_W;
        mh = SPR_BARRISTA2_H;
    } else {
        man = spr_barrista1;
        mw = SPR_BARRISTA1_W;
        mh = SPR_BARRISTA1_H;
    }
    gfx_blit(BARMAN_X, BARMAN_Y, mw, mh, man, 1, LIPPUS_COLOR_KEY);
    /* Il fronte del banco copre le gambe: e' dietro al bancone. */
    gfx_clip(0, BAR_FRONT_Y, 320, 240 - BAR_FRONT_Y);
    gfx_blit(0, 0, SPR_INTERNOBAR_W, SPR_INTERNOBAR_H, spr_internobar, 0, 0);
    gfx_clip(0, 0, 0, 0);
    for (i = 0; i < GLASS_N; i++) {
        if (!g_glass[i].alive) {
            continue;
        }
        gfx_blit(g_glass[i].x - SPR_BICCHIERE_W / 2, g_glass[i].y - SPR_BICCHIERE_H / 2,
                 SPR_BICCHIERE_W, SPR_BICCHIERE_H, spr_bicchiere, 1, LIPPUS_COLOR_KEY);
    }
    if (g_bar_flash > 0) {
        gfx_pixel(BAR_SIGHT_X, BAR_SIGHT_Y, LIPPUS_WHITE);
        gfx_pixel(BAR_SIGHT_X - 2, BAR_SIGHT_Y, LIPPUS_YELLOW);
        gfx_pixel(BAR_SIGHT_X + 2, BAR_SIGHT_Y, LIPPUS_YELLOW);
        gfx_pixel(BAR_SIGHT_X, BAR_SIGHT_Y - 2, LIPPUS_YELLOW);
        gfx_pixel(BAR_SIGHT_X, BAR_SIGHT_Y + 2, LIPPUS_YELLOW);
    }
    if (g_state == ST_PLAY) {
        left = BONUS_TIME - g_timer;
        if (left < 0) {
            left = 0;
        }
        bw = left * 304 / BONUS_TIME;
        gfx_fill_rect(8, 232, 304, 3, LIPPUS_RGB(40, 20, 12));
        if (bw > 0) {
            gfx_fill_rect(8, 232, bw, 3, LIPPUS_AMBER);
        }
    }
}

/* Pagina del carosello: prima gli insetti, poi il banco.
   Le due soglie delle vite stanno scritte sotto, sono le stesse del gioco. */
static void draw_attract_bonus(void) {
    int bugs = g_attract_t < (ATTRACT_BONUS / 2);
    int frame = (g_attract_t / 5) & 1;
    int i;
    if (bugs) {
        gfx_fill_rect(0, 0, 320, 240, LIPPUS_RGB(70, 22, 12));
        gfx_blit(0, BONUS_Y, SPR_INTERNO_W, SPR_INTERNO_H, spr_interno, 0, 0);
        if (BONUS_Y + SPR_INTERNO_H < 240) {
            gfx_fill_rect(0, BONUS_Y + SPR_INTERNO_H, 320, 240 - (BONUS_Y + SPR_INTERNO_H),
                          LIPPUS_RGB(120, 48, 16));
        }
        for (i = 0; i < SLOTS; i++) {
            const uint16_t *pix;
            int w;
            int h;
            bug_sprite(i, frame, &pix, &w, &h);
            gfx_blit(k_lane_x[i] - w / 2, SIGHT_Y - h / 2, w, h, pix, 1, LIPPUS_COLOR_KEY);
        }
    } else {
        const uint16_t *man;
        int mw;
        int mh;
        gfx_blit(0, 0, SPR_INTERNOBAR_W, SPR_INTERNOBAR_H, spr_internobar, 0, 0);
        if (frame) {
            man = spr_barrista2;
            mw = SPR_BARRISTA2_W;
            mh = SPR_BARRISTA2_H;
        } else {
            man = spr_barrista1;
            mw = SPR_BARRISTA1_W;
            mh = SPR_BARRISTA1_H;
        }
        gfx_blit(BARMAN_X, BARMAN_Y, mw, mh, man, 1, LIPPUS_COLOR_KEY);
        gfx_blit(BAR_SIGHT_X - SPR_BICCHIERE_W / 2, BAR_SIGHT_Y - SPR_BICCHIERE_H / 2,
                 SPR_BICCHIERE_W, SPR_BICCHIERE_H, spr_bicchiere, 1, LIPPUS_COLOR_KEY);
    }
    panel(4, 22);
    center(8, bugs ? "EVERY 2 LEVELS" : "THEN THE BAR", LIPPUS_CREAM, 1);
    panel(164, 52);
    if (bugs) {
        center(170, "SHOOT BUGS IN THE SIGHT", LIPPUS_AMBER, 1);
        center(186, "A FAST ONE PAYS MORE", LIPPUS_CREAM, 1);
    } else {
        center(170, "SHOOT THE GLASSES", LIPPUS_AMBER, 1);
        center(186, "BEFORE THEY FALL", LIPPUS_CREAM, 1);
    }
    center(202, "10000 THEN 40000 WINS A LIFE", LIPPUS_YELLOW, 1);
    if ((g_attract_t & 16) == 0) {
        center(222, "PRESS START", LIPPUS_CREAM, 1);
    }
}

static void draw(void) {
    int i;
    int step = g_slot[1].step;
    if (g_state == ST_TITLE && g_attract_page == ATTRACT_PAGE_BONUS) {
        draw_attract_bonus();
        return;
    }
    if (g_state == ST_BANG) {
        gfx_blit(0, 0, SPR_SCERIFFO_W, SPR_SCERIFFO_H, spr_sceriffo, 0, 0);
        return;
    }
    if (g_bonus) {
        if (g_bar) {
            draw_bar();
        } else {
            draw_bonus();
        }
        draw_popup();
        draw_hud();
        draw_banner();
        return;
    }
    gfx_blit(0, 0, SPR_SFONDO_W, SPR_SFONDO_H, spr_sfondo, 0, 0);
    if (g_state == ST_TITLE && g_attract_page == 0 && (g_attract_t / 60) % 4 == 1) {
        step = 99;
    }
    draw_doors(step);
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
        put_u(num, 0, g_scores[i].score);
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

static void enter_attract(int page) {
    g_state = ST_TITLE;
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
        enter_attract(ATTRACT_PAGE_SCORE);
    }
}

void lippus_main(void) {
    g_state = ST_TITLE;
    g_rank = -1;
    g_rng = 0x51A10Eu;
    scores_load();
    for (;;) {
        lippus_pad_t pad = pad_poll();
        update(&pad);
        draw();
        gfx_present();
        sys_frame(33);
    }
}
