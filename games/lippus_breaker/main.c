#include "lippus.h"

/* Vetrata in orizzontale. La spatola guida una perla di vetro:
   le tessere si staccano, il piombo resta. Le gocce di pigmento
   allargano la spatola, rallentano la perla, scheggiano i vicini,
   sdoppiano la perla o ridanno una vita.
   L'anteprima e la classifica sono come Maze Mouse: fasi illustrate,
   poi venti nomi. Start parte la partita, A lancia la perla. */

#define COLS 12
#define ROWS 8
#define BRICK_W 24
#define BRICK_H 10
#define PITCH_X 26
#define PITCH_Y 12
#define ORIGIN_X 4
#define ORIGIN_Y 24
#define BALL_R 3
#define BALLS 3
#define DROPS 4
#define PAD_W 46
#define PAD_WIDE 74
#define PAD_H 6
#define PAD_Y 214
#define MIN_X (4 + BALL_R)
#define MAX_X (LIPPUS_SCREEN_W - 4 - BALL_R)
#define MIN_Y (20 + BALL_R)
#define RECORD_NAME "BREAKER"
#define SCORE_N 20
#define LEVELS 20
#define ATTRACT_LEN 420
#define SPEED_CAP 7
#define SPEED_WAIT 300

#define ST_ATTRACT 0
#define ST_WAIT 1
#define ST_READY 2
#define ST_PLAY 3
#define ST_CLEAR 4
#define ST_OVER 5
#define ST_NAME 6

#define K_EMPTY 0
#define K_COBALT 1
#define K_RUBY 2
#define K_EMERALD 3
#define K_AMBER 4
#define K_VIOLET 5
#define K_MORTAR 6
#define K_STONE 7
#define K_GOLD 8
#define K_WELL 9
#define K_LEAD 10

#define DROP_WIDE 1
#define DROP_SLOW 2
#define DROP_CHISEL 3
#define DROP_TWIN 4
#define DROP_LIFE 5

#define COL_NIGHT LIPPUS_RGB(8, 10, 26)
#define COL_HUD LIPPUS_RGB(28, 16, 26)
#define COL_LEAD LIPPUS_RGB(24, 26, 38)
#define COL_LEAD_HI LIPPUS_RGB(78, 80, 98)
#define COL_SILL LIPPUS_RGB(18, 14, 24)
#define COL_HURT LIPPUS_RGB(72, 18, 32)
#define COL_WOOD LIPPUS_RGB(118, 72, 36)
#define COL_WOOD_HI LIPPUS_RGB(214, 164, 88)
#define COL_WOOD_DK LIPPUS_RGB(72, 42, 22)
#define COL_BOX LIPPUS_RGB(16, 10, 26)
#define COL_FLASH LIPPUS_RGB(255, 236, 206)
#define COL_CRACK LIPPUS_RGB(42, 32, 28)
#define COL_TEAL LIPPUS_RGB(96, 214, 204)
#define COL_CHISEL LIPPUS_RGB(255, 146, 42)
#define COL_WIDE LIPPUS_RGB(64, 128, 255)

static const char *const k_names[LEVELS] = {
    "Frame", "Steps", "Diamond", "Columns",
    "Ring", "Checker", "Rain", "Rose",
    "Cross", "Bars", "Corners", "Arrow",
    "Vault", "Wave", "Split", "Pyramid",
    "Zigzag", "Pillars", "Orbit", "Crown",
};

/* Ogni riga e' 12 celle. # e' piombo, le lettere sono vetro. */
static const char *const k_levels[LEVELS][ROWS] = {
    {
        "############",
        "#bb......bb#",
        "#rr......rr#",
        "#ee......ee#",
        "#aa..pp..aa#",
        "#vv..gg..vv#",
        "#mm......mm#",
        "............",
    },
    {
        "bb..........",
        "rrbb........",
        "eerrbb......",
        "aaeerrpp....",
        "vvaaeegg....",
        "mmvvaaee....",
        "..mmvvbb....",
        "....mmrr....",
    },
    {
        "....ssss....",
        "...saaaas...",
        "..srggggrs..",
        ".srbpppprs..",
        "..srggggrs..",
        "...saaaas...",
        "....ssss....",
        "............",
    },
    {
        "#bb#rr#ee#aa",
        "#bb#rr#ee#aa",
        "#vv#mm#gg#pp",
        "............",
        "#bb#rr#ee#aa",
        "#vv#mm#ss#gg",
        "#ee#aa#bb#rr",
        "............",
    },
    {
        "............",
        "...rrrrr....",
        "..rb....br..",
        "..r.pggp.r..",
        "..r.pggp.r..",
        "..rb....br..",
        "...rrrrr....",
        "............",
    },
    {
        "brbrbrbrbrbr",
        "rbrbrbrbrbrb",
        "brbrbrbrbrbr",
        "............",
        "eaeaeaeaeaea",
        "aeaeaeaeaeae",
        "............",
        "gpbpsssspbpg",
    },
    {
        "s.b.s.r.s.e.",
        ".s.a.s.v.s.m",
        "s.b.s.g.s.e.",
        ".s.p.s.p.s.g",
        "s.b.s.r.s.e.",
        ".s.a.s.v.s.m",
        "s.m.s.s.s.m.",
        "............",
    },
    {
        "..r..##..r..",
        ".rbr#pp#rbr.",
        "rbrb.gg.brbr",
        ".rbr#pp#rbr.",
        "..r..##..r..",
        ".mmm....mmm.",
        ".msp.ss.psm.",
        "............",
    },
    {
        "............",
        "......bb....",
        "......ee....",
        "..rrppppaa..",
        "......vv....",
        "......mm....",
        "......gg....",
        "............",
    },
    {
        "bbbb....rrrr",
        "bbbb....rrrr",
        "............",
        "eeee....aaaa",
        "eeee....aaaa",
        "............",
        "vvvvppggmmmm",
        "vvvvppggmmmm",
    },
    {
        "bbbr....reee",
        "bbbr....reee",
        "vvvr....raaa",
        "............",
        "............",
        "mmmb....gsss",
        "mmmb....gsss",
        "pppb....rsss",
    },
    {
        ".....ss.....",
        "....srrs....",
        "...sr..rs...",
        "..sr....rs..",
        ".sr..pp..rs.",
        "sr...gg...rs",
        "m...........",
        "............",
    },
    {
        "############",
        "#bb#ee#aa#vv",
        "#bb#ee#aa#vv",
        "#..#..#..#..",
        "#rr#mm#gg#pp",
        "#rr#mm#gg#ss",
        "#..#..#..#..",
        "............",
    },
    {
        "bbrr........",
        "..eeaa......",
        "bbrr..eeaa..",
        "......vvmm..",
        "..ppggbbrr..",
        "eeaa..vvmm..",
        "......ppgg..",
        "bbrr........",
    },
    {
        "bbb######eee",
        "bbb######eee",
        "bbb######eee",
        "............",
        "vvv######mmm",
        "vvv######mmm",
        "ppp######ggg",
        "............",
    },
    {
        "............",
        ".....gg.....",
        "....appa....",
        "...arrrra...",
        "..aeeeeeea..",
        ".avvvvvvvva.",
        "mssssssssssm",
        "............",
    },
    {
        "bbbbb.......",
        ".....eeeee..",
        "..vvvvv.....",
        ".....aaaaa..",
        "rrrrr.......",
        ".....mmmmm..",
        "..ppppp.....",
        ".....ggggg..",
    },
    {
        "b..e..a..v..",
        "b..e..a..v..",
        "b..e..a..v..",
        "b..e..a..v..",
        "r..m..g..p..",
        "r..m..g..p..",
        "s..s..s..s..",
        "............",
    },
    {
        "............",
        ".....rr.....",
        "....r..r....",
        "...r.pp.r...",
        "..r..gg..r..",
        "...r....r...",
        "....r..r....",
        ".....mm.....",
    },
    {
        "s..s..s..s..",
        "ss.ss.ss.ss.",
        ".ssssssssss.",
        "..rrppggvv..",
        "...bbbbbb...",
        "....eeee....",
        ".....aa.....",
        "............",
    },
};

static uint8_t g_kind[ROWS][COLS];
static uint8_t g_hp[ROWS][COLS];
static uint8_t g_flash[ROWS][COLS];
static int g_on[BALLS];
static int g_twin[BALLS];
static int g_bx[BALLS];
static int g_by[BALLS];
static int g_vx[BALLS];
static int g_vy[BALLS];
static int g_prevx[BALLS];
static int g_prevy[BALLS];
static int g_drop_on[DROPS];
static int g_dx[DROPS];
static int g_dy[DROPS];
static int g_dk[DROPS];
static int g_px;
static int g_pw;
static int g_wide;
static int g_slow;
static int g_chisel;
static int g_score;
static int g_lives;
static int g_level;
static int g_left;
static int g_state;
static int g_timer;
static int g_hurt;
static int g_missed;
static int g_speed;
static int g_speed_wait;
static int g_sped_up;
static int g_frame;
static uint32_t g_rng = 0x7E55E4A1u;

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

static int iabs(int v) {
    return v < 0 ? -v : v;
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

static void text_center(int y, const char *s, uint16_t color, int scale) {
    int x = (LIPPUS_SCREEN_W - gfx_text_width(s, scale)) / 2;
    gfx_text(x, y, s, color, scale);
}

static int pressed_go(const lippus_pad_t *pad) {
    return pad_pressed(pad, LIPPUS_A) || pad_pressed(pad, LIPPUS_CLICK);
}

static void format_u(char *dst, unsigned value) {
    int used = 0;
    write_u(dst, &used, value);
    dst[used] = 0;
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

static int cell_x(int c) {
    return ORIGIN_X + c * PITCH_X;
}

static int cell_y(int r) {
    return ORIGIN_Y + r * PITCH_Y;
}

static int base_speed(void) {
    int spd = 3 + (g_level - 1) / 2;
    if (spd > SPEED_CAP - 1) {
        spd = SPEED_CAP - 1;
    }
    return spd;
}

static int speed_wait_frames(void) {
    int pair = (g_level - 1) / 2;
    int wait = SPEED_WAIT - pair * 20;
    if (wait < 120) {
        wait = 120;
    }
    return wait;
}

static void begin_speed(void) {
    g_speed = base_speed();
    g_speed_wait = speed_wait_frames();
    g_sped_up = 0;
}

static int ball_speed(void) {
    int spd = g_speed;
    if (g_slow && spd > 2) {
        spd--;
    }
    if (spd < 2) {
        spd = 2;
    }
    if (spd > SPEED_CAP) {
        spd = SPEED_CAP;
    }
    return spd;
}

static void push_speed(int *v, int target) {
    int mag = iabs(*v);
    if (*v == 0 || mag >= target) {
        return;
    }
    *v += *v > 0 ? 1 : -1;
}

static void raise_speed(void) {
    int i;
    if (g_speed >= SPEED_CAP) {
        return;
    }
    g_speed++;
    for (i = 0; i < BALLS; i++) {
        int cap;
        if (!g_on[i]) {
            continue;
        }
        push_speed(&g_vy[i], g_speed);
        cap = g_speed > 2 ? g_speed - 1 : g_speed;
        if (cap > 5) {
            cap = 5;
        }
        push_speed(&g_vx[i], cap);
    }
    snd_voice(3, SND_TRIANGLE, 880, 90, 60);
}

static void tick_speed(void) {
    if (g_sped_up || g_state != ST_PLAY) {
        return;
    }
    if (--g_speed_wait > 0) {
        return;
    }
    g_sped_up = 1;
    raise_speed();
}

static void clamp_paddle(void) {
    if (g_px < 4) {
        g_px = 4;
    }
    if (g_px + g_pw > LIPPUS_SCREEN_W - 4) {
        g_px = LIPPUS_SCREEN_W - 4 - g_pw;
    }
}

static void set_wide(int wide) {
    int mid = g_px + g_pw / 2;
    g_wide = wide;
    g_pw = wide ? PAD_WIDE : PAD_W;
    g_px = mid - g_pw / 2;
    clamp_paddle();
}

static void clear_drops(void) {
    int i;
    for (i = 0; i < DROPS; i++) {
        g_drop_on[i] = 0;
    }
}

static void reset_powers(void) {
    g_slow = 0;
    g_chisel = 0;
    set_wide(0);
    clear_drops();
}

static int tile_points(int kind) {
    if (kind == K_MORTAR) {
        return 40;
    }
    if (kind == K_STONE) {
        return 80;
    }
    if (kind == K_GOLD) {
        return 100;
    }
    if (kind == K_WELL) {
        return 30;
    }
    return 10;
}

static int decode(char ch, int *hp) {
    switch (ch) {
    case 'b':
        *hp = 1;
        return K_COBALT;
    case 'r':
        *hp = 1;
        return K_RUBY;
    case 'e':
        *hp = 1;
        return K_EMERALD;
    case 'a':
        *hp = 1;
        return K_AMBER;
    case 'v':
        *hp = 1;
        return K_VIOLET;
    case 'm':
        *hp = 2;
        return K_MORTAR;
    case 's':
        *hp = 3;
        return K_STONE;
    case 'g':
        *hp = 1;
        return K_GOLD;
    case 'p':
        *hp = 1;
        return K_WELL;
    case '#':
        *hp = 1;
        return K_LEAD;
    default:
        *hp = 0;
        return K_EMPTY;
    }
}

static void load_level(void) {
    int pattern = (g_level - 1) % LEVELS;
    int r;
    int c;
    g_left = 0;
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            int hp = 0;
            int kind = decode(k_levels[pattern][r][c], &hp);
            g_kind[r][c] = (uint8_t)kind;
            g_hp[r][c] = (uint8_t)hp;
            g_flash[r][c] = 0;
            if (kind != K_EMPTY && kind != K_LEAD) {
                g_left++;
            }
        }
    }
    begin_speed();
}

static void level_label(char *dst) {
    const char *name = k_names[(g_level - 1) % LEVELS];
    int n = 0;
    int giro;
    while (name[n] && n < 16) {
        dst[n] = name[n];
        n++;
    }
    giro = (g_level - 1) / LEVELS + 1;
    if (giro > 1) {
        dst[n++] = ' ';
        write_u(dst, &n, (unsigned)giro);
    }
    dst[n] = 0;
}

static void stick_ball(void) {
    g_on[0] = 1;
    g_bx[0] = g_px + g_pw / 2;
    g_by[0] = PAD_Y - BALL_R - 1;
    g_prevx[0] = g_bx[0];
    g_prevy[0] = g_by[0];
    g_vx[0] = 0;
    g_vy[0] = 0;
}

static void serve(void) {
    int i;
    for (i = 0; i < BALLS; i++) {
        g_on[i] = 0;
        g_twin[i] = 0;
    }
    stick_ball();
    g_state = ST_READY;
}

static void new_game(void) {
    g_score = 0;
    g_lives = 3;
    g_level = 1;
    g_hurt = 0;
    g_missed = 0;
    g_px = (LIPPUS_SCREEN_W - PAD_W) / 2;
    g_pw = PAD_W;
    reset_powers();
    load_level();
    g_on[0] = 0;
    g_on[1] = 0;
    g_on[2] = 0;
    g_state = ST_WAIT;
    g_timer = 18;
}

static void launch(const lippus_pad_t *pad) {
    int dir = 0;
    int spd = ball_speed();
    if (pad_held(pad, LIPPUS_LEFT)) {
        dir--;
    }
    if (pad_held(pad, LIPPUS_RIGHT)) {
        dir++;
    }
    if (dir == 0) {
        dir = (rnd() & 1u) ? -1 : 1;
    }
    g_vx[0] = dir * (spd >= 4 ? 3 : 2);
    g_vy[0] = -spd;
    g_missed = 0;
    g_state = ST_PLAY;
    snd_voice(1, SND_TRIANGLE, 330, 120, 40);
}

static int roll_drop(void) {
    unsigned n = rnd() % 20u;
    if (n < 1u) {
        return DROP_LIFE;
    }
    if (n < 8u) {
        return DROP_WIDE;
    }
    if (n < 13u) {
        return DROP_SLOW;
    }
    if (n < 17u) {
        return DROP_CHISEL;
    }
    return DROP_TWIN;
}

static void spawn_drop(int c, int r) {
    int i;
    for (i = 0; i < DROPS; i++) {
        if (g_drop_on[i]) {
            continue;
        }
        g_drop_on[i] = 1;
        g_dx[i] = cell_x(c) + BRICK_W / 2;
        g_dy[i] = cell_y(r) + BRICK_H / 2;
        g_dk[i] = roll_drop();
        return;
    }
}

static void dampen_balls(void) {
    int i;
    for (i = 0; i < BALLS; i++) {
        if (!g_on[i]) {
            continue;
        }
        if (iabs(g_vx[i]) > 1) {
            g_vx[i] += g_vx[i] > 0 ? -1 : 1;
        }
        if (iabs(g_vy[i]) > 2) {
            g_vy[i] += g_vy[i] > 0 ? -1 : 1;
        }
    }
}

static int first_ball(void) {
    int i;
    for (i = 0; i < BALLS; i++) {
        if (g_on[i]) {
            return i;
        }
    }
    return -1;
}

static int spawn_twin(void) {
    int src = first_ball();
    int i;
    if (src < 0) {
        return 0;
    }
    for (i = 0; i < BALLS; i++) {
        if (g_on[i]) {
            continue;
        }
        g_on[i] = 1;
        g_twin[i] = 1;
        g_bx[i] = g_bx[src];
        g_by[i] = g_by[src];
        g_prevx[i] = g_bx[i];
        g_prevy[i] = g_by[i];
        g_vx[i] = -g_vx[src];
        if (g_vx[i] == 0) {
            g_vx[i] = 2;
        }
        g_vy[i] = g_vy[src];
        return 1;
    }
    return 0;
}

static void catch_drop(int kind) {
    snd_voice(3, SND_TRIANGLE, kind == DROP_LIFE ? 784u : 660u, 160, 90);
    if (kind == DROP_WIDE) {
        set_wide(1);
        return;
    }
    if (kind == DROP_SLOW) {
        if (!g_slow) {
            g_slow = 1;
            dampen_balls();
        }
        return;
    }
    if (kind == DROP_CHISEL) {
        g_chisel = 1;
        return;
    }
    if (kind == DROP_TWIN) {
        if (!spawn_twin()) {
            g_score += 50;
        }
        return;
    }
    if (g_lives < 5) {
        g_lives++;
    } else {
        g_score += 100;
    }
}

static void damage(int c, int r, int noisy) {
    int kind;
    int broken;
    if ((unsigned)c >= COLS || (unsigned)r >= ROWS) {
        return;
    }
    kind = g_kind[r][c];
    if (kind == K_EMPTY || kind == K_LEAD || g_hp[r][c] == 0) {
        return;
    }
    g_hp[r][c]--;
    broken = g_hp[r][c] == 0;
    if (!broken) {
        if (noisy) {
            snd_voice(2, SND_TRIANGLE, 196, 80, 28);
        }
        return;
    }
    g_kind[r][c] = K_EMPTY;
    g_flash[r][c] = 3;
    g_left--;
    g_score += tile_points(kind);
    if (kind == K_WELL || (kind == K_GOLD && (rnd() & 1u) == 0)) {
        spawn_drop(c, r);
    }
    if (noisy) {
        snd_voice(2, SND_SQUARE, (unsigned)(380 + kind * 36), 150, 48);
    }
}

static void hit_tile(int c, int r) {
    int armed;
    int n;
    static const int side[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    if (g_kind[r][c] == K_LEAD) {
        snd_voice(2, SND_SQUARE, 148, 70, 22);
        return;
    }
    armed = g_chisel;
    g_chisel = 0;
    damage(c, r, 1);
    if (armed) {
        for (n = 0; n < 4; n++) {
            damage(c + side[n][0], r + side[n][1], 0);
        }
        snd_voice(3, SND_NOISE, 900, 90, 40);
    }
    if (g_state == ST_PLAY && g_left <= 0) {
        g_state = ST_CLEAR;
        g_timer = 52;
        snd_voice(3, SND_TRIANGLE, 659, 150, 130);
    }
}

static int find_cell(int cx, int cy, int *oc, int *orow) {
    int r;
    int c;
    int found = 0;
    int best = 1 << 30;
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            int x;
            int y;
            int dx;
            int dy;
            int dist;
            if (g_kind[r][c] == K_EMPTY) {
                continue;
            }
            x = cell_x(c);
            y = cell_y(r);
            if (cx <= x - BALL_R || cx >= x + BRICK_W + BALL_R || cy <= y - BALL_R ||
                cy >= y + BRICK_H + BALL_R) {
                continue;
            }
            dx = cx - (x + BRICK_W / 2);
            dy = cy - (y + BRICK_H / 2);
            dist = dx * dx + dy * dy;
            if (dist < best) {
                best = dist;
                *oc = c;
                *orow = r;
                found = 1;
            }
        }
    }
    return found;
}

static void shove_out(int i) {
    int c;
    int r;
    int x;
    int y;
    int pen[4];
    int side = 0;
    int n;
    if (!find_cell(g_bx[i], g_by[i], &c, &r)) {
        return;
    }
    x = cell_x(c);
    y = cell_y(r);
    pen[0] = g_bx[i] - (x - BALL_R);
    pen[1] = (x + BRICK_W + BALL_R) - g_bx[i];
    pen[2] = g_by[i] - (y - BALL_R);
    pen[3] = (y + BRICK_H + BALL_R) - g_by[i];
    for (n = 1; n < 4; n++) {
        if (pen[n] < pen[side]) {
            side = n;
        }
    }
    if (side == 0) {
        g_bx[i] = x - BALL_R;
        g_vx[i] = -iabs(g_vx[i] == 0 ? 2 : g_vx[i]);
    } else if (side == 1) {
        g_bx[i] = x + BRICK_W + BALL_R;
        g_vx[i] = iabs(g_vx[i] == 0 ? 2 : g_vx[i]);
    } else if (side == 2) {
        g_by[i] = y - BALL_R;
        g_vy[i] = -iabs(g_vy[i] == 0 ? 2 : g_vy[i]);
    } else {
        g_by[i] = y + BRICK_H + BALL_R;
        g_vy[i] = iabs(g_vy[i] == 0 ? 2 : g_vy[i]);
    }
}

static void move_ball(int i) {
    int hit_c = -1;
    int hit_r = -1;
    g_prevx[i] = g_bx[i];
    g_prevy[i] = g_by[i];
    g_bx[i] += g_vx[i];
    if (g_vx[i] != 0 && g_state == ST_PLAY) {
        int c;
        int r;
        if (find_cell(g_bx[i], g_by[i], &c, &r)) {
            int pos = cell_x(c);
            if (g_vx[i] > 0) {
                g_bx[i] = pos - BALL_R;
            } else {
                g_bx[i] = pos + BRICK_W + BALL_R;
            }
            g_vx[i] = -g_vx[i];
            hit_c = c;
            hit_r = r;
            hit_tile(c, r);
        }
    }
    if (g_bx[i] < MIN_X) {
        g_bx[i] = MIN_X;
        if (g_vx[i] < 0) {
            g_vx[i] = -g_vx[i];
            snd_voice(1, SND_SQUARE, 210, 48, 18);
        }
    } else if (g_bx[i] > MAX_X) {
        g_bx[i] = MAX_X;
        if (g_vx[i] > 0) {
            g_vx[i] = -g_vx[i];
            snd_voice(1, SND_SQUARE, 210, 48, 18);
        }
    }
    if (g_state != ST_PLAY || !g_on[i]) {
        return;
    }

    g_by[i] += g_vy[i];
    if (g_vy[i] != 0 && g_state == ST_PLAY) {
        int c;
        int r;
        if (find_cell(g_bx[i], g_by[i], &c, &r)) {
            int pos = cell_y(r);
            int same = c == hit_c && r == hit_r;
            if (g_vy[i] > 0) {
                g_by[i] = pos - BALL_R;
            } else {
                g_by[i] = pos + BRICK_H + BALL_R;
            }
            g_vy[i] = -g_vy[i];
            if (!same) {
                hit_tile(c, r);
            }
        }
    }
    if (g_by[i] < MIN_Y) {
        g_by[i] = MIN_Y;
        if (g_vy[i] < 0) {
            g_vy[i] = -g_vy[i];
            snd_voice(1, SND_SQUARE, 210, 48, 18);
        }
    }
    if (g_state != ST_PLAY || !g_on[i]) {
        return;
    }

    if (g_vy[i] > 0 && g_by[i] >= PAD_Y - BALL_R && g_by[i] <= PAD_Y + PAD_H + BALL_R &&
        g_bx[i] >= g_px - BALL_R && g_bx[i] <= g_px + g_pw + BALL_R) {
        if (g_bx[i] < g_px || g_bx[i] > g_px + g_pw) {
            if (g_bx[i] < g_px) {
                g_bx[i] = g_px - BALL_R;
                g_vx[i] = -iabs(g_vx[i] == 0 ? 2 : g_vx[i]);
            } else {
                g_bx[i] = g_px + g_pw + BALL_R;
                g_vx[i] = iabs(g_vx[i] == 0 ? 2 : g_vx[i]);
            }
            snd_voice(1, SND_TRIANGLE, 392, 100, 24);
        } else {
            int half = g_pw / 2;
            int rel = g_bx[i] - (g_px + half);
            int vx = (rel * 4) / (half > 0 ? half : 1);
            int spd = ball_speed();
            if (vx == 0) {
                vx = rel < 0 ? -1 : 1;
            }
            if (vx > 4) {
                vx = 4;
            }
            if (vx < -4) {
                vx = -4;
            }
            g_vx[i] = vx;
            g_vy[i] = (iabs(vx) >= 4 && spd > 2) ? -(spd - 1) : -spd;
            g_by[i] = PAD_Y - BALL_R;
            snd_voice(1, SND_TRIANGLE, 494, 140, 32);
        }
    }

    if (g_state == ST_PLAY) {
        shove_out(i);
    }
    if (g_bx[i] < MIN_X) {
        g_bx[i] = MIN_X;
    } else if (g_bx[i] > MAX_X) {
        g_bx[i] = MAX_X;
    }
    if (g_by[i] > LIPPUS_SCREEN_H + BALL_R) {
        g_on[i] = 0;
        return;
    }
    if (g_vx[i] == 0) {
        g_vx[i] = g_bx[i] < LIPPUS_SCREEN_W / 2 ? 1 : -1;
    }
    if (g_vy[i] == 0) {
        g_vy[i] = -ball_speed();
    }
}

static int any_ball(void) {
    int i;
    for (i = 0; i < BALLS; i++) {
        if (g_on[i]) {
            return 1;
        }
    }
    return 0;
}

static void lose_ball(void) {
    g_lives--;
    g_hurt = 10;
    reset_powers();
    g_on[0] = 0;
    g_on[1] = 0;
    g_on[2] = 0;
    snd_voice(1, SND_NOISE, 90, 160, 180);
    g_missed = 1;
    if (g_lives <= 0) {
        g_state = ST_OVER;
        g_timer = 100;
        return;
    }
    g_state = ST_WAIT;
    g_timer = 36;
}

static void step_balls(void) {
    int i;
    for (i = 0; i < BALLS; i++) {
        if (!g_on[i] || g_state != ST_PLAY) {
            continue;
        }
        move_ball(i);
    }
}

static void step_drops(void) {
    int i;
    for (i = 0; i < DROPS; i++) {
        if (!g_drop_on[i]) {
            continue;
        }
        g_dy[i] += 3;
        if (g_dy[i] + 3 >= PAD_Y && g_dy[i] - 3 <= PAD_Y + PAD_H && g_dx[i] + 3 >= g_px &&
            g_dx[i] - 3 <= g_px + g_pw) {
            int kind = g_dk[i];
            g_drop_on[i] = 0;
            catch_drop(kind);
            continue;
        }
        if (g_dy[i] > LIPPUS_SCREEN_H + 6) {
            g_drop_on[i] = 0;
        }
    }
}

static void move_paddle(const lippus_pad_t *pad) {
    if (pad_held(pad, LIPPUS_LEFT)) {
        g_px -= 8;
    }
    if (pad_held(pad, LIPPUS_RIGHT)) {
        g_px += 8;
    }
    clamp_paddle();
}

static uint16_t tile_body(int kind, int hp) {
    if (kind == K_COBALT) {
        return LIPPUS_RGB(46, 98, 214);
    }
    if (kind == K_RUBY) {
        return LIPPUS_RGB(198, 42, 70);
    }
    if (kind == K_EMERALD) {
        return LIPPUS_RGB(32, 152, 94);
    }
    if (kind == K_AMBER) {
        return LIPPUS_RGB(228, 156, 42);
    }
    if (kind == K_VIOLET) {
        return LIPPUS_RGB(136, 72, 198);
    }
    if (kind == K_MORTAR) {
        return hp < 2 ? LIPPUS_RGB(116, 108, 98) : LIPPUS_RGB(156, 148, 134);
    }
    if (kind == K_STONE) {
        if (hp <= 1) {
            return LIPPUS_RGB(122, 102, 74);
        }
        if (hp == 2) {
            return LIPPUS_RGB(158, 136, 98);
        }
        return LIPPUS_RGB(186, 164, 118);
    }
    if (kind == K_GOLD) {
        return LIPPUS_RGB(255, 206, 64);
    }
    if (kind == K_WELL) {
        return LIPPUS_RGB(236, 226, 206);
    }
    return COL_LEAD;
}

static uint16_t tile_edge(int kind) {
    if (kind == K_LEAD) {
        return COL_LEAD_HI;
    }
    if (kind == K_MORTAR || kind == K_STONE) {
        return LIPPUS_RGB(78, 68, 56);
    }
    if (kind == K_GOLD) {
        return LIPPUS_RGB(176, 120, 28);
    }
    if (kind == K_WELL) {
        return LIPPUS_RGB(168, 140, 110);
    }
    return LIPPUS_RGB(255, 244, 220);
}

static void draw_tile(int x, int y, int kind, int hp) {
    gfx_fill_rect(x, y, BRICK_W, BRICK_H, tile_body(kind, hp));
    gfx_rect(x, y, BRICK_W, BRICK_H, tile_edge(kind));
    if (kind == K_LEAD) {
        gfx_fill_rect(x + 1, y + 1, BRICK_W - 2, 1, COL_LEAD_HI);
        return;
    }
    gfx_fill_rect(x + 2, y + 1, 6, 1, LIPPUS_RGB(255, 248, 230));
    if (kind == K_WELL) {
        gfx_fill_circle(x + BRICK_W / 2, y + BRICK_H / 2, 2, LIPPUS_RGB(214, 64, 128));
    }
    if ((kind == K_MORTAR && hp < 2) || (kind == K_STONE && hp < 3)) {
        gfx_line(x + 3, y + 2, x + BRICK_W - 4, y + BRICK_H - 3, COL_CRACK);
    }
    if (kind == K_STONE && hp == 1) {
        gfx_line(x + BRICK_W - 5, y + 2, x + 4, y + BRICK_H - 3, COL_CRACK);
    }
}

static void draw_knife_at(int x, int y, int w) {
    int hx = x + w / 2 - 2;
    gfx_fill_rect(x, y, w, PAD_H, COL_WOOD);
    gfx_fill_rect(x, y, w, 2, COL_WOOD_HI);
    gfx_fill_rect(x, y + PAD_H - 1, w, 1, COL_WOOD_DK);
    gfx_fill_rect(hx, y + PAD_H, 5, 10, COL_WOOD_DK);
    gfx_fill_rect(hx, y + PAD_H, 2, 10, COL_WOOD_HI);
}

static void draw_knife(void) {
    draw_knife_at(g_px, PAD_Y, g_pw);
}

static void draw_bead_at(int x, int y, uint16_t body) {
    gfx_fill_circle(x, y, BALL_R, body);
    gfx_fill_circle(x - 1, y - 1, 1, LIPPUS_WHITE);
}

static void draw_bead(int i) {
    uint16_t body = LIPPUS_WHITE;
    int x = g_bx[i];
    int y = g_by[i];
    if (!g_twin[i]) {
        if (g_chisel) {
            body = COL_CHISEL;
        } else if (g_slow) {
            body = COL_TEAL;
        } else if (g_wide) {
            body = COL_WIDE;
        }
    }
    if (g_prevx[i] != x || g_prevy[i] != y) {
        gfx_fill_circle(g_prevx[i], g_prevy[i], 2, LIPPUS_RGB(150, 176, 210));
    }
    gfx_fill_circle(x, y, BALL_R, body);
    gfx_fill_circle(x - 1, y - 1, 1, LIPPUS_WHITE);
}

static uint16_t drop_color(int kind) {
    if (kind == DROP_WIDE) {
        return COL_WIDE;
    }
    if (kind == DROP_SLOW) {
        return COL_TEAL;
    }
    if (kind == DROP_CHISEL) {
        return COL_CHISEL;
    }
    if (kind == DROP_TWIN) {
        return LIPPUS_RGB(176, 110, 255);
    }
    return LIPPUS_RGB(255, 86, 112);
}

static void draw_hud(void) {
    char num[12];
    char lv[12];
    int used = 0;
    int pip = 188;
    gfx_fill_rect(0, 0, LIPPUS_SCREEN_W, 16, COL_HUD);
    gfx_text(6, 4, "BREAKER", LIPPUS_AMBER, 1);
    write_u(num, &used, (unsigned)g_score);
    num[used] = 0;
    gfx_text(78, 4, num, LIPPUS_CREAM, 1);
    used = 0;
    lv[used++] = 'L';
    lv[used++] = 'V';
    write_u(lv, &used, (unsigned)g_level);
    lv[used] = 0;
    gfx_text(148, 4, lv, LIPPUS_CREAM, 1);
    if (g_wide) {
        gfx_fill_rect(pip, 5, 8, 6, COL_WIDE);
        pip += 12;
    }
    if (g_slow) {
        gfx_fill_rect(pip, 5, 8, 6, COL_TEAL);
        pip += 12;
    }
    if (g_chisel) {
        gfx_fill_rect(pip, 5, 8, 6, COL_CHISEL);
    }
    if (g_lives > 0) {
        int i;
        int x0 = LIPPUS_SCREEN_W - 8 - g_lives * 10;
        for (i = 0; i < g_lives; i++) {
            gfx_fill_circle(x0 + i * 10, 8, 3, LIPPUS_RGB(220, 72, 96));
        }
    }
}

static void draw_world(void) {
    int r;
    int c;
    int i;
    gfx_clear(COL_NIGHT);
    gfx_fill_rect(4, 196, LIPPUS_SCREEN_W - 8, 44, g_hurt ? COL_HURT : COL_SILL);
    gfx_fill_rect(4, 196, LIPPUS_SCREEN_W - 8, 2, LIPPUS_RGB(54, 44, 40));
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            int x = cell_x(c);
            int y = cell_y(r);
            if (g_flash[r][c]) {
                gfx_fill_rect(x, y, BRICK_W, BRICK_H, COL_FLASH);
                continue;
            }
            if (g_kind[r][c] == K_EMPTY) {
                continue;
            }
            draw_tile(x, y, g_kind[r][c], g_hp[r][c]);
        }
    }
    gfx_fill_rect(0, 16, LIPPUS_SCREEN_W, 4, COL_LEAD);
    gfx_fill_rect(0, 16, 4, LIPPUS_SCREEN_H - 16, COL_LEAD);
    gfx_fill_rect(LIPPUS_SCREEN_W - 4, 16, 4, LIPPUS_SCREEN_H - 16, COL_LEAD);
    gfx_fill_rect(0, 16, LIPPUS_SCREEN_W, 1, COL_LEAD_HI);
    for (i = 0; i < DROPS; i++) {
        if (!g_drop_on[i]) {
            continue;
        }
        gfx_fill_circle(g_dx[i], g_dy[i], 3, drop_color(g_dk[i]));
        gfx_fill_circle(g_dx[i] - 1, g_dy[i] - 1, 1, LIPPUS_WHITE);
    }
    draw_knife();
    for (i = 0; i < BALLS; i++) {
        if (g_on[i]) {
            draw_bead(i);
        }
    }
    draw_hud();
}

static void draw_banner(void) {
    char label[20];
    if (g_missed && g_state != ST_CLEAR) {
        text_center(136, "OH NO!", LIPPUS_RGB(220, 72, 96), 1);
    } else {
        level_label(label);
        text_center(136, label, LIPPUS_AMBER, 1);
    }
    if (g_state == ST_READY && (g_frame & 16) == 0) {
        text_center(152, "PRESS A", LIPPUS_CREAM, 1);
    }
    if (g_state == ST_CLEAR) {
        text_center(152, "CLEARED", LIPPUS_CREAM, 1);
    }
}

static void draw_drop_at(int x, int y, int kind) {
    gfx_fill_circle(x, y, 3, drop_color(kind));
    gfx_fill_circle(x - 1, y - 1, 1, LIPPUS_WHITE);
}

static void draw_press_start(void) {
    if ((g_frame & 16) == 0) {
        text_center(LIPPUS_SCREEN_H - 16, "PRESS START", LIPPUS_YELLOW, 1);
    }
}

static void draw_demo(void) {
    int t = g_attract_t;
    const char *caption = "SLIDE THE KNIFE";
    text_center(4, "LIPPUS BREAKER", LIPPUS_AMBER, 2);
    if (t < 105) {
        int swing = t % 70;
        int kx = 70 + (swing < 35 ? swing * 4 : (70 - swing) * 4);
        int by = 146;
        caption = t < 42 ? "SLIDE THE KNIFE" : "A LAUNCHES THE BEAD";
        draw_knife_at(kx, 158, PAD_W);
        if (t >= 42) {
            by = 146 - (t - 42) * 2;
            if (by < 40) {
                by = 40;
            }
        }
        draw_bead_at(kx + PAD_W / 2, by, LIPPUS_WHITE);
    } else if (t < 210) {
        static const int kinds[5] = {K_RUBY, K_AMBER, K_LEAD, K_EMERALD, K_COBALT};
        int u = t - 105;
        int x0 = 24;
        int bead_x = 36 + u * 2;
        int i;
        if (bead_x > 270) {
            bead_x = 270;
        }
        caption = bead_x < x0 + 2 * 56 ? "GLASS BREAKS" : "LEAD STAYS PUT";
        for (i = 0; i < 5; i++) {
            int tx = x0 + i * 56;
            int passed = bead_x > tx + 16;
            if (kinds[i] == K_LEAD || !passed) {
                draw_tile(tx, 86, kinds[i], 1);
            } else if (bead_x < tx + 28) {
                gfx_fill_rect(tx, 86, BRICK_W, BRICK_H, COL_FLASH);
            }
        }
        if (bead_x > x0 + 2 * 56 + 8 && bead_x < x0 + 3 * 56) {
            bead_x = x0 + 2 * 56 - 8;
        }
        draw_bead_at(bead_x, 112, LIPPUS_WHITE);
        draw_knife_at(130, 158, PAD_W);
    } else if (t < 315) {
        int u = t - 210;
        int hp_m = 2;
        int hp_s = 3;
        int x0 = (LIPPUS_SCREEN_W - 2 * 56) / 2;
        caption = "MORTAR TAKES TWO HITS";
        if (u > 30) {
            hp_m = 1;
        }
        if (u > 60) {
            hp_m = 0;
            caption = "STONE TAKES THREE HITS";
        }
        if (u > 75) {
            hp_s = 2;
        }
        if (u > 90) {
            hp_s = 1;
        }
        if (hp_m > 0) {
            draw_tile(x0, 86, K_MORTAR, hp_m);
        } else if (u < 72) {
            gfx_fill_rect(x0, 86, BRICK_W, BRICK_H, COL_FLASH);
        }
        draw_tile(x0 + 56, 86, K_STONE, hp_s);
        draw_bead_at(x0 + (u > 60 ? 68 : 12), 112, LIPPUS_WHITE);
        draw_knife_at(120, 158, PAD_W);
    } else {
        static const int kinds[5] = {DROP_WIDE, DROP_SLOW, DROP_CHISEL, DROP_TWIN, DROP_LIFE};
        static const char *labels[5] = {"WIDE", "SLOW", "CHIP", "TWIN", "LIFE"};
        static const char *caps[5] = {
            "WIDENS THE KNIFE", "SLOWS THE BEAD", "CHIPS NEIGHBORS", "ADDS A BEAD", "GRANTS A LIFE",
        };
        int u = t - 315;
        int which = (u / 21) % 5;
        int fall = (u % 21) * 4;
        int i;
        int kx;
        caption = caps[which];
        for (i = 0; i < 5; i++) {
            int x = 28 + i * 58;
            uint16_t color = i == which ? LIPPUS_AMBER : LIPPUS_RGB(150, 156, 176);
            if (i != which) {
                draw_drop_at(x, 78, kinds[i]);
            }
            gfx_text(x - 12, 96, labels[i], color, 1);
        }
        if (fall > 70) {
            fall = 70;
        }
        draw_drop_at(28 + which * 58, 48 + fall, kinds[which]);
        kx = 28 + which * 58 - PAD_W / 2;
        if (kx < 8) {
            kx = 8;
        }
        draw_knife_at(kx, 150, PAD_W);
    }
    text_center(196, caption, LIPPUS_CREAM, 1);
}

static void draw_score_row(int y, int rank, const score_t *row, uint16_t color) {
    char pos[4];
    char name[4];
    char num[8];
    format_u(pos, (unsigned)(rank + 1));
    name[0] = row->name[0];
    name[1] = row->name[1];
    name[2] = row->name[2];
    name[3] = 0;
    format_u(num, (unsigned)row->score);
    gfx_text(88 - gfx_text_width(pos, 1), y, pos, color, 1);
    gfx_text(104, y, name, color, 1);
    gfx_text(236 - gfx_text_width(num, 1), y, num, color, 1);
}

static void draw_scores(void) {
    int i;
    text_center(2, "TOP 20", LIPPUS_AMBER, 1);
    if (g_score_n == 0) {
        text_center(100, "NO SCORES YET", LIPPUS_CREAM, 1);
        return;
    }
    for (i = 0; i < g_score_n; i++) {
        uint16_t color = (i == g_rank) ? LIPPUS_YELLOW : LIPPUS_CREAM;
        draw_score_row(16 + i * 10, i, &g_scores[i], color);
    }
}

static void draw_attract(void) {
    gfx_clear(COL_NIGHT);
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
    format_u(num, (unsigned)g_score);
    gfx_clear(COL_NIGHT);
    text_center(28, "NEW HIGH SCORE", LIPPUS_AMBER, 1);
    text_center(46, num, LIPPUS_CREAM, 2);
    text_center(78, "YOUR NAME", LIPPUS_CREAM, 1);
    shown[0] = g_initials[0];
    shown[1] = g_initials[1];
    shown[2] = g_initials[2];
    shown[3] = 0;
    text_center(104, shown, LIPPUS_YELLOW, 3);
    if ((g_frame & 8) == 0) {
        int x = (LIPPUS_SCREEN_W - gfx_text_width(shown, 3)) / 2 + g_name_i * 24;
        gfx_fill_rect(x, 132, 20, 3, LIPPUS_YELLOW);
    }
    text_center(168, "UP DOWN", LIPPUS_CREAM, 1);
    text_center(184, "LEFT RIGHT", LIPPUS_CREAM, 1);
    text_center(200, "START ENTERS", LIPPUS_CREAM, 1);
}

static void draw_game_over(void) {
    char num[8];
    format_u(num, (unsigned)g_score);
    gfx_fill_rect(46, 92, 228, 52, COL_BOX);
    gfx_rect(46, 92, 228, 52, LIPPUS_AMBER);
    text_center(100, "GAME OVER", LIPPUS_AMBER, 2);
    text_center(124, num, LIPPUS_CREAM, 1);
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

static void fade_fx(void) {
    int r;
    int c;
    if (g_hurt > 0) {
        g_hurt--;
    }
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            if (g_flash[r][c]) {
                g_flash[r][c]--;
            }
        }
    }
}

void lippus_main(void) {
    g_rank = -1;
    scores_load();
    g_rng ^= sys_ticks() | 1u;
    g_pw = PAD_W;
    g_px = (LIPPUS_SCREEN_W - PAD_W) / 2;
    enter_attract(0);

    for (;;) {
        lippus_pad_t pad = pad_poll();
        g_frame++;

        if (g_state == ST_ATTRACT) {
            if (pad_pressed(&pad, LIPPUS_START)) {
                g_rank = -1;
                new_game();
            } else {
                if (++g_attract_t >= ATTRACT_LEN) {
                    g_attract_t = 0;
                    g_attract_page ^= 1;
                }
                draw_attract();
            }
            if (g_state != ST_ATTRACT) {
                draw_world();
                draw_banner();
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
            fade_fx();
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

        move_paddle(&pad);
        if (g_state == ST_WAIT) {
            g_timer--;
            if (g_timer <= 0) {
                serve();
            }
        } else if (g_state == ST_READY) {
            stick_ball();
            if (pressed_go(&pad) || pad_pressed(&pad, LIPPUS_UP) || pad_pressed(&pad, LIPPUS_START)) {
                launch(&pad);
            }
        } else if (g_state == ST_PLAY) {
            tick_speed();
            step_balls();
            if (g_state == ST_PLAY && !any_ball()) {
                lose_ball();
            } else if (g_state == ST_PLAY || g_state == ST_CLEAR) {
                step_drops();
            }
        } else if (g_state == ST_CLEAR) {
            step_drops();
            g_timer--;
            if (g_timer <= 0) {
                g_level++;
                g_missed = 0;
                load_level();
                clear_drops();
                serve();
            }
        }

        fade_fx();
        draw_world();
        if (g_state == ST_WAIT || g_state == ST_READY || g_state == ST_CLEAR) {
            draw_banner();
        }
        gfx_present();
        sys_frame(33);
    }
}
