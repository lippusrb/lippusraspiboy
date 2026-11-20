#include "lippus.h"
#include "lippus/sfx.h"

#include "assets.h"
#include "font.h"

/* Skyward Climb. Kip sale la torre prima della lava.
   A salta (altezza dal tempo di pressione, secondo salto con gli stivali).
   B a terra, o Su+B in aria: rampino. B in aria: pistola verso il basso,
   il rinculo frena la caduta. Quattro colpi, si ricaricano al contatto.
   Le piattaforme sottili si attraversano dal basso. Il rampino non attacca
   rovi e cavi. La tartaruga si ribalta di lato o col rampino.
   Ogni bioma finisce in una stanza di riposo: si sceglie un potenziamento.
   All'avvio le slide spiegano la salita, poi passa la classifica dei venti.
   Lo schermo della console e' 320x240: l'area di gioco e' 12 tile
   da x=64, le due fasce da 64 sono l'HUD. */

#define FRAME_MS 33
#define PLAY_X 64
#define PLAY_W 192
#define COLS 12
#define TILE 16
#define CH 12
#define RING 3
#define ENTS 36
#define CRUMBS 16
#define BIOME_M (9 * CH)
#define SCORE_N 20
#define SLIDE_N 5
#define SLIDE_HOW 4
#define SLIDE_LEN 120
#define SCORE_HOLD 180

#define GRAV 56
#define JUMP_V 1420
#define JUMP_CUT 520
#define MAX_FALL -900
#define MAX_RISE 1500
#define RUN_ACC 96
#define AIR_ACC 64
#define RUN_MAX 560
#define RUN_FRIC 150
#define RECOIL 540
#define ROPE_BASE 72
#define ROPE_LONG 94

#define TILE_AIR 0
#define TILE_SOLID 1
#define TILE_ONEWAY 2
#define TILE_CRUMBLE 3
#define TILE_SPIKE_UP 4
#define TILE_SPIKE_DN 5
#define TILE_THORN 6
#define TILE_TURRET_L 7
#define TILE_TURRET_R 8
#define TILE_PORTAL 9

#define EK_NONE 0
#define EK_GULL 1
#define EK_TURTLE 2
#define EK_BAT 3
#define EK_CRYSTAL 4
#define EK_FIRE 5
#define EK_ORB 6
#define EK_BULLET 7
#define EK_SPARK 8

#define ST_TITLE 0
#define ST_PLAY 1
#define ST_PAUSE 2
#define ST_REST 3
#define ST_PEAK 4
#define ST_DIE 5
#define ST_OVER 6
#define ST_WIN 7
#define ST_NAME 8

static const uint16_t PAL[16] = {
    0xF81F, 0x0862, 0x2147, 0x52CE, 0x9D36, 0xF7BE, 0xD947, 0xF323,
    0xF565, 0xDCD2, 0x69C5, 0x3C67, 0x8E09, 0x2D9E, 0x1338, 0x7AB8,
};

static const int LAVA_Q8[4] = {101, 135, 169, 203};
static const char *const BIOME_TITLE[4] = {"CRYPT", "GLASS", "WIND", "PEAK"};

/* Schema fisso: 4 biomi da 8 chunk, poi la stanza di riposo.
   Il chunk 35 e' la vetta. Lo stesso indice ricostruisce sempre la stessa stanza.
   Riga 0 = cima. '-' pianerottolo, '~' cede, '^' spine, 't' rovi,
   L/R sputafuoco, G gabbiano, U tartaruga, B pipistrello, O sfera, C cristallo.
   I pianerottoli in riga 0 e 10 stanno al centro, cosi' i chunk si incastrano.
   I vuoti sono di 4 righe al massimo; Vento e Vetta hanno un salto da rampino. */

static const char CRYPT_0[] =
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#.......C..#"
    "#..........#"
    "#...----...#"
    "#..........#"
    "############";

static const char CRYPT_1[] =
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#..----....#"
    "#..........#"
    "#..G.......#"
    "#..----....#"
    "#..........#"
    "#.......C..#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char CRYPT_2[] =
    "#...----...#"
    "#..........#"
    "#.......C..#"
    "#....----..#"
    "#..........#"
    "#.......G..#"
    "#......----#"
    "#..........#"
    "#..C.......#"
    "#..----....#"
    "#...----...#"
    "#..........#";

static const char CRYPT_3[] =
    "#...----...#"
    "#..........#"
    "#.C........#"
    "#----......#"
    "#..........#"
    "#....U.....#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char CRYPT_4[] =
    "#...----...#"
    "#..........#"
    "#.G........#"
    "#----......#"
    "#..........#"
    "#.......G..#"
    "#......----#"
    "#..........#"
    "#....C.....#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char CRYPT_5[] =
    "#...----...#"
    "#..........#"
    "#..C....G..#"
    "#..------..#"
    "#..........#"
    "#..........#"
    "#....U.....#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char CRYPT_6[] =
    "#...----...#"
    "#..........#"
    "#.....C....#"
    "#...----...#"
    "#..........#"
    "#..G.......#"
    "#..----....#"
    "#..........#"
    "#.......G..#"
    "#....----..#"
    "#...----...#"
    "#..........#";

static const char CRYPT_7[] =
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#..----....#"
    "#..^^^.....#"
    "#..----....#"
    "#..........#"
    "#....U.....#"
    "#...----...#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char SERRE_0[] =
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#..~~~.....#"
    "#..........#"
    "#.....G....#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char SERRE_1[] =
    "#...----...#"
    "#..........#"
    "#..tttt....#"
    "#..........#"
    "#....----..#"
    "#....B.....#"
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#..----....#"
    "#...----...#"
    "#..........#";

static const char SERRE_2[] =
    "#...----...#"
    "#..........#"
    "#.....C....#"
    "#...----...#"
    "#....B.....#"
    "#..........#"
    "#....U.....#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char SERRE_3[] =
    "#...----...#"
    "#..........#"
    "#.......C..#"
    "#......~~~.#"
    "#..........#"
    "#..~~~.....#"
    "#..........#"
    "#..G.......#"
    "#..----....#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char SERRE_4[] =
    "#...----...#"
    "#..........#"
    "#t........t#"
    "#....----..#"
    "#..........#"
    "#.......G..#"
    "#......----#"
    "#..........#"
    "#..C.......#"
    "#..----....#"
    "#...----...#"
    "#..........#";

static const char SERRE_5[] =
    "#...----...#"
    "#..........#"
    "#.......B..#"
    "#......----#"
    "#..........#"
    "#..U.......#"
    "#..----....#"
    "#..........#"
    "#....C.....#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char SERRE_6[] =
    "#...----...#"
    "#..........#"
    "#.....C....#"
    "#....~~~...#"
    "#..........#"
    "#..~~~.....#"
    "#..........#"
    "#......G...#"
    "#....----..#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char SERRE_7[] =
    "#...----...#"
    "#..........#"
    "#..tttt....#"
    "#....----..#"
    "#....B.....#"
    "#...----...#"
    "#..........#"
    "#...U......#"
    "#..~~~.....#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char WIND_0[] =
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#..----....#"
    "#..........#"
    "#....^^....#"
    "#...----...#"
    "#..........#"
    "#.......G..#"
    "#....----..#"
    "#...----...#"
    "#..........#";

static const char WIND_1[] =
    "#...----...#"
    "#..........#"
    "L..........#"
    "#..----....#"
    "#..........#"
    "#.....C....#"
    "#...----...#"
    "#..........#"
    "#..........R"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char WIND_2[] =
    "#...----...#"
    "#..........#"
    "#..^^^.....#"
    "#..----....#"
    "#..........#"
    "#......^^^.#"
    "#......----#"
    "#..........#"
    "#..B.......#"
    "#..----....#"
    "#...----...#"
    "#..........#";

static const char WIND_3[] =
    "#...----...#"
    "#..........#"
    "#..........#"
    "#..C.......#"
    "#..........#"
    "#...----...#"
    "#..........#"
    "#.......G..#"
    "#......----#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char WIND_4[] =
    "#...----...#"
    "L..........#"
    "#..........#"
    "#..----....#"
    "#..........#"
    "#..........R"
    "#....----..#"
    "#..........#"
    "#..G.......#"
    "#..----....#"
    "#...----...#"
    "#..........#";

static const char WIND_5[] =
    "#...----...#"
    "#..........#"
    "#..^^^.....#"
    "#..~~~.....#"
    "#..........#"
    "#....B.....#"
    "#...----...#"
    "#..........#"
    "#.......C..#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char WIND_6[] =
    "#...----...#"
    "#..........#"
    "L..........#"
    "#..........#"
    "#..........#"
    "#..----....#"
    "#..........#"
    "#......^^..#"
    "#......----#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char WIND_7[] =
    "#...----...#"
    "#..........#"
    "#..^^^.....#"
    "#..----....#"
    "#..........R"
    "#....----..#"
    "#..........#"
    "#..B.......#"
    "#..----....#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_0[] =
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#..----....#"
    "#..........#"
    "#.....O....#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_1[] =
    "#...----...#"
    "#..........#"
    "#..^^^..C..#"
    "#..----....#"
    "#..........#"
    "#.....O....#"
    "#...----...#"
    "#..........#"
    "#......^^..#"
    "#......----#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_2[] =
    "#...----...#"
    "#..........#"
    "#.....C....#"
    "#....~~~...#"
    "#..........#"
    "#..O.......#"
    "#..----....#"
    "#..........#"
    "#....B.....#"
    "#...----...#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_3[] =
    "#...----...#"
    "#..........#"
    "#..O.......#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#"
    "#..^^^.....#"
    "#..----....#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_4[] =
    "#...----...#"
    "L..........#"
    "#..^^^.....#"
    "#..----....#"
    "#..........#"
    "#..........R"
    "#....----..#"
    "#..........#"
    "#.....O....#"
    "#...----...#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_5[] =
    "#...----...#"
    "#..........#"
    "#..C.......#"
    "#..~~~.....#"
    "#..........#"
    "#....U.....#"
    "#...----...#"
    "#..........#"
    "#......O...#"
    "#......----#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_6[] =
    "#...----...#"
    "#..........#"
    "#..........#"
    "#..O.......#"
    "#..........#"
    "#..----....#"
    "#..^^^.....#"
    "#..----....#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#";

static const char ASTRAL_7[] =
    "#...----...#"
    "#..........#"
    "#..^^^..O..#"
    "#..----....#"
    "#..........R"
    "#....~~~...#"
    "#..........#"
    "#..B.U.....#"
    "#..----....#"
    "#....C.....#"
    "#...----...#"
    "#..........#";

static const char REST_MAP[] =
    "############"
    "#..........#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#"
    "#..........#"
    "#...----...#";

static const char CAP_MAP[] =
    "############"
    "############"
    "#..........#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#..........#"
    "#...----...#"
    "#..........#";


static const char *const COURSE[4][8] = {
    {CRYPT_0, CRYPT_1, CRYPT_2, CRYPT_3, CRYPT_4, CRYPT_5, CRYPT_6, CRYPT_7},
    {SERRE_0, SERRE_1, SERRE_2, SERRE_3, SERRE_4, SERRE_5, SERRE_6, SERRE_7},
    {WIND_0, WIND_1, WIND_2, WIND_3, WIND_4, WIND_5, WIND_6, WIND_7},
    {ASTRAL_0, ASTRAL_1, ASTRAL_2, ASTRAL_3, ASTRAL_4, ASTRAL_5, ASTRAL_6, ASTRAL_7},
};

typedef char map_len_ok[sizeof(CRYPT_0) == 145 && sizeof(CRYPT_1) == 145 && sizeof(CRYPT_2) == 145 && sizeof(CRYPT_3) == 145 && sizeof(CRYPT_4) == 145 && sizeof(CRYPT_5) == 145 && sizeof(CRYPT_6) == 145 && sizeof(CRYPT_7) == 145 && sizeof(SERRE_0) == 145 && sizeof(SERRE_1) == 145 && sizeof(SERRE_2) == 145 && sizeof(SERRE_3) == 145 && sizeof(SERRE_4) == 145 && sizeof(SERRE_5) == 145 && sizeof(SERRE_6) == 145 && sizeof(SERRE_7) == 145 && sizeof(WIND_0) == 145 && sizeof(WIND_1) == 145 && sizeof(WIND_2) == 145 && sizeof(WIND_3) == 145 && sizeof(WIND_4) == 145 && sizeof(WIND_5) == 145 && sizeof(WIND_6) == 145 && sizeof(WIND_7) == 145 && sizeof(ASTRAL_0) == 145 && sizeof(ASTRAL_1) == 145 && sizeof(ASTRAL_2) == 145 && sizeof(ASTRAL_3) == 145 && sizeof(ASTRAL_4) == 145 && sizeof(ASTRAL_5) == 145 && sizeof(ASTRAL_6) == 145 && sizeof(ASTRAL_7) == 145 && sizeof(REST_MAP) == 145 && sizeof(CAP_MAP) == 145 ? 1 : -1];

static uint16_t scratch[256];
static uint8_t map[RING][CH][COLS];
static int chunk_base[RING];
static int rest_base;
static int peak_base;
static int rest_done;
static int cam_top;
static int draw_dy;
static int clock;
static int shake;
static uint32_t rng;

static int px, py, vx, vy, px_acc, py_acc;
static int face;
static int grounded, coyote, jump_buf, jumps;
static int hearts, hurt_t, ammo, muzzle_t;
static int score, combo, crystals;
static int rope, pierce, boots, magnet;
static int lava_y, lava_acc;
static int death_lava;
static int won_flag;
static int state;
static int die_t;
static int grapple, hook_x, hook_y, hook_vx, hook_vy, hook_t;
static int last_wave;

static int offer[3];
static int offer_sel;

typedef struct {
    char name[3];
    int score;
} score_t;

static score_t scores[SCORE_N];
static int score_n;
static int rank_hi;
static char letters[3];
static int name_pos;
static int letter_hold;
static int attract_page;
static int attract_t;
static int end_t;
static char rec[LIPPUS_RECORD_MAX];

static int seq_hz[6];
static int seq_ms[6];
static int seq_n, seq_i, seq_left, seq_wave;
static int amb_i, amb_left;
/* Livello 2 e' il volume di sempre. Sulle slide Y sale e B scende. */
#define MUS_BASE 36
#define MUS_LVL_MAX 4
#define MUS_LVL_DEF 2
static int mus_lvl = MUS_LVL_DEF;

static const uint16_t AMB_HZ[] = {196, 0, 247, 294, 0, 220, 0, 0};
static const uint16_t AMB_MS[] = {180, 50, 160, 200, 60, 220, 80, 320};

static unsigned mus_volume(void) {
    return (unsigned)(MUS_BASE * mus_lvl / MUS_LVL_DEF);
}

struct crumb {
    int16_t row;
    int16_t col;
    uint8_t t;
};
static struct crumb crumb[CRUMBS];
static int crumb_n;

struct ent {
    uint8_t kind;
    uint8_t mode;
    uint8_t dir;
    uint8_t flags;
    int16_t x, y, vx, vy, t;
};
static struct ent ent[ENTS];

static int iabs(int v) { return v < 0 ? -v : v; }

static uint32_t rnd(void) {
    rng = rng * 1664525u + 1013904223u;
    return rng;
}

static int biome_of(int world_y) {
    int m;
    if (world_y < 0) {
        return 0;
    }
    m = world_y / TILE / BIOME_M;
    if (m > 3) {
        m = 3;
    }
    return m;
}

static void seq_start(int wave, const int *hz, const int *ms, int n) {
    int i;
    if (n > 6) {
        n = 6;
    }
    seq_wave = wave;
    seq_n = n;
    seq_i = 0;
    seq_left = 0;
    for (i = 0; i < n; i++) {
        seq_hz[i] = hz[i];
        seq_ms[i] = ms[i];
    }
}

static void seq_tick(void) {
    if (seq_n == 0) {
        return;
    }
    if (seq_left > 0) {
        seq_left -= FRAME_MS;
        return;
    }
    if (seq_i >= seq_n) {
        seq_n = 0;
        return;
    }
    if (seq_hz[seq_i] <= 0) {
        snd_voice(2, SND_OFF, 0, 0, 0);
    } else {
        snd_voice(2, (unsigned)seq_wave, (unsigned)seq_hz[seq_i], 170, (unsigned)seq_ms[seq_i]);
    }
    seq_left = seq_ms[seq_i];
    seq_i++;
}

static void amb_tick(void) {
    unsigned n = (unsigned)(sizeof(AMB_HZ) / sizeof(AMB_HZ[0]));
    if (amb_left > 0) {
        amb_left -= FRAME_MS;
        return;
    }
    if (AMB_HZ[amb_i] == 0) {
        snd_voice(3, SND_OFF, 0, 0, 0);
    } else {
        snd_voice(3, SND_TRIANGLE, AMB_HZ[amb_i], mus_volume(), AMB_MS[amb_i]);
    }
    amb_left = (int)AMB_MS[amb_i];
    amb_i++;
    if (amb_i >= (int)n) {
        amb_i = 0;
    }
}

static void mus_nudge(int dir) {
    int next = mus_lvl + dir;
    int n = (int)(sizeof(AMB_HZ) / sizeof(AMB_HZ[0]));
    if (next < 0) {
        next = 0;
    }
    if (next > MUS_LVL_MAX) {
        next = MUS_LVL_MAX;
    }
    if (next == mus_lvl) {
        return;
    }
    mus_lvl = next;
    if (amb_left > 0) {
        amb_i--;
        if (amb_i < 0) {
            amb_i = n - 1;
        }
        amb_left = 0;
    }
}

static void sfx_jump(int dub) {
    if (dub) {
        sfx_sweep(0, SND_TRIANGLE, 340, 1200, 200, 120);
    } else {
        sfx_sweep(0, SND_SQUARE, 220, 980, 180, 140);
    }
}

static void sfx_shot(void) {
    sfx_sweep(0, SND_SQUARE, 520, 70, 200, 90);
    snd_voice(1, SND_NOISE, 900, 170, 60);
}

static void sfx_hook_fly(void) { sfx_sweep(0, SND_SQUARE, 780, 140, 150, 80); }

static void sfx_hook_hit(void) { sfx_sweep(0, SND_SQUARE, 880, 1500, 200, 70); }

static void sfx_stomp(void) { sfx_sweep(0, SND_SQUARE, 160, 720, 200, 100); }

static void sfx_hurt(void) {
    sfx_sweep(0, SND_SQUARE, 280, 55, 220, 180);
    snd_voice(1, SND_NOISE, 380, 150, 120);
}

static void sfx_crystal(void) {
    int boost = combo * 40;
    int hz[2];
    int ms[2];
    if (boost > 480) {
        boost = 480;
    }
    hz[0] = 988 + boost;
    hz[1] = 1318 + boost;
    ms[0] = 55;
    ms[1] = 100;
    seq_start(SND_TRIANGLE, hz, ms, 2);
}

static void sfx_upgrade(void) {
    static const int hz[4] = {523, 659, 784, 1047};
    static const int ms[4] = {85, 85, 85, 140};
    seq_start(SND_SQUARE, hz, ms, 4);
}

static void put_num(char *d, unsigned n, int digits) {
    int i;
    d[digits] = 0;
    for (i = digits - 1; i >= 0; i--) {
        d[i] = (char)('0' + (n % 10u));
        n /= 10u;
    }
}

static void format_u(char *dst, unsigned value) {
    char tmp[8];
    int n = 0;
    int used = 0;
    do {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && n < 8);
    while (n > 0) {
        dst[used++] = tmp[--n];
    }
    dst[used] = 0;
}

static void add_score(int base) {
    int m = combo > 0 ? combo : 1;
    score += base * m;
    if (score > 999999) {
        score = 999999;
    }
}

static int slot_of_row(int row) {
    int i;
    for (i = 0; i < RING; i++) {
        if (chunk_base[i] >= 0 && row >= chunk_base[i] && row < chunk_base[i] + CH) {
            return i;
        }
    }
    return -1;
}

static int tile_at(int wx, int wy) {
    int col, row, s, tr;
    if (wx < 0 || wx >= PLAY_W || wy < 0) {
        return TILE_AIR;
    }
    col = wx >> 4;
    row = wy >> 4;
    s = slot_of_row(row);
    if (s < 0) {
        return TILE_AIR;
    }
    tr = 11 - (row - chunk_base[s]);
    return map[s][tr][col];
}

static void set_tile_rc(int row, int col, int id) {
    int s, tr;
    if (col < 0 || col >= COLS) {
        return;
    }
    s = slot_of_row(row);
    if (s < 0) {
        return;
    }
    tr = 11 - (row - chunk_base[s]);
    map[s][tr][col] = (uint8_t)id;
}

static int is_solid(int t) {
    return t == TILE_SOLID || t == TILE_THORN || t == TILE_CRUMBLE || t == TILE_TURRET_L ||
           t == TILE_TURRET_R;
}

static int is_oneway(int t) { return t == TILE_ONEWAY; }

static int is_grapple(int t) {
    return t == TILE_SOLID || t == TILE_ONEWAY || t == TILE_CRUMBLE || t == TILE_TURRET_L ||
           t == TILE_TURRET_R;
}

static int is_reject(int t) { return t == TILE_THORN || t == TILE_SPIKE_DN; }

static int point_solid(int x, int y) { return is_solid(tile_at(x, y)); }

static int box_solid(int x, int feet) {
    int l = x + 3;
    int r = x + 12;
    int y0 = feet + 2;
    int y1 = feet + 8;
    int y2 = feet + 12;
    return point_solid(l, y0) || point_solid(r, y0) || point_solid(l, y1) || point_solid(r, y1) ||
           point_solid(l, y2) || point_solid(r, y2);
}

static int ceiling_at(int x, int feet) {
    int head = feet + 14;
    return point_solid(x + 4, head) || point_solid(x + 11, head);
}

static void arm_crumble(int wx, int wy) {
    int row, col, i;
    if (tile_at(wx, wy) != TILE_CRUMBLE) {
        return;
    }
    row = wy >> 4;
    col = wx >> 4;
    for (i = 0; i < crumb_n; i++) {
        if (crumb[i].row == row && crumb[i].col == col) {
            return;
        }
    }
    if (crumb_n >= CRUMBS) {
        return;
    }
    crumb[crumb_n].row = (int16_t)row;
    crumb[crumb_n].col = (int16_t)col;
    crumb[crumb_n].t = 24;
    crumb_n++;
}

static void crumbs_tick(void) {
    int i = 0;
    while (i < crumb_n) {
        if (crumb[i].t > 0) {
            crumb[i].t--;
        }
        if (crumb[i].t == 0) {
            set_tile_rc(crumb[i].row, crumb[i].col, TILE_AIR);
            snd_voice(1, SND_NOISE, 500, 80, 40);
            crumb[i] = crumb[crumb_n - 1];
            crumb_n--;
            continue;
        }
        i++;
    }
}

static int crumb_time(int row, int col) {
    int i;
    for (i = 0; i < crumb_n; i++) {
        if (crumb[i].row == row && crumb[i].col == col) {
            return crumb[i].t;
        }
    }
    return -1;
}

static int ent_new(void) {
    int i;
    for (i = 0; i < ENTS; i++) {
        if (ent[i].kind == EK_NONE) {
            return i;
        }
    }
    return -1;
}

static void ent_clear_band(int y0, int y1) {
    int i;
    for (i = 0; i < ENTS; i++) {
        if (ent[i].kind == EK_NONE || ent[i].kind == EK_BULLET || ent[i].kind == EK_SPARK) {
            continue;
        }
        if (ent[i].y >= y0 && ent[i].y < y1) {
            ent[i].kind = EK_NONE;
        }
    }
}

static void spawn_at(int kind, int x, int y, int dir) {
    int i = ent_new();
    struct ent *e;
    if (i < 0) {
        return;
    }
    e = &ent[i];
    e->kind = (uint8_t)kind;
    e->mode = 0;
    e->dir = (uint8_t)(dir ? 1 : 0);
    e->flags = 0;
    e->x = (int16_t)x;
    e->y = (int16_t)y;
    e->vx = 0;
    e->vy = 0;
    e->t = 0;
}

static int floor_char(char ch) {
    return ch == '#' || ch == '-' || ch == '~' || ch == 't' || ch == 'L' || ch == 'R';
}

static void open_ceiling(int base) {
    int s = slot_of_row(base + 11);
    int c;
    if (s < 0) {
        return;
    }
    for (c = 2; c <= 9; c++) {
        map[s][0][c] = TILE_AIR;
    }
}

static const char *schema_of(int n) {
    int play;
    if (n >= 36) {
        return CAP_MAP;
    }
    if (n == 35 || (n % 9) == 8) {
        return REST_MAP;
    }
    play = n - n / 9;
    return COURSE[play / 8][play % 8];
}

static void gen_chunk(int slot, int base) {
    const char *rows;
    int n = base / CH;
    int tr, c;
    int quiet = 0;
    if (chunk_base[slot] >= 0) {
        int old = chunk_base[slot];
        ent_clear_band(old * TILE - 8, (old + CH) * TILE + 24);
    }
    chunk_base[slot] = base;
    rows = schema_of(n);
    if (n >= 36) {
        quiet = 1;
    } else if (n == 35) {
        peak_base = base;
        quiet = 1;
    } else if ((n % 9) == 8) {
        rest_base = base;
        quiet = 1;
    }

    for (tr = 0; tr < CH; tr++) {
        for (c = 0; c < COLS; c++) {
            char ch = rows[tr * COLS + c];
            int id = TILE_AIR;
            int tile_row = base + (11 - tr);
            switch (ch) {
            case '#':
                id = TILE_SOLID;
                break;
            case '-':
                id = TILE_ONEWAY;
                break;
            case '~':
                id = TILE_CRUMBLE;
                break;
            case '^':
                id = TILE_SPIKE_UP;
                break;
            case 'v':
                id = TILE_SPIKE_DN;
                break;
            case 't':
                id = TILE_THORN;
                break;
            case 'L':
                id = TILE_TURRET_L;
                break;
            case 'R':
                id = TILE_TURRET_R;
                break;
            case '*':
                id = TILE_PORTAL;
                break;
            default:
                break;
            }
            map[slot][tr][c] = (uint8_t)id;
            if (ch == 'C') {
                spawn_at(EK_CRYSTAL, c * TILE, tile_row * TILE + 2, 0);
            }
            if (!quiet && (ch == 'G' || ch == 'U' || ch == 'B' || ch == 'O')) {
                char below = (tr < 11) ? rows[(tr + 1) * COLS + c] : '.';
                int kind = ch == 'G' ? EK_GULL : ch == 'U' ? EK_TURTLE : ch == 'B' ? EK_BAT : EK_ORB;
                if (kind == EK_TURTLE && !floor_char(below)) {
                    kind = EK_GULL;
                }
                spawn_at(kind, c * TILE, tile_row * TILE, c < 6);
            }
        }
    }
}

static void ensure_chunks(void) {
    int guard = 0;
    int need = (cam_top >> 4) + 2;
    while (guard++ < 4) {
        int hi = chunk_base[0];
        int slot = 0;
        int i;
        int nb;
        for (i = 1; i < RING; i++) {
            if (chunk_base[i] > hi) {
                hi = chunk_base[i];
            }
            if (chunk_base[i] < chunk_base[slot]) {
                slot = i;
            }
        }
        if (hi + CH > need || (peak_base >= 0 && hi >= peak_base)) {
            break;
        }
        nb = hi + CH;
        gen_chunk(slot, nb);
    }
}

static int spike_phase(void) {
    int p = clock % 120;
    if (p < 54) {
        return 0;
    }
    if (p < 60) {
        return 1;
    }
    return 2;
}

static void spawn_spark(int x, int y) {
    int i = ent_new();
    if (i < 0) {
        return;
    }
    ent[i].kind = EK_SPARK;
    ent[i].x = (int16_t)x;
    ent[i].y = (int16_t)y;
    ent[i].vx = (int16_t)((int)(rnd() % 5u) - 2);
    ent[i].vy = (int16_t)(2 + (int)(rnd() % 3u));
    ent[i].t = 12;
    ent[i].mode = 0;
    ent[i].dir = 0;
    ent[i].flags = 0;
}

static void kill_ent(struct ent *e, int pts) {
    if (combo < 9) {
        combo++;
    }
    add_score(pts);
    spawn_spark(e->x, e->y);
    spawn_spark(e->x + 6, e->y + 4);
    e->kind = EK_NONE;
}

static void explode_bat(struct ent *e, int award) {
    int dx = (px + 8) - (e->x + 8);
    int dy = (py + 8) - (e->y + 8);
    int col = (e->x + 8) >> 4;
    int row = (e->y + 8) >> 4;
    int dr, dc;
    if (dx * dx + dy * dy < 26 * 26) {
        award = 0;
    }
    for (dr = -1; dr <= 1; dr++) {
        for (dc = -1; dc <= 1; dc++) {
            int r = row + dr;
            int c = col + dc;
            int s = slot_of_row(r);
            int tr;
            if (s < 0 || c < 0 || c >= COLS) {
                continue;
            }
            tr = 11 - (r - chunk_base[s]);
            if (map[s][tr][c] == TILE_CRUMBLE) {
                map[s][tr][c] = TILE_AIR;
            }
        }
    }
    snd_voice(1, SND_NOISE, 180, 200, 140);
    spawn_spark(e->x, e->y);
    spawn_spark(e->x + 8, e->y + 6);
    if (award) {
        kill_ent(e, 150);
    } else {
        e->mode = 2;
        e->t = 8;
        e->vx = 0;
        e->vy = 0;
    }
}

static void hurt(int from_x) {
    if (hurt_t > 0 || hearts <= 0 || state != ST_PLAY) {
        return;
    }
    hearts--;
    hurt_t = 42;
    shake = 8;
    combo = 0;
    vy = 860;
    vx = (px + 8 < from_x) ? -760 : 760;
    grapple = 0;
    sfx_hurt();
    if (hearts <= 0) {
        hearts = 0;
        state = ST_DIE;
        die_t = 46;
    }
}

static void shoot_aimed(int left, int right) {
    int i;
    struct ent *e;
    if (ammo <= 0) {
        return;
    }
    i = ent_new();
    if (i < 0) {
        return;
    }
    ammo--;
    e = &ent[i];
    e->kind = EK_BULLET;
    e->flags = (uint8_t)(pierce ? 1 : 0);
    e->x = (int16_t)(px + 4);
    e->y = (int16_t)(py + 2);
    e->vx = (int16_t)(left ? -2 : right ? 2 : 0);
    e->vy = -4;
    e->t = 22;
    e->mode = 0;
    e->dir = (uint8_t)(face < 0 ? 0 : 1);
    vy += RECOIL;
    if (vy > MAX_RISE) {
        vy = MAX_RISE;
    }
    muzzle_t = 4;
    sfx_shot();
}

static void start_grapple(int left, int right) {
    grapple = 1;
    hook_t = 0;
    hook_x = px + 8;
    hook_y = py + 12;
    hook_vx = left ? -2 : right ? 2 : 0;
    hook_vy = (left || right) ? 4 : 5;
    sfx_hook_fly();
}

static void hook_reject(void) {
    spawn_spark(hook_x - 4, hook_y);
    snd_voice(1, SND_NOISE, 1400, 120, 40);
    grapple = 0;
}

static void fly_hook(void) {
    int range = rope ? ROPE_LONG : ROPE_BASE;
    int step;
    hook_t++;
    if (hook_t > 28) {
        grapple = 0;
        return;
    }
    for (step = 0; step < 2; step++) {
        int i;
        hook_x += hook_vx;
        hook_y += hook_vy;
        if (hook_x < 8) {
            hook_x = 8;
        }
        if (hook_x > PLAY_W - 8) {
            hook_x = PLAY_W - 8;
        }
        {
            int dx = hook_x - (px + 8);
            int dy = hook_y - (py + 10);
            if (dx * dx + dy * dy > range * range) {
                grapple = 0;
                return;
            }
        }
        for (i = 0; i < ENTS; i++) {
            struct ent *e = &ent[i];
            int ew = 16, eh = 16;
            if (e->kind != EK_GULL && e->kind != EK_TURTLE && e->kind != EK_BAT && e->kind != EK_ORB) {
                continue;
            }
            if (hook_x < e->x || hook_x >= e->x + ew || hook_y < e->y || hook_y >= e->y + eh) {
                continue;
            }
            if (e->kind == EK_ORB) {
                hook_reject();
                return;
            }
            if (e->kind == EK_TURTLE) {
                e->t = 70;
            } else if (e->kind == EK_GULL) {
                e->t = 60;
            } else {
                e->mode = 0;
                e->t = 60;
                e->vy = 0;
            }
            sfx_hook_hit();
            grapple = 2;
            hook_t = 0;
            return;
        }
        {
            int t = tile_at(hook_x, hook_y);
            if (is_reject(t)) {
                hook_reject();
                return;
            }
            if (is_grapple(t)) {
                if (t == TILE_ONEWAY || t == TILE_CRUMBLE) {
                    hook_y = (hook_y & ~15) + 16;
                }
                sfx_hook_hit();
                grapple = 2;
                hook_t = 0;
                return;
            }
        }
    }
}

static void pull_player(void) {
    int tx = hook_x - (px + 8);
    int ty = hook_y - (py + 8);
    int step = rope ? 5 : 4;
    int guard = 0;
    hook_t++;
    if (hook_t > 36 || (iabs(tx) < 8 && iabs(ty) < 8)) {
        grapple = 0;
        vy = 1100;
        grounded = 0;
        jumps = boots ? 1 : 0;
        return;
    }
    while (guard++ < step) {
        int mx = 0;
        int my = 0;
        if (iabs(tx) > iabs(ty)) {
            mx = tx > 0 ? 1 : -1;
        } else if (ty != 0) {
            my = ty > 0 ? 1 : -1;
        }
        if (mx && !box_solid(px + mx, py)) {
            px += mx;
            tx -= mx;
        } else if (mx) {
            vx = 0;
            mx = 0;
        }
        if (my > 0 && !ceiling_at(px, py)) {
            py += 1;
            ty -= 1;
        } else if (my < 0 && !point_solid(px + 4, py - 1) && !point_solid(px + 11, py - 1)) {
            py -= 1;
            ty += 1;
        } else {
            break;
        }
    }
}

static void move_x(int dx) {
    int step = dx > 0 ? 1 : -1;
    int guard = 0;
    while (dx != 0 && guard++ < 8) {
        if (box_solid(px + step, py)) {
            vx = 0;
            px_acc = 0;
            break;
        }
        px += step;
        dx -= step;
    }
    if (px < 0) {
        px = 0;
    }
    if (px > PLAY_W - 16) {
        px = PLAY_W - 16;
    }
}

static void move_y(int dy) {
    int guard = 0;
    if (dy > 0) {
        while (dy > 0 && guard++ < 8) {
            if (ceiling_at(px, py)) {
                vy = 0;
                py_acc = 0;
                break;
            }
            py += 1;
            dy -= 1;
        }
        return;
    }
    while (dy < 0 && guard++ < 8) {
        int below = py - 1;
        int t1, t2, top, land;
        if (below < 0) {
            py -= 1;
            dy += 1;
            continue;
        }
        t1 = tile_at(px + 4, below);
        t2 = tile_at(px + 11, below);
        top = (below & ~15) + 16;
        land = 0;
        if (is_solid(t1) || is_solid(t2)) {
            land = 1;
        } else if ((is_oneway(t1) || is_oneway(t2)) && py >= top) {
            land = 1;
        }
        if (land) {
            py = top;
            vy = 0;
            py_acc = 0;
            grounded = 1;
            if (is_solid(t1) && tile_at(px + 4, below) == TILE_CRUMBLE) {
                arm_crumble(px + 4, below);
            }
            if (tile_at(px + 11, below) == TILE_CRUMBLE) {
                arm_crumble(px + 11, below);
            }
            break;
        }
        py -= 1;
        dy += 1;
    }
}

static void probe_floor(void) {
    int below, t1, t2, top;
    if (grounded || vy > 0 || py <= 0) {
        return;
    }
    below = py - 1;
    if (below < 0) {
        return;
    }
    t1 = tile_at(px + 4, below);
    t2 = tile_at(px + 11, below);
    top = (below & ~15) + 16;
    if (py > top + 2) {
        return;
    }
    if (is_solid(t1) || is_solid(t2) || ((is_oneway(t1) || is_oneway(t2)) && py >= top)) {
        py = top;
        vy = 0;
        grounded = 1;
        if (tile_at(px + 4, below) == TILE_CRUMBLE) {
            arm_crumble(px + 4, below);
        }
        if (tile_at(px + 11, below) == TILE_CRUMBLE) {
            arm_crumble(px + 11, below);
        }
    }
}

static void pixels_of(int *acc, int vel, int *out) {
    *acc += vel;
    *out = *acc / 256;
    *acc -= *out * 256;
}

static void player_update(const lippus_pad_t *pad) {
    int left = pad_held(pad, LIPPUS_LEFT);
    int right = pad_held(pad, LIPPUS_RIGHT);
    int up = pad_held(pad, LIPPUS_UP);
    int dx = 0;
    int dy = 0;
    int was_ground;
    int accel;

    if (pad_pressed(pad, LIPPUS_A)) {
        jump_buf = 6;
    } else if (jump_buf > 0) {
        jump_buf--;
    }
    if (left && !right) {
        face = -1;
    } else if (right && !left) {
        face = 1;
    }

    accel = grounded ? RUN_ACC : AIR_ACC;
    if (left && !right) {
        vx -= accel;
    } else if (right && !left) {
        vx += accel;
    } else if (grapple != 2) {
        if (grounded) {
            if (vx > RUN_FRIC) {
                vx -= RUN_FRIC;
            } else if (vx < -RUN_FRIC) {
                vx += RUN_FRIC;
            } else {
                vx = 0;
            }
        } else {
            vx -= vx / 6;
        }
    }
    if (state == ST_PLAY && biome_of(py) == 2) {
        vx += ((clock / 120) & 1) ? 38 : -38;
    }
    if (vx > RUN_MAX) {
        vx = RUN_MAX;
    }
    if (vx < -RUN_MAX) {
        vx = -RUN_MAX;
    }

    if (grapple != 0 && pad_pressed(pad, LIPPUS_A)) {
        grapple = 0;
    }
    if (pad_pressed(pad, LIPPUS_B) && grapple == 0) {
        int aerial = !grounded && coyote == 0;
        if (aerial && !up) {
            shoot_aimed(left, right);
        } else {
            start_grapple(left, right);
        }
    }

    was_ground = grounded;
    grounded = 0;
    pixels_of(&px_acc, vx, &dx);
    move_x(dx);

    if (grapple == 2) {
        pull_player();
    } else {
        if (grapple == 1) {
            fly_hook();
        }
        vy -= GRAV;
        if (vy < MAX_FALL) {
            vy = MAX_FALL;
        }
        if (jump_buf > 0 && grapple != 2) {
            int can = (jumps == 0 && (was_ground || coyote > 0)) || (boots && jumps == 1 && !was_ground);
            if (can) {
                int dub = !(was_ground || coyote > 0);
                vy = JUMP_V;
                jumps = dub ? 2 : 1;
                jump_buf = 0;
                coyote = 0;
                was_ground = 0;
                sfx_jump(dub);
            }
        }
        if (!pad_held(pad, LIPPUS_A) && vy > JUMP_CUT) {
            vy = JUMP_CUT;
        }
        pixels_of(&py_acc, vy, &dy);
        move_y(dy);
        probe_floor();
    }

    if (grounded) {
        jumps = 0;
        combo = 0;
        coyote = 6;
        if (!was_ground) {
            ammo = 4;
        }
    } else if (coyote > 0) {
        coyote--;
    }

    if (hurt_t > 0) {
        hurt_t--;
    }
    if (muzzle_t > 0) {
        muzzle_t--;
    }
    if (py < -30) {
        death_lava = 1;
        hearts = 0;
        state = ST_DIE;
        die_t = 30;
    }
}

static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void bounce_on(struct ent *e, int kill, int pts) {
    (void)e;
    vy = 1000;
    py_acc = 0;
    grounded = 0;
    jumps = 1;
    ammo = 4;
    jump_buf = 0;
    sfx_stomp();
    if (kill) {
        kill_ent(e, pts);
    }
}

static void player_vs_ents(int prev_py) {
    int i;
    int hx = px + 2;
    for (i = 0; i < ENTS; i++) {
        struct ent *e = &ent[i];
        int ew = 16;
        int eh = 16;
        int etop;
        int stomp;
        if (e->kind == EK_NONE || e->kind == EK_BULLET || e->kind == EK_SPARK) {
            continue;
        }
        if (e->kind == EK_CRYSTAL) {
            if (overlap(hx, py, 12, 14, e->x + 2, e->y, 12, 14)) {
                crystals++;
                add_score(50);
                sfx_crystal();
                if (magnet && crystals > 0 && (crystals % 50) == 0 && hearts < 3) {
                    hearts++;
                }
                e->kind = EK_NONE;
            }
            continue;
        }
        if (!overlap(hx, py, 12, 14, e->x, e->y, ew, eh)) {
            continue;
        }
        etop = e->y + eh;
        stomp = vy < 0 && prev_py >= etop - 4 && py <= etop + 6;
        if (e->kind == EK_GULL) {
            if (e->t > 0) {
                if (stomp) {
                    bounce_on(e, 0, 0);
                }
            } else if (stomp) {
                bounce_on(e, 1, 100);
            } else {
                hurt(e->x + 8);
            }
        } else if (e->kind == EK_TURTLE) {
            if (stomp) {
                bounce_on(e, e->t > 0, 200);
            } else if (e->t == 0) {
                hurt(e->x + 8);
            }
        } else if (e->kind == EK_BAT) {
            if (e->mode == 2) {
                hurt(e->x + 8);
            } else if (e->t > 0 && e->mode == 0 && stomp) {
                bounce_on(e, 0, 0);
            } else if (stomp) {
                bounce_on(e, 1, 150);
            } else {
                hurt(e->x + 8);
            }
        } else if (e->kind == EK_FIRE || e->kind == EK_ORB) {
            hurt(e->x + 8);
        }
    }
}

static void bullet_vs(struct ent *b) {
    int i;
    for (i = 0; i < ENTS; i++) {
        struct ent *e = &ent[i];
        if (e == b || e->kind == EK_NONE || e->kind == EK_BULLET || e->kind == EK_SPARK ||
            e->kind == EK_CRYSTAL) {
            continue;
        }
        if (!overlap(b->x, b->y, 8, 8, e->x, e->y, 16, 16)) {
            continue;
        }
        if (e->kind == EK_GULL) {
            kill_ent(e, 100);
            if ((b->flags & 1) == 0) {
                b->kind = EK_NONE;
                return;
            }
        } else if (e->kind == EK_BAT && e->mode != 2) {
            kill_ent(e, 150);
            if ((b->flags & 1) == 0) {
                b->kind = EK_NONE;
                return;
            }
        } else if (e->kind == EK_TURTLE) {
            if ((b->flags & 1) != 0 || e->t > 0) {
                kill_ent(e, 200);
                if ((b->flags & 1) == 0) {
                    b->kind = EK_NONE;
                    return;
                }
            } else if (b->vx != 0) {
                e->t = 70;
                sfx_stomp();
                b->kind = EK_NONE;
                return;
            } else {
                b->kind = EK_NONE;
                return;
            }
        } else if (e->kind == EK_FIRE) {
            e->kind = EK_NONE;
            b->kind = EK_NONE;
            return;
        } else if (e->kind == EK_ORB) {
            if ((b->flags & 1) == 0) {
                b->kind = EK_NONE;
                return;
            }
        }
    }
}

static void ents_update(int prev_py) {
    int i;
    for (i = 0; i < ENTS; i++) {
        struct ent *e = &ent[i];
        if (e->kind == EK_NONE) {
            continue;
        }
        if (e->kind == EK_SPARK) {
            e->x = (int16_t)(e->x + e->vx);
            e->y = (int16_t)(e->y + e->vy);
            e->vy = (int16_t)(e->vy - 1);
            e->t--;
            if (e->t <= 0) {
                e->kind = EK_NONE;
            }
            continue;
        }
        if (e->kind == EK_BULLET) {
            e->x = (int16_t)(e->x + e->vx);
            e->y = (int16_t)(e->y + e->vy);
            e->t--;
            if (e->t <= 0 || e->x < 0 || e->x > PLAY_W || point_solid(e->x + 3, e->y + 3)) {
                e->kind = EK_NONE;
                continue;
            }
            bullet_vs(e);
            continue;
        }
        if (e->kind == EK_CRYSTAL) {
            if (magnet) {
                int dx = (px + 8) - (e->x + 8);
                int dy = (py + 8) - (e->y + 8);
                if (dx * dx + dy * dy < 52 * 52) {
                    if (dx > 3) {
                        e->x = (int16_t)(e->x + 2);
                    } else if (dx < -3) {
                        e->x = (int16_t)(e->x - 2);
                    }
                    if (dy > 3) {
                        e->y = (int16_t)(e->y + 2);
                    } else if (dy < -3) {
                        e->y = (int16_t)(e->y - 2);
                    }
                }
            }
            continue;
        }
        if (e->kind == EK_FIRE) {
            e->x = (int16_t)(e->x + e->vx);
            e->t--;
            if (e->t <= 0 || point_solid(e->x + 8, e->y + 8) || e->x < -16 || e->x > PLAY_W) {
                e->kind = EK_NONE;
            }
            continue;
        }
        if (e->kind == EK_GULL) {
            int nx;
            if (e->t > 0) {
                e->t--;
                continue;
            }
            nx = e->x + (e->dir ? 1 : -1);
            if (nx < 16 || nx > PLAY_W - 32 || point_solid(nx + (e->dir ? 14 : 0), e->y + 8)) {
                e->dir = (uint8_t)(e->dir ? 0 : 1);
            } else {
                e->x = (int16_t)nx;
            }
            continue;
        }
        if (e->kind == EK_ORB) {
            int nx = e->x + (e->dir ? 1 : -1);
            if (nx < 16 || nx > PLAY_W - 32 || point_solid(nx + (e->dir ? 14 : 0), e->y + 8)) {
                e->dir = (uint8_t)(e->dir ? 0 : 1);
            } else {
                e->x = (int16_t)nx;
            }
            continue;
        }
        if (e->kind == EK_TURTLE) {
            int nx, ahead, foot;
            if (e->t > 0) {
                e->t--;
                continue;
            }
            nx = e->x + (e->dir ? 1 : -1);
            ahead = e->dir ? nx + 14 : nx;
            foot = point_solid(ahead, e->y - 2) || is_oneway(tile_at(ahead, e->y - 2)) ||
                   tile_at(ahead, e->y - 2) == TILE_CRUMBLE;
            if (nx < 16 || nx > PLAY_W - 32 || point_solid(ahead, e->y + 4) || !foot) {
                e->dir = (uint8_t)(e->dir ? 0 : 1);
            } else {
                e->x = (int16_t)nx;
            }
            continue;
        }
        if (e->kind == EK_BAT) {
            if (e->mode == 2) {
                e->t--;
                if (e->t <= 0) {
                    if (overlap(px + 2, py, 12, 14, e->x - 6, e->y - 6, 28, 28)) {
                        hurt(e->x + 8);
                    }
                    e->kind = EK_NONE;
                }
                continue;
            }
            if (e->t > 0 && e->mode == 0) {
                e->t--;
                continue;
            }
            if (e->mode == 0) {
                if (iabs((px + 8) - (e->x + 8)) < 22 && py + 8 < e->y) {
                    e->mode = 1;
                    e->t = 0;
                }
            } else {
                e->y = (int16_t)(e->y - 2);
                e->t++;
                if (e->t > 30 || point_solid(e->x + 8, e->y)) {
                    explode_bat(e, 1);
                }
            }
        }
    }
    player_vs_ents(prev_py);
}

static void turrets_fire(void) {
    int wave = clock / 60;
    int s, tr, c;
    if (wave == last_wave) {
        return;
    }
    last_wave = wave;
    for (s = 0; s < RING; s++) {
        if (chunk_base[s] < 0) {
            continue;
        }
        for (tr = 0; tr < CH; tr++) {
            for (c = 0; c < COLS; c++) {
                int id = map[s][tr][c];
                int row, i;
                struct ent *e;
                if (id != TILE_TURRET_L && id != TILE_TURRET_R) {
                    continue;
                }
                i = ent_new();
                if (i < 0) {
                    return;
                }
                row = chunk_base[s] + (11 - tr);
                e = &ent[i];
                e->kind = EK_FIRE;
                e->x = (int16_t)(c * TILE + (id == TILE_TURRET_L ? 12 : -8));
                e->y = (int16_t)(row * TILE);
                e->vx = (int16_t)(id == TILE_TURRET_L ? 2 : -2);
                e->vy = 0;
                e->t = 80;
                e->mode = 0;
                e->dir = (uint8_t)(id == TILE_TURRET_L ? 1 : 0);
                e->flags = 0;
            }
        }
    }
}

static void hazards(void) {
    int x0 = (px + 2) >> 4;
    int x1 = (px + 13) >> 4;
    int y0 = py >> 4;
    int y1 = (py + 13) >> 4;
    int r, c;
    int phase = spike_phase();
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 > 11) {
        x1 = 11;
    }
    for (r = y0; r <= y1; r++) {
        for (c = x0; c <= x1; c++) {
            int s = slot_of_row(r);
            int tr, id;
            if (s < 0) {
                continue;
            }
            tr = 11 - (r - chunk_base[s]);
            id = map[s][tr][c];
            if (id == TILE_SPIKE_UP && phase == 2) {
                hurt(c * TILE + 8);
            }
            if (id == TILE_SPIKE_DN) {
                hurt(c * TILE + 8);
            }
        }
    }
}

static void lava_tick(void) {
    int b = biome_of(py);
    int add = LAVA_Q8[b];
    if (b == 1 && py - lava_y > 100) {
        add += 68;
    }
    lava_acc += add;
    while (lava_acc >= 256) {
        lava_y++;
        lava_acc -= 256;
    }
    if (py <= lava_y) {
        death_lava = 1;
        hearts = 0;
        sfx_hurt();
        state = ST_DIE;
        die_t = 36;
    }
}

static void cam_update(void) {
    int target = py + 156;
    int d;
    if (target < 239) {
        target = 239;
    }
    d = target - cam_top;
    if (d > 3) {
        cam_top += d / 3;
    } else if (d < -3) {
        cam_top += d / 2;
    } else {
        cam_top = target;
    }
}

static int score_qualifies(int sc) {
    if (sc <= 0) {
        return 0;
    }
    if (score_n < SCORE_N) {
        return 1;
    }
    return sc > scores[score_n - 1].score;
}

static void scores_save(void) {
    int n = 0;
    int i;
    for (i = 0; i < score_n; i++) {
        char num[8];
        int k;
        if (n + 16 >= (int)LIPPUS_RECORD_MAX) {
            break;
        }
        rec[n++] = scores[i].name[0];
        rec[n++] = scores[i].name[1];
        rec[n++] = scores[i].name[2];
        rec[n++] = ' ';
        format_u(num, (unsigned)scores[i].score);
        for (k = 0; num[k]; k++) {
            rec[n++] = num[k];
        }
        rec[n++] = '\n';
    }
    sys_record_write("SKYWARD", rec, n);
}

static void scores_load(void) {
    int got;
    int i = 0;
    score_n = 0;
    got = sys_record_read("SKYWARD", rec, (int)LIPPUS_RECORD_MAX - 1);
    if (got <= 0) {
        return;
    }
    if (got > (int)LIPPUS_RECORD_MAX - 1) {
        got = (int)LIPPUS_RECORD_MAX - 1;
    }
    rec[got] = 0;
    /* Il vecchio salvataggio teneva cinque nomi in 40 byte. */
    if (got == 40 && !(rec[0] >= 'A' && rec[0] <= 'Z' && rec[3] == ' ')) {
        for (i = 0; i < 5; i++) {
            int k;
            unsigned sc = 0;
            for (k = 0; k < 3; k++) {
                char ch = rec[i * 8 + k];
                if (ch < 'A' || ch > 'Z') {
                    ch = 'A';
                }
                scores[score_n].name[k] = ch;
            }
            sc = (unsigned)(uint8_t)rec[i * 8 + 4] | ((unsigned)(uint8_t)rec[i * 8 + 5] << 8) |
                 ((unsigned)(uint8_t)rec[i * 8 + 6] << 16) | ((unsigned)(uint8_t)rec[i * 8 + 7] << 24);
            if (sc > 0 && score_n < SCORE_N) {
                scores[score_n].score = (int)sc;
                score_n++;
            }
        }
        scores_save();
        return;
    }
    i = 0;
    while (rec[i] && score_n < SCORE_N) {
        int used = 0;
        int sc;
        char a, b, c;
        if (rec[i] == '\n' || rec[i] == '\r') {
            i++;
            continue;
        }
        if (i + 3 >= got) {
            break;
        }
        a = rec[i];
        b = rec[i + 1];
        c = rec[i + 2];
        if (a < 'A' || a > 'Z' || b < 'A' || b > 'Z' || c < 'A' || c > 'Z' || rec[i + 3] != ' ') {
            while (rec[i] && rec[i] != '\n') {
                i++;
            }
            continue;
        }
        sc = 0;
        i += 4;
        while (rec[i] >= '0' && rec[i] <= '9' && used < 9) {
            sc = sc * 10 + (rec[i] - '0');
            i++;
            used++;
        }
        if (used > 0 && sc > 0) {
            scores[score_n].name[0] = a;
            scores[score_n].name[1] = b;
            scores[score_n].name[2] = c;
            scores[score_n].score = sc;
            score_n++;
        }
        while (rec[i] && rec[i] != '\n') {
            i++;
        }
    }
}

static void score_insert(int sc) {
    int at = score_n;
    int last;
    int i;
    for (i = 0; i < score_n; i++) {
        if (sc > scores[i].score) {
            at = i;
            break;
        }
    }
    if (at >= SCORE_N) {
        rank_hi = -1;
        return;
    }
    last = score_n < SCORE_N ? score_n : SCORE_N - 1;
    for (i = last; i > at; i--) {
        scores[i] = scores[i - 1];
    }
    scores[at].name[0] = letters[0];
    scores[at].name[1] = letters[1];
    scores[at].name[2] = letters[2];
    scores[at].score = sc;
    if (score_n < SCORE_N) {
        score_n++;
    }
    rank_hi = at;
}

static void enter_attract(int page) {
    attract_page = page;
    attract_t = 0;
    state = ST_TITLE;
}

static void enter_name(void) {
    letters[0] = 'A';
    letters[1] = 'A';
    letters[2] = 'A';
    name_pos = 0;
    letter_hold = 0;
    state = ST_NAME;
}

static void finish_run(int won) {
    won_flag = won;
    if (won) {
        int bonus = (py / TILE) * 5;
        if (bonus < 0) {
            bonus = 0;
        }
        score += bonus;
        if (score > 999999) {
            score = 999999;
        }
    }
    end_t = 90;
    state = won ? ST_WIN : ST_OVER;
}

static void new_game(void) {
    int i;
    for (i = 0; i < ENTS; i++) {
        ent[i].kind = EK_NONE;
    }
    crumb_n = 0;
    for (i = 0; i < RING; i++) {
        chunk_base[i] = -1;
    }
    rest_base = -1;
    peak_base = -1;
    rest_done = -1;
    rng = sys_ticks() | 1u;
    gen_chunk(0, 0);
    gen_chunk(1, CH);
    gen_chunk(2, CH * 2);
    px = 88;
    py = 16;
    vx = 0;
    vy = 0;
    px_acc = 0;
    py_acc = 0;
    face = 1;
    grounded = 1;
    coyote = 6;
    jump_buf = 0;
    jumps = 0;
    hearts = 3;
    hurt_t = 0;
    ammo = 4;
    muzzle_t = 0;
    score = 0;
    combo = 0;
    crystals = 0;
    rope = 0;
    pierce = 0;
    boots = 0;
    magnet = 0;
    lava_y = -96;
    lava_acc = 0;
    death_lava = 0;
    won_flag = 0;
    grapple = 0;
    hook_t = 0;
    clock = 0;
    shake = 0;
    cam_top = 239;
    last_wave = 0;
    offer_sel = 0;
}

static int sy_of(int world_bottom, int h) { return cam_top - (world_bottom + h - 1) + draw_dy; }

static void blit_idx(int x, int y, const uint8_t *src, int w, int h, int flip) {
    int row, col;
    if (w <= 0 || h <= 0 || w * h > 256) {
        return;
    }
    for (row = 0; row < h; row++) {
        const uint8_t *s = src + row * w;
        uint16_t *d = scratch + row * w;
        for (col = 0; col < w; col++) {
            uint8_t ix = flip ? s[w - 1 - col] : s[col];
            d[col] = ix ? PAL[ix] : PAL[0];
        }
    }
    gfx_blit(x, y, w, h, scratch, 1, PAL[0]);
}

static const uint8_t *sprite_ptr(const uint8_t (*rows)[16]) { return &rows[0][0]; }

static void spr(int x, int y, const uint8_t *src, int scale) {
    int row, col;
    for (row = 0; row < 16; row++) {
        const uint8_t *s = src + row * 16;
        uint16_t *d = scratch + row * 16;
        for (col = 0; col < 16; col++) {
            uint8_t ix = s[col];
            d[col] = ix ? PAL[ix] : PAL[0];
        }
    }
    gfx_blit(x, y, 16, 16, scratch, scale, PAL[0]);
}

static void draw_tiles(void) {
    int top = cam_top >> 4;
    int bot = (cam_top - 239) >> 4;
    int row, col;
    int phase = spike_phase();
    if (bot < 0) {
        bot = 0;
    }
    for (row = top; row >= bot; row--) {
        int biome = biome_of(row * TILE);
        int sy = sy_of(row * TILE, 16);
        int s = slot_of_row(row);
        if (biome > 3) {
            biome = 3;
        }
        for (col = 0; col < COLS; col++) {
            int sx = PLAY_X + col * TILE;
            int id = TILE_AIR;
            int tr = 0;
            if (s >= 0) {
                tr = 11 - (row - chunk_base[s]);
                id = map[s][tr][col];
            }
            if (id == TILE_AIR) {
                continue;
            }
            if (id == TILE_SOLID) {
                blit_idx(sx, sy, sprite_ptr(BIOME_BACKGROUNDS[biome]), 16, 16, 0);
            } else if (id == TILE_ONEWAY) {
                blit_idx(sx, sy, sprite_ptr(BIOME_PLATFORMS[biome]), 16, 16, 0);
            } else if (id == TILE_CRUMBLE) {
                int ct = crumb_time(row, col);
                const uint8_t *sp = (ct >= 0 && ct < 10) ? sprite_ptr(TILE_CRACKED_CRUMBLE)
                                                         : &TILE_CRACKED_BRICK[0][0];
                blit_idx(sx, sy, sp, 16, 16, 0);
            } else if (id == TILE_SPIKE_UP) {
                const uint8_t *sp = &TILE_SPIKES_RETRACTED[0][0];
                if (phase == 1) {
                    sp = &TILE_SPIKES_WARNING[0][0];
                } else if (phase == 2) {
                    sp = &TILE_SPIKES_ACTIVE[0][0];
                }
                blit_idx(sx, sy, sp, 16, 16, 0);
            } else if (id == TILE_SPIKE_DN) {
                blit_idx(sx, sy, &TILE_SPIKES_ACTIVE[0][0], 16, 16, 0);
            } else if (id == TILE_THORN) {
                const uint8_t *sp =
                    (biome >= 1 && ((col + row) & 1)) ? &TILE_ELECTRIC_CABLE[0][0] : &TILE_THORNS[0][0];
                blit_idx(sx, sy, sp, 16, 16, 0);
            } else if (id == TILE_TURRET_L || id == TILE_TURRET_R) {
                int p = clock % 60;
                int fr = 0;
                if (p < 6) {
                    fr = 2;
                } else if (p >= 30) {
                    fr = 1;
                }
                blit_idx(sx, sy, sprite_ptr(GARGOYLE_ANIM_FRAMES[fr]), 16, 16, id == TILE_TURRET_R);
            } else if (id == TILE_PORTAL) {
                blit_idx(sx, sy, &TILE_REST_PEDESTAL[0][0], 16, 16, 0);
            }
        }
        blit_idx(PLAY_X, sy, &BIOME_WALLS_4X16[biome][0][0], 4, 16, 0);
        blit_idx(PLAY_X + PLAY_W - 4, sy, &BIOME_WALLS_4X16[biome][0][0], 4, 16, 1);
    }
}

static void draw_ents(void) {
    int i;
    int bob = (clock / 8) & 1;
    for (i = 0; i < ENTS; i++) {
        struct ent *e = &ent[i];
        int sx, sy;
        int flip;
        if (e->kind == EK_NONE) {
            continue;
        }
        if (e->kind == EK_SPARK) {
            sx = PLAY_X + e->x;
            sy = sy_of(e->y, 8);
            blit_idx(sx, sy, &SPRITE_SPARK_BOUNCE[0][0], 8, 8, 0);
            continue;
        }
        if (e->kind == EK_BULLET) {
            sx = PLAY_X + e->x;
            if (e->flags & 1) {
                sy = sy_of(e->y, 16);
                blit_idx(sx, sy, &SPRITE_BULLET_PIERCE[0][0], 8, 16, 0);
            } else {
                sy = sy_of(e->y, 8);
                blit_idx(sx, sy, &SPRITE_BULLET_STD[0][0], 8, 8, 0);
            }
            continue;
        }
        flip = e->dir ? 0 : 1;
        sx = PLAY_X + e->x;
        if (e->kind == EK_CRYSTAL) {
            sy = sy_of(e->y + bob, 16);
            blit_idx(sx, sy, &SPRITE_SOLAR_CRYSTAL[0][0], 16, 16, 0);
            continue;
        }
        if (e->kind == EK_GULL) {
            int fr = e->t > 0 ? 3 : (clock / 6) % 3;
            int oy = 0;
            if (biome_of(e->y) == 2 && e->t == 0) {
                oy = ((clock / 4) & 2) ? 2 : -2;
            }
            sy = sy_of(e->y + oy, 16);
            blit_idx(sx, sy, sprite_ptr(GULL_ANIM_FRAMES[fr]), 16, 16, flip);
            continue;
        }
        if (e->kind == EK_TURTLE) {
            int fr = (clock / 8) & 1;
            if (e->t > 0) {
                fr = e->t < 18 ? 3 : 2;
            }
            sy = sy_of(e->y, 16);
            blit_idx(sx, sy, sprite_ptr(TURTLE_ANIM_FRAMES[fr]), 16, 16, flip);
            continue;
        }
        if (e->kind == EK_BAT) {
            int fr = 0;
            if (e->mode == 2) {
                fr = 3;
            } else if (e->mode == 1) {
                fr = e->t > 15 ? 2 : 1;
            }
            sy = sy_of(e->y, 16);
            blit_idx(sx, sy, sprite_ptr(BAT_ANIM_FRAMES[fr]), 16, 16, 0);
            continue;
        }
        if (e->kind == EK_FIRE || e->kind == EK_ORB) {
            sy = sy_of(e->y, 16);
            blit_idx(sx, sy, sprite_ptr(((clock / 3) & 1) ? PROJECTILE_FIREBALL_1 : PROJECTILE_FIREBALL_0),
                     16, 16, e->kind == EK_FIRE && e->vx < 0);
        }
    }
}

static void draw_player(void) {
    const uint8_t *sp = &SPRITE_KIP_IDLE[0][0];
    int sx, sy;
    if (hurt_t > 20) {
        sp = &SPRITE_KIP_HURT[0][0];
    } else if (grapple != 0) {
        sp = &SPRITE_KIP_GRAPPLE[0][0];
    } else if (!grounded && vy > 80) {
        sp = &SPRITE_KIP_JUMP[0][0];
    } else if (!grounded) {
        sp = &SPRITE_KIP_FALL[0][0];
    } else if (vx > 40 || vx < -40) {
        sp = ((px / 8) & 1) ? &SPRITE_KIP_RUN_1[0][0] : &SPRITE_KIP_RUN_0[0][0];
    }
    if (hurt_t > 0 && (hurt_t & 2)) {
        return;
    }
    sx = PLAY_X + px;
    sy = sy_of(py, 16);
    blit_idx(sx, sy, sp, 16, 16, face < 0);
    if (muzzle_t > 0) {
        blit_idx(sx, sy_of(py - 8, 8), &SPRITE_MUZZLE_FLASH[0][0], 16, 8, 0);
    }
}

static void draw_hook(void) {
    int x0, y0, x1, y1, n, i;
    if (grapple == 0) {
        return;
    }
    x0 = PLAY_X + px + 8;
    y0 = sy_of(py + 12, 1);
    x1 = PLAY_X + hook_x;
    y1 = sy_of(hook_y, 1);
    gfx_line(x0, y0, x1, y1, PAL[12]);
    n = iabs(x1 - x0) > iabs(y1 - y0) ? iabs(x1 - x0) : iabs(y1 - y0);
    n /= 8;
    if (n > 10) {
        n = 10;
    }
    for (i = 1; i <= n; i++) {
        int x = x0 + (x1 - x0) * i / (n + 1);
        int y = y0 + (y1 - y0) * i / (n + 1);
        blit_idx(x - 4, y - 4, &SPRITE_ROPE_LINK[0][0], 8, 8, 0);
    }
    blit_idx(x1 - 8, y1 - 8, grapple == 2 ? &SPRITE_HOOK_CLOSED[0][0] : &SPRITE_HOOK_OPEN[0][0], 16,
             16, hook_vx < 0);
}

static void draw_lava(void) {
    int sy = sy_of(lava_y, 1);
    int col;
    int frame;
    if (sy >= 240) {
        return;
    }
    frame = (clock / 5) & 3;
    if (sy < 0) {
        gfx_fill_rect(PLAY_X, 0, PLAY_W, 240, PAL[6]);
        return;
    }
    gfx_fill_rect(PLAY_X, sy, PLAY_W, 240 - sy, PAL[6]);
    for (col = 0; col < COLS; col++) {
        blit_idx(PLAY_X + col * TILE, sy, sprite_ptr(LAVA_CREST_FRAMES[frame]), 16, 16, 0);
        if (sy + 16 < 240) {
            blit_idx(PLAY_X + col * TILE, sy + 16, &TILE_LAVA_DEEP[0][0], 16, 16, 0);
        }
    }
}

static int magma_width(const char *s, int scale) {
    int n = 0;
    if (!s || scale < 1) {
        return 0;
    }
    while (s[n]) {
        n++;
    }
    return n * 9 * scale;
}

static void magma_tones(uint16_t ink, uint16_t *top, uint16_t *bot) {
    if (ink == PAL[8]) {
        *top = PAL[8];
        *bot = PAL[7];
    } else if (ink == PAL[7] || ink == PAL[6]) {
        *top = PAL[7];
        *bot = PAL[6];
    } else if (ink == PAL[13]) {
        *top = PAL[13];
        *bot = PAL[14];
    } else if (ink == PAL[4]) {
        *top = PAL[4];
        *bot = PAL[3];
    } else if (ink == PAL[3]) {
        *top = PAL[3];
        *bot = PAL[2];
    } else {
        *top = PAL[5];
        *bot = PAL[4];
    }
}

static void magma_runs(int x, int y, uint8_t bits, int scale, uint16_t color) {
    int col = 0;
    while (col < 8) {
        int start;
        if ((bits & (uint8_t)(0x80u >> col)) == 0) {
            col++;
            continue;
        }
        start = col;
        while (col < 8 && (bits & (uint8_t)(0x80u >> col)) != 0) {
            col++;
        }
        gfx_fill_rect(x + start * scale, y, (col - start) * scale, scale, color);
    }
}

static void magma_text(int x, int y, const char *str, uint16_t ink, int scale) {
    uint16_t top;
    uint16_t bot;
    if (!str || scale < 1) {
        return;
    }
    magma_tones(ink, &top, &bot);
    while (*str) {
        unsigned char ch = (unsigned char)*str++;
        const uint8_t *glyph;
        int row;
        if (ch < 0x20 || ch > 0x7E) {
            ch = '?';
        }
        glyph = FONT_MAGMA_8X8[ch - 0x20];
        for (row = 0; row < 8; row++) {
            uint16_t body = row < 4 ? top : bot;
            magma_runs(x + scale, y + (row + 1) * scale, glyph[row], scale, PAL[10]);
            magma_runs(x, y + row * scale, glyph[row], scale, body);
        }
        x += 9 * scale;
    }
}

static void draw_hud(void) {
    char buf[12];
    int i;
    int meters = py / TILE;
    int prox;
    int gap;
    if (meters < 0) {
        meters = 0;
    }
    gfx_fill_rect(0, 0, PLAY_X, 240, PAL[1]);
    gfx_fill_rect(PLAY_X + PLAY_W, 0, 320 - PLAY_X - PLAY_W, 240, PAL[1]);
    gfx_fill_rect(PLAY_X - 1, 0, 1, 240, PAL[2]);
    gfx_fill_rect(PLAY_X + PLAY_W, 0, 1, 240, PAL[2]);

    magma_text(16, 6, "LIFE", PAL[4], 1);
    for (i = 0; i < 3; i++) {
        const uint8_t *sp = (i < hearts) ? &HUD_HEART_FULL[0][0] : &HUD_HEART_EMPTY[0][0];
        blit_idx(2 + i * 20, 18, sp, 16, 16, 0);
    }
    magma_text(16, 40, "AMMO", PAL[8], 1);
    for (i = 0; i < 4; i++) {
        const uint8_t *sp = (i < ammo) ? &HUD_AMMO_FULL[0][0] : &HUD_AMMO_EMPTY[0][0];
        blit_idx(6 + i * 14, 52, sp, 10, 16, 0);
    }
    magma_text(16, 78, "ALT", PAL[13], 1);
    put_num(buf, (unsigned)meters, 4);
    buf[4] = 'm';
    buf[5] = 0;
    magma_text(8, 90, buf, PAL[5], 1);
    magma_text(8, 104, BIOME_TITLE[biome_of(py)], PAL[4], 1);

    i = 0;
    if (rope) {
        blit_idx(4, 140 + i * 18, &ICON_UPGRADE_LONG_ROPE[0][0], 16, 16, 0);
        i++;
    }
    if (pierce) {
        blit_idx(24, 140, &ICON_UPGRADE_PIERCING[0][0], 16, 16, 0);
    }
    if (boots) {
        blit_idx(4, 160, &ICON_UPGRADE_SPRING_BOOTS[0][0], 16, 16, 0);
    }
    if (magnet) {
        blit_idx(24, 160, &ICON_UPGRADE_SOLAR_MAGNET[0][0], 16, 16, 0);
    }
    (void)i;

    magma_text(268, 6, "SCORE", PAL[4], 1);
    put_num(buf, (unsigned)score, 6);
    magma_text(264, 18, buf, PAL[5], 1);
    magma_text(268, 36, "COMBO", PAL[8], 1);
    buf[0] = 'x';
    buf[1] = (char)('0' + (combo > 0 ? combo : 1));
    buf[2] = 0;
    magma_text(284, 48, buf, combo > 1 ? PAL[7] : PAL[3], 1);

    magma_text(276, 70, "LAVA", PAL[6], 1);
    blit_idx(268, 84, &HUD_MINI_KIP[0][0], 8, 8, 0);
    blit_idx(284, 84, &HUD_LAVA_GAUGE_TOP[0][0], 12, 8, 0);
    for (i = 0; i < 8; i++) {
        blit_idx(284, 92 + i * 8, &HUD_LAVA_GAUGE_MID[0][0], 12, 8, 0);
    }
    blit_idx(284, 156, &HUD_LAVA_GAUGE_BOT[0][0], 12, 8, 0);
    gap = py - lava_y;
    if (gap < 0) {
        gap = 0;
    }
    if (gap > 220) {
        gap = 220;
    }
    prox = 220 - gap;
    blit_idx(298, 84 + (prox * 64) / 220, &HUD_LAVA_CURSOR[0][0], 8, 8, 0);
}

static void draw_box(int x, int y, int w, int h) {
    gfx_fill_rect(x, y, w, h, PAL[2]);
    gfx_rect(x, y, w, h, PAL[4]);
}

static void text_cx(int y, const char *s, uint16_t col, int sc) {
    int w = magma_width(s, sc);
    magma_text((320 - w) / 2, y, s, col, sc);
}

static int owned(int id) {
    if (id == UPGRADE_LONG_ROPE) {
        return rope;
    }
    if (id == UPGRADE_PIERCING_SHOT) {
        return pierce;
    }
    if (id == UPGRADE_SPRING_BOOTS) {
        return boots;
    }
    if (id == UPGRADE_SOLAR_MAGNET) {
        return magnet;
    }
    return 0;
}

static void give_upgrade(int id) {
    if (owned(id)) {
        add_score(500);
    }
    if (id == UPGRADE_LONG_ROPE) {
        rope = 1;
    } else if (id == UPGRADE_PIERCING_SHOT) {
        pierce = 1;
    } else if (id == UPGRADE_SPRING_BOOTS) {
        boots = 1;
    } else if (id == UPGRADE_SOLAR_MAGNET) {
        magnet = 1;
    }
    sfx_upgrade();
}

static void roll_offers(void) {
    int used = 0;
    int i;
    for (i = 0; i < 3; i++) {
        int id = 1;
        int guard = 0;
        do {
            id = 1 + (int)(rnd() % 4u);
            guard++;
        } while ((used & (1 << id)) && guard < 16);
        used |= 1 << id;
        offer[i] = id;
    }
    offer_sel = 0;
}

static void draw_offers(void) {
    int i;
    int id = offer[offer_sel];
    const UpgradeMetadata *meta = 0;
    draw_box(70, 28, 180, 150);
    text_cx(36, "REST", PAL[8], 1);
    for (i = 0; i < 3; i++) {
        int oid = offer[i];
        int x = 86 + i * 52;
        const uint8_t *sp = &UPGRADE_TABLE[oid - 1].sprite[0][0];
        blit_idx(x, 54, &TILE_REST_PEDESTAL[0][0], 16, 16, 0);
        blit_idx(x, 40, sp, 16, 16, 0);
        if (i == offer_sel) {
            gfx_rect(x - 3, 36, 22, 36, PAL[8]);
        }
    }
    for (i = 0; i < 4; i++) {
        if (UPGRADE_TABLE[i].id == (UpgradeType)id) {
            meta = &UPGRADE_TABLE[i];
        }
    }
    if (meta) {
        text_cx(96, meta->name, PAL[5], 1);
        text_cx(110, meta->desc, PAL[13], 1);
        if (owned(id)) {
            text_cx(124, "OWNED: +500", PAL[7], 1);
        }
    }
        text_cx(148, "A PICKS", PAL[4], 1);
}

static void draw_press_start(void) {
    if ((attract_t & 16) == 0) {
        text_cx(222, "PRESS START", PAL[7], 1);
    }
}

static void draw_slide_dots(int page) {
    int i;
    int x0 = 160 - SLIDE_HOW * 5;
    for (i = 0; i < SLIDE_HOW; i++) {
        gfx_fill_rect(x0 + i * 10, 206, 4, 4, i == page ? PAL[8] : PAL[4]);
    }
}

static void draw_how(int page) {
    int i;
    int fr = (attract_t / 6) % 3;
    text_cx(8, "SKYWARD CLIMB", PAL[8], 2);
    if (page == 0) {
        spr(144, 48, &SPRITE_KIP_IDLE[0][0], 2);
        for (i = 0; i < 3; i++) {
            spr(112 + i * 36, 92, &HUD_HEART_FULL[0][0], 2);
        }
        for (i = 0; i < 8; i++) {
            spr(96 + i * 16, 132, &TILE_LAVA_CREST_0[0][0], 1);
        }
        text_cx(156, "CLIMB AHEAD OF THE LAVA", PAL[5], 1);
        text_cx(172, "THREE HEARTS, THEN YOU FALL", PAL[13], 1);
    } else if (page == 1) {
        spr(80, 52, &SPRITE_KIP_JUMP[0][0], 2);
        spr(144, 52, &SPRITE_KIP_GRAPPLE[0][0], 2);
        spr(208, 52, &SPRITE_KIP_FALL[0][0], 2);
        text_cx(108, "A JUMPS", PAL[5], 1);
        text_cx(124, "B ON GROUND: GRAPPLE", PAL[5], 1);
        text_cx(140, "B IN AIR: SHOOT DOWN", PAL[5], 1);
        text_cx(156, "UP+B GRAPPLES IN AIR", PAL[13], 1);
    } else if (page == 2) {
        spr(72, 88, sprite_ptr(BIOME_PLATFORMS[0]), 2);
        spr(104, 88, sprite_ptr(BIOME_PLATFORMS[0]), 2);
        spr(80, 56, sprite_ptr(GULL_ANIM_FRAMES[fr]), 2);
        spr(176, 88, sprite_ptr(BIOME_PLATFORMS[0]), 2);
        spr(208, 88, sprite_ptr(BIOME_PLATFORMS[0]), 2);
        spr(184, 56, sprite_ptr(TURTLE_ANIM_FRAMES[2]), 2);
        text_cx(140, "LAND ON THE THIN LEDGES", PAL[5], 1);
        text_cx(156, "STOMP THE GULL", PAL[5], 1);
        text_cx(172, "FLIP THE TURTLE FROM THE SIDE", PAL[13], 1);
    } else {
        spr(144, 44, &SPRITE_SOLAR_CRYSTAL[0][0], 2);
        for (i = 0; i < 4; i++) {
            spr(96 + i * 36, 88, &UPGRADE_TABLE[i].sprite[0][0], 2);
        }
        text_cx(140, "CRYSTALS SCORE POINTS", PAL[5], 1);
        text_cx(156, "EACH BIOME, PICK ONE", PAL[5], 1);
        text_cx(172, "UPGRADE", PAL[13], 1);
    }
    draw_slide_dots(page);
    draw_press_start();
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
    magma_text(x, y, pos, color, 1);
    magma_text(x + 28, y, name, color, 1);
    magma_text(x + 140 - magma_width(num, 1), y, num, color, 1);
}

static void draw_scores(void) {
    int i;
    text_cx(6, "TOP 20", PAL[8], 2);
    if (score_n == 0) {
        text_cx(108, "NO SCORES YET", PAL[5], 1);
    } else {
        for (i = 0; i < score_n; i++) {
            uint16_t color = (i == rank_hi) ? PAL[8] : PAL[5];
            int col = i / 10;
            int row = i % 10;
            draw_score_row(12 + col * 156, 40 + row * 16, i, &scores[i], color);
        }
    }
    draw_press_start();
}

static void draw_vol_hint(int y) {
    char line[20];
    int i;
    int n = 0;
    const char *p = "Y+  B-  VOL ";
    while (*p && n < 14) {
        line[n++] = *p++;
    }
    for (i = 0; i < MUS_LVL_MAX; i++) {
        line[n++] = (char)(i < mus_lvl ? '#' : '-');
    }
    line[n] = 0;
    text_cx(y, line, mus_lvl ? PAL[8] : PAL[3], 1);
}

static void draw_attract(void) {
    if (attract_page >= SLIDE_HOW) {
        draw_scores();
        draw_vol_hint(204);
    } else {
        draw_how(attract_page);
        draw_vol_hint(188);
    }
}

static void draw_end(void) {
    char buf[12];
    draw_box(78, 48, 164, 120);
    if (won_flag || state == ST_WIN) {
        text_cx(60, "SUMMIT", PAL[8], 2);
    } else if (death_lava) {
        text_cx(64, "THE LAVA", PAL[7], 1);
    } else {
        text_cx(64, "KIP FELL", PAL[6], 1);
    }
    text_cx(92, "SCORE", PAL[4], 1);
    put_num(buf, (unsigned)score, 6);
    text_cx(106, buf, PAL[5], 2);
    if ((clock / 16) & 1) {
        text_cx(140, "START", PAL[5], 1);
    }
}

static void draw_name(void) {
    int i;
    char one[2];
    char num[8];
    format_u(num, (unsigned)score);
    text_cx(28, "NEW HIGH SCORE", PAL[8], 2);
    text_cx(64, num, PAL[5], 2);
    text_cx(96, "YOUR NAME", PAL[4], 1);
    one[1] = 0;
    for (i = 0; i < 3; i++) {
        one[0] = letters[i];
        magma_text(132 + i * 20, 116, one, PAL[5], 2);
        if (i == name_pos && (clock & 8) == 0) {
            gfx_fill_rect(132 + i * 20, 136, 16, 2, PAL[8]);
        }
    }
    text_cx(160, "UP DOWN", PAL[4], 1);
    text_cx(176, "LEFT RIGHT", PAL[4], 1);
    text_cx(192, "A ENTERS", PAL[4], 1);
}

static void draw_pause(void) {
    draw_box(100, 90, 120, 40);
    text_cx(104, "PAUSED", PAL[5], 1);
}

static void enter_room(void) {
    int row = py >> 4;
    if (peak_base >= 0 && row >= peak_base && row < peak_base + CH && py >= peak_base * TILE + 16) {
        state = ST_PEAK;
        grapple = 0;
        vx = 0;
        vy = 0;
        combo = 0;
        return;
    }
    if (rest_base >= 0 && rest_base != rest_done && row >= rest_base && row < rest_base + CH &&
        py >= rest_base * TILE + 16) {
        roll_offers();
        state = ST_REST;
        grapple = 0;
        vx = 0;
        vy = 0;
        combo = 0;
    }
}

static void rest_input(const lippus_pad_t *pad) {
    if (pad_pressed(pad, LIPPUS_LEFT)) {
        offer_sel = (offer_sel + 2) % 3;
    }
    if (pad_pressed(pad, LIPPUS_RIGHT)) {
        offer_sel = (offer_sel + 1) % 3;
    }
    if (pad_pressed(pad, LIPPUS_A) || pad_pressed(pad, LIPPUS_B)) {
        give_upgrade(offer[offer_sel]);
        open_ceiling(rest_base);
        rest_done = rest_base;
        state = ST_PLAY;
        ammo = 4;
    }
}

static void name_input(const lippus_pad_t *pad) {
    int move = 0;
    if (pad_pressed(pad, LIPPUS_UP)) {
        move = 1;
    } else if (pad_pressed(pad, LIPPUS_DOWN)) {
        move = -1;
    }
    if (move == 0 && (pad_held(pad, LIPPUS_UP) || pad_held(pad, LIPPUS_DOWN))) {
        if (++letter_hold > 10 && (letter_hold & 3) == 0) {
            move = pad_held(pad, LIPPUS_UP) ? 1 : -1;
        }
    } else {
        letter_hold = 0;
    }
    if (move != 0) {
        char ch = (char)(letters[name_pos] + move);
        if (ch < 'A') {
            ch = 'Z';
        }
        if (ch > 'Z') {
            ch = 'A';
        }
        letters[name_pos] = ch;
    }
    if (pad_pressed(pad, LIPPUS_LEFT) && name_pos > 0) {
        name_pos--;
    }
    if (pad_pressed(pad, LIPPUS_RIGHT) && name_pos < 2) {
        name_pos++;
    }
    if (pad_pressed(pad, LIPPUS_A) || pad_pressed(pad, LIPPUS_START)) {
        score_insert(score);
        scores_save();
        enter_attract(SLIDE_HOW);
    }
}

static void draw_frame(void) {
    int play = state == ST_PLAY || state == ST_PAUSE || state == ST_DIE || state == ST_REST || state == ST_PEAK;
    draw_dy = (shake & 1) ? 1 : 0;
    gfx_clip(0, 0, 0, 0);
    gfx_clear(PAL[1]);
    if (play || state == ST_OVER || state == ST_WIN) {
        gfx_clip(PLAY_X, 0, PLAY_W, 240);
        gfx_fill_rect(PLAY_X, 0, PLAY_W, 240, PAL[1]);
        draw_tiles();
        draw_ents();
        draw_player();
        draw_lava();
        draw_hook();
        gfx_clip(0, 0, 0, 0);
        draw_hud();
    }
    if (state == ST_TITLE) {
        draw_attract();
    } else if (state == ST_PAUSE) {
        draw_pause();
    } else if (state == ST_REST) {
        draw_offers();
    } else if (state == ST_PEAK) {
        draw_box(70, 70, 180, 70);
        text_cx(82, "ASTRAL PEAK", PAL[8], 1);
        text_cx(100, "A TO FINISH", PAL[5], 1);
    } else if (state == ST_OVER || state == ST_WIN) {
        draw_end();
    } else if (state == ST_NAME) {
        draw_name();
    }
    if (combo > 1 && state == ST_PLAY) {
        char b[3];
        b[0] = 'x';
        b[1] = (char)('0' + combo);
        b[2] = 0;
        magma_text(PLAY_X + px + 14, sy_of(py, 16) - 8, b, PAL[8], 1);
    }
}

void lippus_main(void) {
    rank_hi = -1;
    scores_load();
    enter_attract(0);
    for (;;) {
        lippus_pad_t pad = pad_poll();
        int prev_py = py;
        if (state == ST_TITLE) {
            clock++;
            if (pad_pressed(&pad, LIPPUS_Y)) {
                mus_nudge(1);
            }
            if (pad_pressed(&pad, LIPPUS_B)) {
                mus_nudge(-1);
            }
            if (pad_pressed(&pad, LIPPUS_START) || pad_pressed(&pad, LIPPUS_A)) {
                rank_hi = -1;
                new_game();
                state = ST_PLAY;
            } else {
                int hold = attract_page >= SLIDE_HOW ? SCORE_HOLD : SLIDE_LEN;
                if (++attract_t >= hold) {
                    attract_t = 0;
                    attract_page++;
                    if (attract_page >= SLIDE_N) {
                        attract_page = 0;
                    }
                }
            }
        } else if (state == ST_PAUSE) {
            if (pad_pressed(&pad, LIPPUS_START)) {
                state = ST_PLAY;
            }
        } else if (state == ST_REST) {
            rest_input(&pad);
        } else if (state == ST_PEAK) {
            if (pad_pressed(&pad, LIPPUS_A) || pad_pressed(&pad, LIPPUS_START)) {
                finish_run(1);
            }
        } else if (state == ST_DIE) {
            die_t--;
            if (die_t <= 0) {
                finish_run(0);
            }
        } else if (state == ST_NAME) {
            name_input(&pad);
            clock++;
        } else if (state == ST_OVER || state == ST_WIN) {
            clock++;
            if (end_t > 0) {
                end_t--;
            }
            if (end_t <= 0 || pad_pressed(&pad, LIPPUS_START) || pad_pressed(&pad, LIPPUS_A)) {
                if (score_qualifies(score)) {
                    enter_name();
                } else {
                    rank_hi = -1;
                    enter_attract(SLIDE_HOW);
                }
            }
        } else if (state == ST_PLAY) {
            if (pad_pressed(&pad, LIPPUS_START)) {
                state = ST_PAUSE;
            } else {
                clock++;
                player_update(&pad);
                if (state == ST_PLAY) {
                    turrets_fire();
                    ents_update(prev_py);
                    hazards();
                    crumbs_tick();
                    lava_tick();
                    cam_update();
                    ensure_chunks();
                    if (state == ST_PLAY) {
                        enter_room();
                    }
                }
            }
        }
        if (shake > 0 && state == ST_PLAY) {
            shake--;
        }
        amb_tick();
        seq_tick();
        sfx_tick(FRAME_MS);
        draw_frame();
        gfx_present();
        sys_frame(FRAME_MS);
    }
}
