#include "lippus.h"
#include "lippus/sfx.h"

#include "assets.h"
#include "rooms.h"

/* Incursore contro l'Egemonia. Stanze 20x14, una schermata ciascuna.
   A spara, B blocca la mira, X apre o sfiata, Y granata, click lama.
   Select breve cambia arma, Select lungo l'inventario, Start il menu.
   Otto slot in record/INCSAV0..7. Abort e la morte non li cancellano.
   Il sintetizzatore a campioni dei file audio non gira qui: il firmware
   ha quattro voci, e ogni effetto usa quelle. La musica e' un snd_tune
   in loop sulla voce 1. */

#define FRAME 33
#define MAP 8
#define COLS 20
#define ROWS 14
#define REC_NAME "INCURSOR"

#define D_NONE 0
#define D_CLOSED 1
#define D_LOCKED 2
#define D_OPEN 3

#define K_COMBAT 0
#define K_START 1
#define K_KEY 2
#define K_BOSS 3
#define K_LOOT 4

#define W_PISTOL 0
#define W_SCATTER 1
#define W_RAIL 2
#define W_MINI 3

#define EK_GRUNT 0
#define EK_DROID 1
#define EK_ELITE 2
#define EK_BRUTE 3
#define EK_BOSS 4

#define SK_PULSE 0
#define SK_OC 1
#define SK_SCAT 2
#define SK_LASER 3
#define SK_ORB 4
#define SK_NADE 5

#define PK_MED 0
#define PK_SH 1
#define PK_FUS 2
#define PK_YEL 3
#define PK_RED 4
#define PK_KEY 5
#define PK_DATA 6
#define PK_WSC 7
#define PK_WRA 8
#define PK_WMI 9

#define SFX_PULSE 1
#define SFX_OC 2
#define SFX_RAIL 3
#define SFX_BLADE 4
#define SFX_PARRY 5
#define SFX_VENT 6
#define SFX_ALARM 7
#define SFX_THROW 8
#define SFX_BOOM 9
#define SFX_SLOW_IN 10
#define SFX_SLOW_OUT 11
#define SFX_DOOR 12
#define SFX_LOCK 13
#define SFX_PICK 14
#define SFX_DROID 15
#define SFX_HIT 16
#define SFX_SCATTER 17
#define SFX_MINI 18
#define SFX_MINE 19
#define SFX_DEATH 20
#define SFX_STEP 21
#define SFX_DRY 22

#define TITLE_PAGES 5

#define ST_TITLE 0
#define ST_UP 1
#define ST_PLAY 2
#define ST_PAUSE 3
#define ST_INV 4
#define ST_DEAD 5
#define ST_DECK 6
#define ST_WIN 7
#define ST_SLOT 8

#define SAVE_SLOTS 8

#define MAX_EN 8
#define MAX_SHOT 22
#define MAX_PICK 6
#define MAX_MINE 4
#define MAX_BAR 4
#define MAX_VENT 8
#define MAX_CRUSH 2
#define MAX_FX 6
#define MAX_BOOM 8

typedef struct {
    uint8_t on, kind, tmpl, dist;
    uint8_t door[4];
    uint8_t visited, cleared, looted;
} Room;

typedef struct {
    int x, y, hp, maxhp, kind, on;
    int cd, burst, shield, kt, kvx, kvy, strafe;
} Enemy;

typedef struct {
    int x, y, vx, vy, life, dmg, team, kind, slow, on;
} Shot;

typedef struct {
    int x, y, kind, val, on;
} Pick;

typedef struct {
    int x, y, fuse, on;
} Mine;

typedef struct {
    int x, y, hp, on;
} Barrel;

typedef struct {
    int tx, ty, dir, on;
} Vent;

typedef struct {
    int x, y, dir, minx, maxx, on;
} Crush;

typedef struct {
    int x, y, t, on;
} Fx;

typedef struct __attribute__((packed)) {
    char magic[4];
    uint16_t data;
    uint8_t hp_rank, heat_rank, dot_rank, unlocks;
} Meta;

/* Formato precedente, un solo slot dentro INCURSOR. Si legge solo per migrarlo. */
typedef struct __attribute__((packed)) {
    char magic[4];
    uint16_t data;
    uint8_t hp_rank, heat_rank, dot_rank, unlocks;
    uint8_t has_save;
    uint8_t deck, rx, ry, face, weapon;
    uint8_t hp, sp, grenades, has_key, over;
    uint8_t ammo_s, ammo_y, heat, key_used, held;
    uint16_t px, py;
    uint32_t seed;
    uint8_t doors[64];
    uint8_t visited[8];
    uint8_t cleared[8];
    uint8_t looted[8];
} OldRec;

typedef struct __attribute__((packed)) {
    char magic[4];
    uint8_t deck, rx, ry, face, weapon;
    uint8_t hp, sp, grenades, has_key, over;
    uint8_t ammo_s, ammo_y, heat, key_used, held;
    uint16_t px, py;
    uint32_t seed;
    uint8_t doors[64];
    uint8_t visited[8];
    uint8_t cleared[8];
    uint8_t looted[8];
} Slot;

typedef char old_rec_size_ok[(sizeof(OldRec) == 122) ? 1 : -1];

static const int NX[4] = {0, 1, 0, -1};
static const int NY[4] = {-1, 0, 1, 0};
static const int OPP[4] = {2, 3, 0, 1};
static const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const char *const DECK_NAME[3] = {"HANGAR", "REACTOR", "BRIDGE"};
static const char *const BOSS_NAME[3] = {"TWIN TURRET", "CORE ALPHA", "HEGEMONY CORE"};
static const char *const PAUSE_ITEM[4] = {"RESUME", "SAVE", "LOAD", "ABORT"};
static const char *const WEP_NAME[4] = {"PISTOL", "SCATTER", "RAIL", "MINIGUN"};

static uint16_t g_tile_rgb[TID_COUNT][256];
static uint16_t g_scratch[32 * 32];
static uint8_t g_tiles[ROWS][COLS];
static Room g_map[64];
static Enemy g_enemy[MAX_EN];
static Shot g_shot[MAX_SHOT];
static Pick g_pick[MAX_PICK];
static Mine g_mine[MAX_MINE];
static Barrel g_bar[MAX_BAR];
static Vent g_vent[MAX_VENT];
static Crush g_crush[MAX_CRUSH];
static Fx g_fx[MAX_FX];
static int g_bx[MAX_BOOM], g_by[MAX_BOOM], g_bd[MAX_BOOM], g_bn;
static int g_nvent, g_ncrush, g_nbar, g_nmine;

static Meta g_meta;
static Slot g_slot[SAVE_SLOTS];
static uint8_t g_slot_ok[SAVE_SLOTS];
static uint32_t g_rng = 1;
static uint32_t g_seed = 1;
static uint32_t g_salt = 1;
static int g_parent[64];
static int g_bfs_dist[64];
static uint8_t g_path[64];
static uint8_t g_q[64];

static int g_state, g_frame, g_tick, g_wait, g_menu, g_page;
static int g_deck, g_rx, g_ry, g_px, g_py, g_face;
static int g_hp, g_sp, g_heat, g_lock, g_over, g_weapon, g_held;
static int g_ammo_s, g_ammo_y, g_grenades, g_has_key, g_key_used;
static int g_data, g_hp_rank, g_heat_rank, g_dot_rank, g_unlocks;
static int g_cd, g_melee_cd, g_iframes, g_combat, g_moving;
static int g_slow, g_slash, g_rail_t, g_rail_x, g_rail_y, g_grid_off;
static int g_shake, g_dot, g_boss_down, g_spiral, g_firing;
static int g_slot_mode, g_slot_back, g_confirm;
static int g_start_x, g_start_y;
static uint16_t g_edge;
static int g_sel_track, g_sel_hold;
static char g_banner[28];
static int g_banner_t;
static int g_sox, g_soy;

static int ri(int x, int y) { return y * MAP + x; }

static int inside(int x, int y) {
    return (unsigned)x < MAP && (unsigned)y < MAP;
}

static int iabs(int v) { return v < 0 ? -v : v; }

static unsigned rnd(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 16;
}

static int bit_get(const uint8_t *b, int i) { return (b[i >> 3] >> (i & 7)) & 1; }

static void bit_set(uint8_t *b, int i, int v) {
    uint8_t m = (uint8_t)(1u << (i & 7));
    if (v) {
        b[i >> 3] |= m;
    } else {
        b[i >> 3] &= (uint8_t)~m;
    }
}

static uint16_t C(int idx) { return SCI_FI_PALETTE_32[idx]; }

static int hp_max(void) { return 100 + g_hp_rank * 10; }

static void banner(const char *s, int t) {
    int i = 0;
    while (s[i] && i < 27) {
        g_banner[i] = s[i];
        i++;
    }
    g_banner[i] = 0;
    g_banner_t = t;
}

static void mem_zero(void *p, int n) {
    int i;
    uint8_t *b = (uint8_t *)p;
    for (i = 0; i < n; i++) {
        b[i] = 0;
    }
}

static void slot_name(int i, char *d) {
    d[0] = 'I';
    d[1] = 'N';
    d[2] = 'C';
    d[3] = 'S';
    d[4] = 'A';
    d[5] = 'V';
    d[6] = (char)('0' + i);
    d[7] = 0;
}

static void store_meta(void) {
    g_meta.magic[0] = 'I';
    g_meta.magic[1] = 'N';
    g_meta.magic[2] = 'C';
    g_meta.magic[3] = '2';
    g_meta.data = (uint16_t)g_data;
    g_meta.hp_rank = (uint8_t)g_hp_rank;
    g_meta.heat_rank = (uint8_t)g_heat_rank;
    g_meta.dot_rank = (uint8_t)g_dot_rank;
    g_meta.unlocks = (uint8_t)g_unlocks;
}

static void flush_disk(void) {
    store_meta();
    sys_record_write(REC_NAME, &g_meta, (int)sizeof(g_meta));
}

static int any_save(void) {
    int i;
    for (i = 0; i < SAVE_SLOTS; i++) {
        if (g_slot_ok[i]) {
            return 1;
        }
    }
    return 0;
}

static void read_slots(void) {
    int i;
    for (i = 0; i < SAVE_SLOTS; i++) {
        char name[8];
        Slot s;
        int n;
        slot_name(i, name);
        mem_zero(&s, (int)sizeof(s));
        n = sys_record_read(name, &s, (int)sizeof(s));
        if (n == (int)sizeof(s) && s.magic[0] == 'S' && s.magic[1] == 'A' && s.magic[2] == 'V' &&
            s.magic[3] == '1') {
            g_slot[i] = s;
            g_slot_ok[i] = 1;
        } else {
            mem_zero(&g_slot[i], (int)sizeof(g_slot[i]));
            g_slot_ok[i] = 0;
        }
    }
}

static int write_slot(int i) {
    char name[8];
    slot_name(i, name);
    g_slot[i].magic[0] = 'S';
    g_slot[i].magic[1] = 'A';
    g_slot[i].magic[2] = 'V';
    g_slot[i].magic[3] = '1';
    if (sys_record_write(name, &g_slot[i], (int)sizeof(g_slot[i])) != 0) {
        return 0;
    }
    g_slot_ok[i] = 1;
    return 1;
}

static void take_meta(uint16_t data, uint8_t hp, uint8_t heat, uint8_t dot, uint8_t unlocks) {
    g_data = data;
    g_hp_rank = hp;
    g_heat_rank = heat;
    g_dot_rank = dot;
    g_unlocks = unlocks | 1;
    if (g_hp_rank > 3) {
        g_hp_rank = 3;
    }
    if (g_heat_rank > 2) {
        g_heat_rank = 2;
    }
    if (g_dot_rank > 2) {
        g_dot_rank = 2;
    }
}

static void grant_data(int n) {
    g_data += n;
    if (g_data > 999) {
        g_data = 999;
    }
    flush_disk();
}

static void fill_slot_from_old(int i, const OldRec *r) {
    int k;
    Slot *s = &g_slot[i];
    mem_zero(s, (int)sizeof(*s));
    s->deck = r->deck;
    s->rx = r->rx;
    s->ry = r->ry;
    s->face = r->face;
    s->weapon = r->weapon;
    s->hp = r->hp;
    s->sp = r->sp;
    s->grenades = r->grenades;
    s->has_key = r->has_key;
    s->over = r->over;
    s->ammo_s = r->ammo_s;
    s->ammo_y = r->ammo_y;
    s->heat = r->heat;
    s->key_used = r->key_used;
    s->held = r->held;
    s->px = r->px;
    s->py = r->py;
    s->seed = r->seed;
    for (k = 0; k < 64; k++) {
        s->doors[k] = r->doors[k];
    }
    for (k = 0; k < 8; k++) {
        s->visited[k] = r->visited[k];
        s->cleared[k] = r->cleared[k];
        s->looted[k] = r->looted[k];
    }
}

static void load_disk(void) {
    uint8_t raw[sizeof(OldRec)];
    int n;
    mem_zero(raw, (int)sizeof(raw));
    mem_zero(&g_meta, (int)sizeof(g_meta));
    n = sys_record_read(REC_NAME, raw, (int)sizeof(raw));
    if (n >= (int)sizeof(Meta) && raw[0] == 'I' && raw[1] == 'N' && raw[2] == 'C' && raw[3] == '2') {
        Meta m;
        mem_zero(&m, (int)sizeof(m));
        {
            int i;
            for (i = 0; i < (int)sizeof(m); i++) {
                ((uint8_t *)&m)[i] = raw[i];
            }
        }
        take_meta(m.data, m.hp_rank, m.heat_rank, m.dot_rank, m.unlocks);
        read_slots();
        return;
    }
    if (n == (int)sizeof(OldRec) && raw[0] == 'I' && raw[1] == 'N' && raw[2] == 'C' && raw[3] == '1') {
        OldRec old;
        int i;
        mem_zero(&old, (int)sizeof(old));
        for (i = 0; i < (int)sizeof(old); i++) {
            ((uint8_t *)&old)[i] = raw[i];
        }
        take_meta(old.data, old.hp_rank, old.heat_rank, old.dot_rank, old.unlocks);
        read_slots();
        if (old.has_save && !g_slot_ok[0]) {
            fill_slot_from_old(0, &old);
            if (write_slot(0)) {
                flush_disk();
            }
        } else {
            flush_disk();
        }
        return;
    }
    take_meta(0, 0, 0, 0, 1);
    read_slots();
}

/* Musica di sottofondo su una voce. Lo spartito a tre canali (basso
   triangolo, arpeggio quadra, lead a dente di sega) non sta nel mixer:
   niente dente di sega, e snd_tune tiene una voce sola. La lead, se non
   e' una pausa, copre l'arpeggio; il basso resta fuori. Gli indici del
   file sono un'ottava sotto la tabella in Hz (N_A3 = 440, N_E4 = 659,
   N_A4 = 880, N_B2+12 = 494, N_C4+1 = 554).
   Infiltrazione 122 BPM (123 ms a sedicesimo), dal titolo e per la run.
   Boss 138 BPM (109 ms) solo nella stanza BOSS del ponte, deck 2.
   snd_tune non fa loop e un snd_voice sulla stessa voce lo spegne:
   a fine frase si rilancia, e se lama o pickup prendono la voce 1 il
   tema riparte dal sedicesimo in corso quando l'effetto finisce.
   Gli spari stanno sulla voce 0 e non la rubano. */
#define BGM_INFILTRATION 1
#define BGM_BOSS_BATTLE 2
#define BGM_VOICE 1
#define BGM_LEN 32
#define BGM_INFIL_MS 123
#define BGM_BOSS_MS 109
/* Meta' del livello precedente, cosi' la quadra non copre gli spari.
   Sul titolo Y e B spostano g_bgm_lvl: 2 e' questo volume, 4 torna al
   precedente, 0 zittisce. Il boss resta un filo piu' alto. */
#define BGM_VOL_IN 18
#define BGM_VOL_BOSS 22
#define BGM_LVL_MAX 4
#define BGM_LVL_DEF 2

#define BGM_N(hz, ms) {(uint16_t)(hz), (uint16_t)(ms)}

static const lippus_note_t g_bgm_infil[BGM_LEN] = {
    BGM_N(659, BGM_INFIL_MS), BGM_N(523, BGM_INFIL_MS), BGM_N(659, BGM_INFIL_MS), BGM_N(587, BGM_INFIL_MS),
    BGM_N(523, BGM_INFIL_MS), BGM_N(523, BGM_INFIL_MS), BGM_N(440, BGM_INFIL_MS), BGM_N(523, BGM_INFIL_MS),
    BGM_N(523, BGM_INFIL_MS), BGM_N(440, BGM_INFIL_MS), BGM_N(587, BGM_INFIL_MS), BGM_N(440, BGM_INFIL_MS),
    BGM_N(494, BGM_INFIL_MS), BGM_N(494, BGM_INFIL_MS), BGM_N(587, BGM_INFIL_MS), BGM_N(494, BGM_INFIL_MS),
    BGM_N(659, BGM_INFIL_MS), BGM_N(523, BGM_INFIL_MS), BGM_N(784, BGM_INFIL_MS), BGM_N(523, BGM_INFIL_MS),
    BGM_N(880, BGM_INFIL_MS), BGM_N(659, BGM_INFIL_MS), BGM_N(784, BGM_INFIL_MS), BGM_N(659, BGM_INFIL_MS),
    BGM_N(698, BGM_INFIL_MS), BGM_N(440, BGM_INFIL_MS), BGM_N(659, BGM_INFIL_MS), BGM_N(587, BGM_INFIL_MS),
    BGM_N(659, BGM_INFIL_MS), BGM_N(392, BGM_INFIL_MS), BGM_N(494, BGM_INFIL_MS), BGM_N(392, BGM_INFIL_MS),
};

static const lippus_note_t g_bgm_boss[BGM_LEN] = {
    BGM_N(880, BGM_BOSS_MS), BGM_N(349, BGM_BOSS_MS), BGM_N(880, BGM_BOSS_MS), BGM_N(784, BGM_BOSS_MS),
    BGM_N(698, BGM_BOSS_MS), BGM_N(349, BGM_BOSS_MS), BGM_N(587, BGM_BOSS_MS), BGM_N(349, BGM_BOSS_MS),
    BGM_N(698, BGM_BOSS_MS), BGM_N(587, BGM_BOSS_MS), BGM_N(784, BGM_BOSS_MS), BGM_N(698, BGM_BOSS_MS),
    BGM_N(659, BGM_BOSS_MS), BGM_N(554, BGM_BOSS_MS), BGM_N(659, BGM_BOSS_MS), BGM_N(554, BGM_BOSS_MS),
    BGM_N(587, BGM_BOSS_MS), BGM_N(349, BGM_BOSS_MS), BGM_N(698, BGM_BOSS_MS), BGM_N(587, BGM_BOSS_MS),
    BGM_N(880, BGM_BOSS_MS), BGM_N(440, BGM_BOSS_MS), BGM_N(784, BGM_BOSS_MS), BGM_N(698, BGM_BOSS_MS),
    BGM_N(784, BGM_BOSS_MS), BGM_N(466, BGM_BOSS_MS), BGM_N(698, BGM_BOSS_MS), BGM_N(659, BGM_BOSS_MS),
    BGM_N(587, BGM_BOSS_MS), BGM_N(659, BGM_BOSS_MS), BGM_N(784, BGM_BOSS_MS), BGM_N(659, BGM_BOSS_MS),
};

static int g_bgm_theme;
static int g_bgm_vol;
static int g_bgm_lvl = BGM_LVL_DEF;
static int g_bgm_total;
static int g_bgm_pos;
static int g_bgm_hold;
static int g_bgm_need_arm;
static int g_bgm_running;
static const lippus_note_t *g_bgm_notes;
static int g_bgm_count;

static int bgm_level_vol(int base) {
    return base * g_bgm_lvl / BGM_LVL_DEF;
}

static void bgm_set_theme(int theme) {
    const lippus_note_t *notes;
    int count;
    int vol;
    int total;
    int i;
    if (theme != BGM_BOSS_BATTLE) {
        theme = BGM_INFILTRATION;
    }
    if (theme == g_bgm_theme && g_bgm_notes) {
        return;
    }
    if (theme == BGM_BOSS_BATTLE) {
        notes = g_bgm_boss;
        count = BGM_LEN;
        vol = bgm_level_vol(BGM_VOL_BOSS);
    } else {
        notes = g_bgm_infil;
        count = BGM_LEN;
        vol = bgm_level_vol(BGM_VOL_IN);
    }
    total = 0;
    for (i = 0; i < count; i++) {
        total += (int)notes[i].ms;
    }
    g_bgm_theme = theme;
    g_bgm_notes = notes;
    g_bgm_count = count;
    g_bgm_vol = vol;
    g_bgm_total = total > 0 ? total : 1;
    g_bgm_pos = 0;
    g_bgm_running = 0;
    g_bgm_need_arm = 1;
}

static void bgm_arm(int pos) {
    int i;
    int acc;
    int step;
    if (!g_bgm_notes || g_bgm_count <= 0) {
        return;
    }
    if (pos < 0 || pos >= g_bgm_total) {
        pos = 0;
    }
    acc = 0;
    step = 0;
    for (i = 0; i < g_bgm_count; i++) {
        int dur = (int)g_bgm_notes[i].ms;
        if (pos < acc + dur) {
            step = i;
            /* Il firmware riparte dall'attacco del sedicesimo. */
            g_bgm_pos = acc;
            break;
        }
        acc += dur;
    }
    snd_tune(BGM_VOICE, SND_SQUARE, (unsigned)g_bgm_vol, g_bgm_notes + step,
             (unsigned)(g_bgm_count - step));
}

static void bgm_nudge(int dir) {
    int next = g_bgm_lvl + dir;
    int base;
    if (next < 0) {
        next = 0;
    }
    if (next > BGM_LVL_MAX) {
        next = BGM_LVL_MAX;
    }
    if (next == g_bgm_lvl) {
        return;
    }
    g_bgm_lvl = next;
    base = g_bgm_theme == BGM_BOSS_BATTLE ? BGM_VOL_BOSS : BGM_VOL_IN;
    g_bgm_vol = bgm_level_vol(base);
    g_bgm_need_arm = 1;
}

static void bgm_hold(int ms) {
    if (ms > g_bgm_hold) {
        g_bgm_hold = ms;
    }
}

static void bgm_follow(void) {
    int theme;
    if (g_state == ST_PAUSE || g_state == ST_INV || g_state == ST_SLOT) {
        return;
    }
    theme = BGM_INFILTRATION;
    if (g_state == ST_PLAY && g_deck == 2 && (unsigned)g_rx < MAP && (unsigned)g_ry < MAP &&
        g_map[ri(g_rx, g_ry)].kind == K_BOSS) {
        theme = BGM_BOSS_BATTLE;
    }
    bgm_set_theme(theme);
}

static void bgm_tick(unsigned frame_ms) {
    if (frame_ms == 0) {
        frame_ms = 1;
    }
    bgm_follow();
    if (!g_bgm_notes || g_bgm_total <= 0) {
        return;
    }
    if (g_bgm_running) {
        g_bgm_pos += (int)frame_ms;
        if (g_bgm_pos >= g_bgm_total) {
            g_bgm_pos = 0;
            g_bgm_need_arm = 1;
        }
    }
    if (g_bgm_hold > 0) {
        g_bgm_hold -= (int)frame_ms;
        if (g_bgm_hold > 0) {
            return;
        }
        g_bgm_hold = 0;
        g_bgm_need_arm = 1;
    }
    if (!g_bgm_need_arm) {
        return;
    }
    g_bgm_need_arm = 0;
    g_bgm_running = 1;
    bgm_arm(g_bgm_pos);
}

static void sfx(int id) {
    switch (id) {
    case SFX_PULSE:
        sfx_sweep(0, SND_SQUARE, 1180, 240, 150, 90);
        break;
    case SFX_OC:
        sfx_sweep(0, SND_SQUARE, 1520, 180, 180, 160);
        sfx_sweep(1, SND_SQUARE, 760, 90, 140, 180);
        bgm_hold(180);
        break;
    case SFX_RAIL:
        sfx_sweep(0, SND_SQUARE, 1800, 70, 200, 220);
        sfx_sweep(3, SND_NOISE, 3200, 140, 170, 200);
        break;
    case SFX_BLADE:
        sfx_sweep(1, SND_TRIANGLE, 420, 130, 160, 140);
        sfx_sweep(3, SND_NOISE, 2400, 600, 100, 120);
        bgm_hold(140);
        break;
    case SFX_PARRY:
        sfx_sweep(0, SND_SQUARE, 1760, 2640, 180, 90);
        sfx_sweep(1, SND_TRIANGLE, 880, 1320, 150, 120);
        bgm_hold(120);
        break;
    case SFX_VENT:
        sfx_sweep(1, SND_TRIANGLE, 520, 1040, 160, 160);
        sfx_sweep(3, SND_NOISE, 3200, 900, 80, 100);
        bgm_hold(160);
        break;
    case SFX_ALARM:
        sfx_sweep(2, SND_SQUARE, 310, 140, 170, 280);
        break;
    case SFX_THROW:
        sfx_sweep(1, SND_TRIANGLE, 260, 620, 130, 110);
        bgm_hold(110);
        break;
    case SFX_BOOM:
        sfx_sweep(2, SND_SQUARE, 190, 40, 210, 360);
        sfx_sweep(3, SND_NOISE, 1600, 80, 200, 400);
        break;
    case SFX_SLOW_IN:
        sfx_sweep(2, SND_TRIANGLE, 640, 110, 150, 280);
        break;
    case SFX_SLOW_OUT:
        sfx_sweep(2, SND_TRIANGLE, 110, 640, 140, 200);
        break;
    case SFX_DOOR:
        sfx_sweep(2, SND_SQUARE, 140, 70, 140, 220);
        sfx_sweep(3, SND_NOISE, 2200, 700, 120, 200);
        break;
    case SFX_LOCK:
        sfx_sweep(1, SND_SQUARE, 150, 110, 160, 160);
        bgm_hold(160);
        break;
    case SFX_PICK:
        sfx_sweep(1, SND_TRIANGLE, 660, 1320, 150, 140);
        bgm_hold(140);
        break;
    case SFX_DROID:
        sfx_sweep(0, SND_SQUARE, 920, 1480, 110, 100);
        break;
    case SFX_HIT:
        sfx_sweep(0, SND_SQUARE, 280, 60, 190, 180);
        sfx_sweep(3, SND_NOISE, 1400, 200, 150, 160);
        break;
    case SFX_SCATTER:
        sfx_sweep(0, SND_SQUARE, 220, 70, 190, 160);
        sfx_sweep(3, SND_NOISE, 1800, 200, 170, 140);
        break;
    case SFX_MINI:
        sfx_sweep(0, SND_SQUARE, 1500, 900, 90, 45);
        break;
    case SFX_MINE:
        sfx_sweep(2, SND_SQUARE, 700, 900, 120, 50);
        break;
    case SFX_DEATH:
        sfx_sweep(0, SND_SQUARE, 520, 40, 200, 520);
        sfx_sweep(3, SND_NOISE, 900, 80, 180, 480);
        break;
    case SFX_STEP:
        sfx_sweep(2, SND_TRIANGLE, 90, 50, 80, 70);
        break;
    case SFX_DRY:
        sfx_sweep(1, SND_SQUARE, 180, 90, 80, 40);
        bgm_hold(40);
        break;
    default:
        break;
    }
}

static void bake_tiles(void) {
    int t, y, x;
    for (t = 0; t < TID_COUNT; t++) {
        const uint8_t (*tile)[16] = ROOM_TILESET[t];
        int keyed = t >= TID_IWALL_H && t <= TID_PIPE_V;
        for (y = 0; y < 16; y++) {
            for (x = 0; x < 16; x++) {
                uint8_t ix = tile[y][x];
                if (keyed && ix == 0) {
                    ix = TILE_FLOOR_PLATE[y][x];
                    if (ix == 0) {
                        ix = 1;
                    }
                }
                g_tile_rgb[t][y * 16 + x] = SCI_FI_PALETTE_32[ix];
            }
        }
    }
}

static void blit(int x, int y, const uint8_t *src, int w, int h, int flipx, int flipy) {
    int row, col;
    if (w <= 0 || h <= 0 || w > 32 || h > 32) {
        return;
    }
    for (row = 0; row < h; row++) {
        int sy = flipy ? h - 1 - row : row;
        const uint8_t *s = src + sy * w;
        uint16_t *d = g_scratch + row * w;
        for (col = 0; col < w; col++) {
            int sx = flipx ? w - 1 - col : col;
            uint8_t ix = s[sx];
            d[col] = ix ? SCI_FI_PALETTE_32[ix] : LIPPUS_MAGENTA;
        }
    }
    gfx_blit(x + g_sox, y + g_soy, w, h, g_scratch, 1, LIPPUS_MAGENTA);
}

static void tile_at(int x, int y, int tid) {
    if ((unsigned)tid >= TID_COUNT) {
        tid = 0;
    }
    gfx_blit(x + g_sox, y + g_soy, 16, 16, g_tile_rgb[tid], 0, 0);
}

static int text_w(const char *s, int scale) {
    int n = 0;
    while (s[n]) {
        n++;
    }
    return n * 9 * scale;
}

static void text(int x, int y, const char *s, int idx, int scale) {
    uint16_t col = C(idx);
    uint16_t sh = C(COL_VOID_BLACK);
    if (scale < 1) {
        scale = 1;
    }
    if (x < 0) {
        x = (320 - text_w(s, scale)) / 2;
        if (x < 0) {
            x = 0;
        }
    }
    while (*s) {
        char ch = *s++;
        const uint8_t *g;
        int row, colb;
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 32);
        }
        if (ch < 32 || ch > 95) {
            ch = '?';
        }
        g = GALACTIC_FONT_8X8[ch - 32];
        for (row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            for (colb = 0; colb < 8; colb++) {
                if (bits & (uint8_t)(0x80 >> colb)) {
                    gfx_fill_rect(x + colb * scale + scale, y + row * scale + scale, scale, scale, sh);
                }
            }
        }
        for (row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            for (colb = 0; colb < 8; colb++) {
                if (bits & (uint8_t)(0x80 >> colb)) {
                    gfx_fill_rect(x + colb * scale, y + row * scale, scale, scale, col);
                }
            }
        }
        x += 9 * scale;
    }
}

static void fmt_u(char *d, unsigned n) {
    char tmp[8];
    int i = 0;
    int j = 0;
    if (n == 0) {
        d[0] = '0';
        d[1] = 0;
        return;
    }
    while (n && i < 7) {
        tmp[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (i > 0) {
        d[j++] = tmp[--i];
    }
    d[j] = 0;
}

static int solid_tid(int tid) {
    return tid == TID_WALL_HULL || tid == TID_CRATE_COVER || tid == TID_DOOR_CLOSED ||
           tid == TID_DOOR_LOCKED || (tid >= TID_IWALL_H && tid <= TID_PIPE_V);
}

static int solid_at(int x, int y) {
    if (x < 0 || y < 0 || x >= 320 || y >= 224) {
        return 1;
    }
    return solid_tid(g_tiles[y >> 4][x >> 4]);
}

static int box_hit(int x, int y, int hw, int hh) {
    return solid_at(x - hw, y - hh) || solid_at(x + hw, y - hh) || solid_at(x - hw, y + hh) ||
           solid_at(x + hw, y + hh);
}

static void try_move(int *x, int *y, int dx, int dy, int hw, int hh) {
    int nx = *x + dx;
    int ny;
    if (!box_hit(nx, *y, hw, hh)) {
        *x = nx;
    }
    ny = *y + dy;
    if (!box_hit(*x, ny, hw, hh)) {
        *y = ny;
    }
}

static int grid_phase(int tx, int ty) {
    if (g_grid_off > 0) {
        return 0;
    }
    return (g_frame / 16 + tx + ty) & 3;
}

static int flame_cell(int tx, int ty) {
    int i, k;
    for (i = 0; i < g_nvent; i++) {
        int blocked = 0;
        if (!g_vent[i].on) {
            continue;
        }
        if (((g_frame / 22 + g_vent[i].tx) % 3) != 2) {
            continue;
        }
        for (k = 1; k <= 2; k++) {
            int cx = g_vent[i].tx + NX[g_vent[i].dir] * k;
            int cy = g_vent[i].ty + NY[g_vent[i].dir] * k;
            if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS) {
                break;
            }
            if (solid_tid(g_tiles[cy][cx])) {
                blocked = 1;
            }
            if (blocked) {
                break;
            }
            if (cx == tx && cy == ty) {
                return 1;
            }
        }
    }
    return 0;
}

static int hazard_dmg(int x, int y) {
    int tx, ty, tid;
    if (x < 0 || y < 0 || x >= 320 || y >= 224) {
        return 0;
    }
    tx = x >> 4;
    ty = y >> 4;
    tid = g_tiles[ty][tx];
    if (tid == TID_HAZARD_GRID_OFF && grid_phase(tx, ty) == 3) {
        return 14;
    }
    if (flame_cell(tx, ty)) {
        return 18;
    }
    return 0;
}

static int los(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = iabs(dx) > iabs(dy) ? iabs(dx) : iabs(dy);
    int i;
    steps /= 8;
    if (steps < 1) {
        steps = 1;
    }
    for (i = 1; i < steps; i++) {
        int x = x0 + dx * i / steps;
        int y = y0 + dy * i / steps;
        if (solid_at(x, y)) {
            return 0;
        }
    }
    return 1;
}

static int in_cone(int x, int y) {
    int dx = x - g_px;
    int dy = y - g_py;
    int dot = dx * DX[g_face] + dy * DY[g_face];
    int cross = dx * DY[g_face] - dy * DX[g_face];
    if (dot <= 0) {
        return 0;
    }
    return iabs(cross) <= dot + 10;
}

static void wipe_actors(void) {
    int i;
    for (i = 0; i < MAX_EN; i++) {
        g_enemy[i].on = 0;
    }
    for (i = 0; i < MAX_SHOT; i++) {
        g_shot[i].on = 0;
    }
    for (i = 0; i < MAX_PICK; i++) {
        g_pick[i].on = 0;
    }
    for (i = 0; i < MAX_MINE; i++) {
        g_mine[i].on = 0;
    }
    for (i = 0; i < MAX_BAR; i++) {
        g_bar[i].on = 0;
    }
    for (i = 0; i < MAX_FX; i++) {
        g_fx[i].on = 0;
    }
    g_nvent = 0;
    g_ncrush = 0;
    g_nbar = 0;
    g_nmine = 0;
    g_bn = 0;
    g_rail_t = 0;
    g_slash = 0;
}

static void stamp_doors(void) {
    Room *r = &g_map[ri(g_rx, g_ry)];
    int d;
    for (d = 0; d < 4; d++) {
        int st = r->door[d];
        int tid = TID_WALL_HULL;
        int nx, ny, seen;
        if (!st) {
            continue;
        }
        nx = g_rx + NX[d];
        ny = g_ry + NY[d];
        seen = inside(nx, ny) && g_map[ri(nx, ny)].visited;
        if (st == D_OPEN) {
            tid = seen ? TID_DOOR_OPEN : TID_FOG;
        } else if (st == D_LOCKED) {
            tid = TID_DOOR_LOCKED;
        } else {
            tid = TID_DOOR_CLOSED;
        }
        if (d == 0 || d == 2) {
            int y = d == 0 ? 0 : 13;
            g_tiles[y][9] = (uint8_t)tid;
            g_tiles[y][10] = (uint8_t)tid;
        } else {
            int x = d == 3 ? 0 : 19;
            g_tiles[6][x] = (uint8_t)tid;
            g_tiles[7][x] = (uint8_t)tid;
        }
    }
}

static void build_geometry(void) {
    const Room *r = &g_map[ri(g_rx, g_ry)];
    int ty, tx;
    int rails[2];
    int nrail = 0;
    wipe_actors();
    for (ty = 0; ty < ROWS; ty++) {
        for (tx = 0; tx < COLS; tx++) {
            char ch = TMPL[r->tmpl][ty][tx];
            int tid = TID_FLOOR_PLATE;
            if (ch == '#') {
                tid = TID_WALL_HULL;
            } else if (ch == 'C') {
                tid = TID_CRATE_COVER;
            } else if (ch == ',') {
                tid = TID_FLOOR_GRATE;
            } else if (ch == 'G') {
                tid = TID_HAZARD_GRID_OFF;
            } else if (ch == 'V') {
                tid = TID_HAZARD_VENT;
                if (g_nvent < MAX_VENT) {
                    int dir = 1;
                    if (tx == 0) {
                        dir = 1;
                    } else if (tx == 19) {
                        dir = 3;
                    } else if (ty == 0) {
                        dir = 2;
                    } else {
                        dir = 0;
                    }
                    g_vent[g_nvent].tx = tx;
                    g_vent[g_nvent].ty = ty;
                    g_vent[g_nvent].dir = dir;
                    g_vent[g_nvent].on = 1;
                    g_nvent++;
                }
            } else if (ch == 'R') {
                tid = TID_FLOOR_GRATE;
                if (nrail == 0 || rails[nrail - 1] != ty) {
                    if (nrail < 2) {
                        rails[nrail++] = ty;
                    }
                }
            } else if (ch == '[') {
                tid = TID_IWALL_HL;
            } else if (ch == '-') {
                tid = TID_IWALL_H;
            } else if (ch == ']') {
                tid = TID_IWALL_HR;
            } else if (ch == '|') {
                tid = TID_IWALL_V;
            } else if (ch == 'P') {
                tid = TID_IWALL_PILLAR;
            } else if (ch == 'T') {
                tid = TID_MACH_CONSOLE;
            } else if (ch == 'S') {
                tid = TID_MACH_SERVER;
            } else if (ch == 'N') {
                tid = TID_MACH_GENERATOR;
            } else if (ch == '=') {
                tid = TID_PIPE_H;
            } else if (ch == '!') {
                tid = TID_PIPE_V;
            } else if (ch == 'Z') {
                tid = TID_FLOOR_CAUTION;
            } else if (ch == '+') {
                tid = TID_FLOOR_PAD;
            } else if (ch == 'B') {
                tid = TID_FLOOR_PLATE;
                if (g_nbar < MAX_BAR) {
                    g_bar[g_nbar].x = tx * 16 + 8;
                    g_bar[g_nbar].y = ty * 16 + 8;
                    g_bar[g_nbar].hp = 16;
                    g_bar[g_nbar].on = 1;
                    g_nbar++;
                }
            }
            g_tiles[ty][tx] = (uint8_t)tid;
        }
    }
    for (tx = 0; tx < nrail && g_ncrush < MAX_CRUSH; tx++) {
        g_crush[g_ncrush].x = 40;
        g_crush[g_ncrush].y = rails[tx] * 16 + 8;
        g_crush[g_ncrush].dir = (tx & 1) ? -1 : 1;
        g_crush[g_ncrush].minx = 24;
        g_crush[g_ncrush].maxx = 296;
        g_crush[g_ncrush].on = 1;
        g_ncrush++;
    }
    stamp_doors();
}

static int spawn_cell_ok(int tx, int ty) {
    int tid;
    if (tx < 2 || ty < 2 || tx > 17 || ty > 11) {
        return 0;
    }
    if ((tx == 9 || tx == 10) && (ty < 4 || ty > 9)) {
        return 0;
    }
    if ((ty == 6 || ty == 7) && (tx < 4 || tx > 15)) {
        return 0;
    }
    tid = g_tiles[ty][tx];
    return tid == TID_FLOOR_PLATE || tid == TID_FLOOR_GRATE || tid == TID_FLOOR_CAUTION ||
           tid == TID_FLOOR_PAD;
}

static int pick_cell(int *ox, int *oy) {
    int n, i, tx, ty;
    for (n = 0; n < 40; n++) {
        tx = 2 + (int)(rnd() % 16);
        ty = 2 + (int)(rnd() % 10);
        if (!spawn_cell_ok(tx, ty)) {
            continue;
        }
        *ox = tx * 16 + 8;
        *oy = ty * 16 + 8;
        if (iabs(*ox - g_px) + iabs(*oy - g_py) < 48) {
            continue;
        }
        for (i = 0; i < MAX_EN; i++) {
            if (g_enemy[i].on && iabs(g_enemy[i].x - *ox) < 20 && iabs(g_enemy[i].y - *oy) < 20) {
                break;
            }
        }
        if (i == MAX_EN) {
            return 1;
        }
    }
    return 0;
}

static int spawn_pick(int x, int y, int kind, int val) {
    int i;
    for (i = 0; i < MAX_PICK; i++) {
        if (!g_pick[i].on) {
            g_pick[i].x = x;
            g_pick[i].y = y;
            g_pick[i].kind = kind;
            g_pick[i].val = val;
            g_pick[i].on = 1;
            return 1;
        }
    }
    return 0;
}

static void spawn_enemy(int kind, int x, int y) {
    int i;
    Enemy *e = 0;
    for (i = 0; i < MAX_EN; i++) {
        if (!g_enemy[i].on) {
            e = &g_enemy[i];
            break;
        }
    }
    if (!e) {
        return;
    }
    e->on = 1;
    e->kind = kind;
    e->x = x;
    e->y = y;
    e->cd = 20 + (int)(rnd() % 20);
    e->burst = 0;
    e->shield = kind == EK_ELITE ? 1 : 0;
    e->kt = 0;
    e->kvx = 0;
    e->kvy = 0;
    e->strafe = 1;
    if (kind == EK_GRUNT) {
        e->hp = 30;
    } else if (kind == EK_DROID) {
        e->hp = 16;
        sfx(SFX_DROID);
    } else if (kind == EK_ELITE) {
        e->hp = 52;
    } else if (kind == EK_BRUTE) {
        e->hp = 120;
    } else {
        e->hp = 220 + g_deck * 110;
        e->x = 160;
        e->y = 104;
    }
    e->maxhp = e->hp;
}

static void spawn_mine(void) {
    int x, y, i;
    if (g_nmine >= MAX_MINE || !pick_cell(&x, &y)) {
        return;
    }
    for (i = 0; i < MAX_MINE; i++) {
        if (!g_mine[i].on) {
            g_mine[i].x = x;
            g_mine[i].y = y;
            g_mine[i].fuse = -1;
            g_mine[i].on = 1;
            g_nmine++;
            return;
        }
    }
}

static void spawn_population(void) {
    Room *r = &g_map[ri(g_rx, g_ry)];
    int x, y;
    int sig = g_deck == 0 ? PK_WSC : (g_deck == 1 ? PK_WRA : PK_WMI);
    int bit = g_deck == 0 ? 2 : (g_deck == 1 ? 4 : 8);
    if (r->kind == K_START) {
        r->cleared = 1;
        r->looted = 1;
        return;
    }
    if (!r->looted && r->kind == K_LOOT) {
        if (pick_cell(&x, &y)) {
            spawn_pick(x, y, PK_MED, 35);
        }
        if (pick_cell(&x, &y)) {
            spawn_pick(x, y, (rnd() & 1) ? PK_YEL : PK_FUS, 1);
        }
        r->looted = 1;
        r->cleared = 1;
        return;
    }
    if (r->kind == K_KEY && !r->looted) {
        if (pick_cell(&x, &y)) {
            if (g_held & bit) {
                spawn_pick(x, y, g_deck == 0 ? PK_FUS : PK_YEL, 1);
            } else {
                spawn_pick(x, y, sig, 1);
            }
        }
        r->looted = 1;
    }
    if (r->kind == K_KEY && !g_has_key && !g_key_used && pick_cell(&x, &y)) {
        spawn_pick(x, y, PK_KEY, 1);
    }
    if (r->cleared) {
        return;
    }
    if (r->kind == K_BOSS) {
        spawn_enemy(EK_BOSS, 160, 104);
        return;
    }
    if (!r->looted && r->kind == K_COMBAT) {
        if ((rnd() % 4) == 0 && pick_cell(&x, &y)) {
            spawn_pick(x, y, PK_MED, 35);
        }
        r->looted = 1;
    }
    if (r->kind == K_KEY) {
        if (pick_cell(&x, &y)) {
            spawn_enemy(g_deck == 0 ? EK_DROID : EK_ELITE, x, y);
        }
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_GRUNT, x, y);
        }
    } else if (g_deck == 0) {
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_GRUNT, x, y);
        }
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_GRUNT, x, y);
        }
        if (r->dist >= 3 && pick_cell(&x, &y)) {
            spawn_enemy(EK_DROID, x, y);
        }
    } else if (g_deck == 1) {
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_ELITE, x, y);
        }
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_GRUNT, x, y);
        }
        if ((rnd() & 1) && pick_cell(&x, &y)) {
            spawn_enemy(EK_DROID, x, y);
        }
    } else if (rnd() & 1) {
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_BRUTE, x, y);
        }
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_GRUNT, x, y);
        }
    } else {
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_ELITE, x, y);
        }
        if (pick_cell(&x, &y)) {
            spawn_enemy(EK_DROID, x, y);
        }
    }
    if (r->kind == K_COMBAT && (g_deck > 0 || (rnd() % 3) == 0)) {
        spawn_mine();
        if (g_deck == 2) {
            spawn_mine();
        }
    }
}

static void enter_at(int x, int y, int travel, int breach) {
    int first;
    g_rx = x;
    g_ry = y;
    first = !g_map[ri(x, y)].visited;
    if (travel < 0) {
        g_px = 160;
        g_py = 112;
    } else if (OPP[travel] == 0) {
        g_px = 160;
        g_py = 40;
    } else if (OPP[travel] == 2) {
        g_px = 160;
        g_py = 184;
    } else if (OPP[travel] == 3) {
        g_px = 40;
        g_py = 112;
    } else {
        g_px = 280;
        g_py = 112;
    }
    build_geometry();
    if (box_hit(g_px, g_py, 5, 6)) {
        g_px = 160;
        g_py = 112;
    }
    spawn_population();
    g_map[ri(x, y)].visited = 1;
    if (breach && first) {
        g_slow = 14;
        sfx(SFX_SLOW_IN);
        if (g_map[ri(x, y)].kind == K_BOSS) {
            banner(BOSS_NAME[g_deck], 90);
        }
    }
}

static void shuffle4(int *o) {
    int i;
    for (i = 3; i > 0; i--) {
        int j = (int)(rnd() % (unsigned)(i + 1));
        int t = o[i];
        o[i] = o[j];
        o[j] = t;
    }
}

static int bfs_far(int sx, int sy) {
    int qh = 0;
    int qt = 0;
    int start = ri(sx, sy);
    int i, far, best;
    for (i = 0; i < 64; i++) {
        g_parent[i] = -1;
        g_bfs_dist[i] = -1;
    }
    g_q[qt++] = (uint8_t)start;
    g_bfs_dist[start] = 0;
    g_parent[start] = start;
    while (qh < qt) {
        int id = g_q[qh++];
        int x = id % MAP;
        int y = id / MAP;
        int d;
        for (d = 0; d < 4; d++) {
            int nx, ny, n;
            if (!g_map[id].door[d]) {
                continue;
            }
            nx = x + NX[d];
            ny = y + NY[d];
            if (!inside(nx, ny)) {
                continue;
            }
            n = ri(nx, ny);
            if (g_bfs_dist[n] >= 0) {
                continue;
            }
            g_bfs_dist[n] = g_bfs_dist[id] + 1;
            g_parent[n] = id;
            g_q[qt++] = (uint8_t)n;
        }
    }
    far = start;
    best = 0;
    for (i = 0; i < 64; i++) {
        if (g_bfs_dist[i] > best) {
            best = g_bfs_dist[i];
            far = i;
        }
    }
    return far;
}

static int dir_between(int fx, int fy, int tx, int ty) {
    if (tx == fx && ty == fy - 1) {
        return 0;
    }
    if (tx == fx + 1 && ty == fy) {
        return 1;
    }
    if (tx == fx && ty == fy + 1) {
        return 2;
    }
    return 3;
}

static void gen_deck(uint32_t seed, int deck) {
    int sx = 2;
    int sy = 5;
    int sp = 0;
    int count = 1;
    int guard = 0;
    int stackx[64], stacky[64];
    int i, added, boss, prev, key, loot, far_d;
    g_seed = seed ? seed : 1;
    g_rng = g_seed;
    g_deck = deck;
    g_start_x = sx;
    g_start_y = sy;
    for (i = 0; i < 64; i++) {
        int d;
        g_map[i].on = 0;
        g_map[i].kind = K_COMBAT;
        g_map[i].tmpl = 0;
        g_map[i].dist = 0;
        g_map[i].visited = 0;
        g_map[i].cleared = 0;
        g_map[i].looted = 0;
        for (d = 0; d < 4; d++) {
            g_map[i].door[d] = D_NONE;
        }
        g_path[i] = 0;
    }
    g_map[ri(sx, sy)].on = 1;
    stackx[0] = sx;
    stacky[0] = sy;
    sp = 1;
    while (sp > 0 && count < 18 && guard++ < 500) {
        int x = stackx[sp - 1];
        int y = stacky[sp - 1];
        int order[4] = {0, 1, 2, 3};
        int stepped = 0;
        int k;
        shuffle4(order);
        for (k = 0; k < 4; k++) {
            int d = order[k];
            int nx = x + NX[d];
            int ny = y + NY[d];
            if (!inside(nx, ny) || g_map[ri(nx, ny)].on) {
                continue;
            }
            g_map[ri(nx, ny)].on = 1;
            g_map[ri(x, y)].door[d] = D_CLOSED;
            g_map[ri(nx, ny)].door[OPP[d]] = D_CLOSED;
            stackx[sp] = nx;
            stacky[sp] = ny;
            sp++;
            count++;
            stepped = 1;
            break;
        }
        if (!stepped) {
            sp--;
        }
    }
    boss = bfs_far(sx, sy);
    prev = g_parent[boss];
    i = boss;
    g_path[ri(sx, sy)] = 1;
    for (guard = 0; i >= 0 && guard++ < 64; guard++) {
        g_path[i] = 1;
        if (i == ri(sx, sy)) {
            break;
        }
        i = g_parent[i];
    }
    if (prev >= 0 && prev != boss) {
        int bx = boss % MAP;
        int by = boss / MAP;
        int px = prev % MAP;
        int py = prev / MAP;
        int d = dir_between(bx, by, px, py);
        g_map[boss].door[d] = D_LOCKED;
        g_map[prev].door[OPP[d]] = D_LOCKED;
    }
    added = 0;
    for (i = 0; i < 40 && added < 3; i++) {
        int x = (int)(rnd() % MAP);
        int y = (int)(rnd() % MAP);
        int d = (int)(rnd() % 4);
        int nx = x + NX[d];
        int ny = y + NY[d];
        int a, b;
        if (!inside(nx, ny)) {
            continue;
        }
        a = ri(x, y);
        b = ri(nx, ny);
        if (!g_map[a].on || !g_map[b].on || g_map[a].door[d]) {
            continue;
        }
        if (a == boss || b == boss) {
            continue;
        }
        g_map[a].door[d] = D_CLOSED;
        g_map[b].door[OPP[d]] = D_CLOSED;
        added++;
    }
    bfs_far(sx, sy);
    for (i = 0; i < 64; i++) {
        if (g_map[i].on && g_bfs_dist[i] > 0) {
            g_map[i].dist = (uint8_t)g_bfs_dist[i];
        }
    }
    key = -1;
    far_d = -1;
    for (i = 0; i < 64; i++) {
        if (!g_map[i].on || g_path[i] || i == boss) {
            continue;
        }
        if (g_bfs_dist[i] > far_d) {
            far_d = g_bfs_dist[i];
            key = i;
        }
    }
    if (key < 0) {
        for (i = 0; i < 64; i++) {
            if (g_map[i].on && i != boss && i != ri(sx, sy)) {
                key = i;
                break;
            }
        }
    }
    loot = -1;
    for (i = 0; i < 64; i++) {
        if (!g_map[i].on || i == boss || i == ri(sx, sy) || i == key) {
            continue;
        }
        if (!g_path[i]) {
            loot = i;
            break;
        }
    }
    if (loot < 0) {
        for (i = 0; i < 64; i++) {
            if (g_map[i].on && i != boss && i != ri(sx, sy) && i != key) {
                loot = i;
                break;
            }
        }
    }
    for (i = 0; i < 64; i++) {
        int kind = K_COMBAT;
        if (!g_map[i].on) {
            continue;
        }
        if (i == ri(sx, sy)) {
            kind = K_START;
        } else if (i == boss) {
            kind = K_BOSS;
        } else if (i == key) {
            kind = K_KEY;
        } else if (i == loot) {
            kind = K_LOOT;
        }
        g_map[i].kind = (uint8_t)kind;
        if (kind == K_START || kind == K_LOOT) {
            g_map[i].tmpl = 0;
        } else if (kind == K_BOSS) {
            g_map[i].tmpl = 6;
        } else if (kind == K_KEY) {
            g_map[i].tmpl = 7;
        } else if (deck == 0) {
            g_map[i].tmpl = (uint8_t)((rnd() & 1) ? 1 : 0);
        } else if (deck == 1) {
            static const uint8_t tab[4] = {1, 2, 3, 4};
            g_map[i].tmpl = tab[rnd() % 4];
        } else {
            static const uint8_t tab[4] = {1, 3, 5, 5};
            g_map[i].tmpl = tab[rnd() % 4];
        }
    }
    g_map[ri(sx, sy)].visited = 1;
    g_map[ri(sx, sy)].cleared = 1;
    g_map[ri(sx, sy)].looted = 1;
}

static void queue_boom(int x, int y, int dmg);
static void kill_enemy(int index, int close);
static void damage_enemy(int index, int dmg, int kx, int ky, int close, int pierce);

static void queue_boom(int x, int y, int dmg) {
    if (g_bn >= MAX_BOOM) {
        return;
    }
    g_bx[g_bn] = x;
    g_by[g_bn] = y;
    g_bd[g_bn] = dmg;
    g_bn++;
}

static void spawn_fx(int x, int y) {
    int i;
    for (i = 0; i < MAX_FX; i++) {
        if (!g_fx[i].on) {
            g_fx[i].x = x;
            g_fx[i].y = y;
            g_fx[i].t = 12;
            g_fx[i].on = 1;
            return;
        }
    }
}

static void hurt_player(int dmg) {
    int to_hp, to_sh;
    if (dmg <= 0 || g_iframes > 0 || g_state != ST_PLAY || g_hp <= 0) {
        return;
    }
    to_hp = dmg / 4;
    to_sh = dmg - to_hp;
    if (to_sh > g_sp) {
        to_hp += to_sh - g_sp;
        to_sh = g_sp;
    }
    g_sp -= to_sh;
    g_hp -= to_hp;
    if (g_hp < 0) {
        g_hp = 0;
    }
    g_iframes = 22;
    g_combat = 100;
    g_shake = 5;
    sfx(SFX_HIT);
}

static void maybe_drop(int x, int y, int kind, int close) {
    int roll = (int)(rnd() % 100);
    int bonus = close ? 18 : 0;
    if (kind == EK_GRUNT) {
        if (roll < 24 + bonus) {
            spawn_pick(x, y, PK_MED, 35);
        } else if (roll < 55 + bonus) {
            spawn_pick(x, y, (rnd() & 1) ? PK_FUS : PK_YEL, 1);
        }
    } else if (kind == EK_DROID) {
        if (roll < 20 + bonus) {
            spawn_pick(x, y, PK_MED, 35);
        }
    } else     if (kind == EK_ELITE) {
        int pool = g_unlocks & ~g_held & 0x0E;
        if (pool && roll < 35) {
            int w = (pool & 2) ? PK_WSC : ((pool & 4) ? PK_WRA : PK_WMI);
            spawn_pick(x, y, w, 1);
        } else if (roll < 30 + bonus) {
            spawn_pick(x, y, PK_MED, 35);
        } else {
            spawn_pick(x, y, PK_YEL, 1);
        }
        if (!spawn_pick(x + 8, y, PK_DATA, 1)) {
            grant_data(1);
        }
    } else if (kind == EK_BRUTE) {
        spawn_pick(x, y, PK_MED, 35);
        spawn_pick(x + 10, y, PK_YEL, 1);
        if (!spawn_pick(x - 8, y, PK_DATA, 2)) {
            grant_data(2);
        }
    } else if (kind == EK_BOSS) {
        spawn_pick(x, y + 14, PK_MED, 35);
        spawn_pick(x + 14, y, PK_SH, 25);
        spawn_pick(x - 14, y, PK_RED, 1);
        if (!spawn_pick(x, y - 14, PK_DATA, 3)) {
            grant_data(3);
        }
    }
}

static void kill_enemy(int index, int close) {
    Enemy *e = &g_enemy[index];
    int k, x, y;
    if (!e->on) {
        return;
    }
    k = e->kind;
    x = e->x;
    y = e->y;
    e->on = 0;
    maybe_drop(x, y, k, close);
    spawn_fx(x, y);
    if (k == EK_DROID) {
        queue_boom(x, y, 26);
    }
    if (k == EK_BOSS) {
        g_boss_down = 1;
    }
}

static int frontal_shield(const Enemy *e, int vx, int vy) {
    int tox = e->x - g_px;
    int toy = e->y - g_py;
    return vx * tox + vy * toy > 0;
}

static void damage_enemy(int index, int dmg, int kx, int ky, int close, int pierce) {
    Enemy *e = &g_enemy[index];
    if (!e->on || dmg <= 0) {
        return;
    }
    if (e->shield && e->kind == EK_ELITE) {
        if (pierce) {
            e->shield = 0;
        } else if (frontal_shield(e, kx, ky)) {
            sfx(SFX_PARRY);
            return;
        }
    }
    e->hp -= dmg;
    if (e->kind != EK_BOSS) {
        if (e->kind == EK_BRUTE) {
            kx /= 2;
            ky /= 2;
        }
        e->kvx = kx;
        e->kvy = ky;
        e->kt = 5;
    }
    if (e->hp <= 0) {
        kill_enemy(index, close);
    }
}

static void apply_boom(int x, int y, int dmg) {
    int i;
    int rad = 30;
    spawn_fx(x, y);
    g_shake = 7;
    if (dmg >= 30) {
        g_grid_off = 160;
    }
    if (iabs(g_px - x) < rad && iabs(g_py - y) < rad) {
        hurt_player(dmg);
    }
    for (i = 0; i < MAX_EN; i++) {
        int hw = g_enemy[i].kind == EK_BRUTE ? 12 : 10;
        if (!g_enemy[i].on) {
            continue;
        }
        if (iabs(g_enemy[i].x - x) <= rad + hw && iabs(g_enemy[i].y - y) <= rad + hw) {
            damage_enemy(i, dmg, g_enemy[i].x - x, g_enemy[i].y - y, 0, 1);
        }
    }
    for (i = 0; i < MAX_BAR; i++) {
        if (!g_bar[i].on) {
            continue;
        }
        if (iabs(g_bar[i].x - x) < rad && iabs(g_bar[i].y - y) < rad) {
            g_bar[i].on = 0;
            queue_boom(g_bar[i].x, g_bar[i].y, 36);
        }
    }
    for (i = 0; i < MAX_MINE; i++) {
        if (!g_mine[i].on) {
            continue;
        }
        if (iabs(g_mine[i].x - x) < rad && iabs(g_mine[i].y - y) < rad) {
            g_mine[i].on = 0;
            queue_boom(g_mine[i].x, g_mine[i].y, 28);
        }
    }
}

static void run_booms(void) {
    int i = 0;
    int any = g_bn > 0;
    while (i < g_bn && i < MAX_BOOM) {
        int x = g_bx[i];
        int y = g_by[i];
        int d = g_bd[i];
        i++;
        apply_boom(x, y, d);
    }
    g_bn = 0;
    if (any) {
        sfx(SFX_BOOM);
    }
}

static int spawn_shot(int x, int y, int vx, int vy, int life, int dmg, int team, int kind, int slow) {
    int i;
    for (i = 0; i < MAX_SHOT; i++) {
        if (!g_shot[i].on) {
            g_shot[i].x = x;
            g_shot[i].y = y;
            g_shot[i].vx = vx;
            g_shot[i].vy = vy;
            g_shot[i].life = life;
            g_shot[i].dmg = dmg;
            g_shot[i].team = team;
            g_shot[i].kind = kind;
            g_shot[i].slow = slow;
            g_shot[i].on = 1;
            return 1;
        }
    }
    return 0;
}

static void vel_toward(int x, int y, int tx, int ty, int speed, int *vx, int *vy) {
    int dx = tx - x;
    int dy = ty - y;
    int ax = iabs(dx);
    int ay = iabs(dy);
    int steps = ax > ay ? ax : ay;
    if (steps < 1) {
        steps = 1;
    }
    *vx = dx * speed / steps;
    *vy = dy * speed / steps;
    if (*vx == 0 && *vy == 0) {
        *vx = speed;
    }
}

static void aim_player(int speed, int *vx, int *vy) {
    if (g_dot >= 0 && g_dot < MAX_EN && g_enemy[g_dot].on) {
        vel_toward(g_px, g_py, g_enemy[g_dot].x, g_enemy[g_dot].y, speed, vx, vy);
    } else {
        *vx = DX[g_face] * speed;
        *vy = DY[g_face] * speed;
        if (*vx == 0 && *vy == 0) {
            *vy = speed;
        }
    }
}

static int find_dot(void) {
    int best = -1;
    int best_d = 999999;
    int range = 78 + g_dot_rank * 28;
    int i;
    for (i = 0; i < MAX_EN; i++) {
        int dx, dy, d2, dot, cross;
        if (!g_enemy[i].on) {
            continue;
        }
        dx = g_enemy[i].x - g_px;
        dy = g_enemy[i].y - g_py;
        d2 = dx * dx + dy * dy;
        if (d2 > range * range) {
            continue;
        }
        dot = dx * DX[g_face] + dy * DY[g_face];
        cross = dx * DY[g_face] - dy * DX[g_face];
        if (dot <= 0 || iabs(cross) > dot) {
            continue;
        }
        if (!los(g_px, g_py, g_enemy[i].x, g_enemy[i].y)) {
            continue;
        }
        if (d2 < best_d) {
            best_d = d2;
            best = i;
        }
    }
    return best;
}

static int nearest_door(void) {
    Room *r = &g_map[ri(g_rx, g_ry)];
    int best = -1;
    int best_d = 40 * 40;
    int d;
    for (d = 0; d < 4; d++) {
        int cx = 160;
        int cy = 112;
        int dx, dy, d2;
        if (r->door[d] != D_CLOSED && r->door[d] != D_LOCKED) {
            continue;
        }
        if (d == 0) {
            cy = 8;
        } else if (d == 2) {
            cy = 216;
        } else if (d == 3) {
            cx = 8;
        } else {
            cx = 312;
        }
        dx = g_px - cx;
        dy = g_py - cy;
        d2 = dx * dx + dy * dy;
        if (d2 < best_d) {
            best_d = d2;
            best = d;
        }
    }
    return best;
}

static void set_link(int d, int st) {
    int nx = g_rx + NX[d];
    int ny = g_ry + NY[d];
    g_map[ri(g_rx, g_ry)].door[d] = (uint8_t)st;
    if (inside(nx, ny) && g_map[ri(nx, ny)].on) {
        g_map[ri(nx, ny)].door[OPP[d]] = (uint8_t)st;
    }
}

static void use_door(int d) {
    Room *r = &g_map[ri(g_rx, g_ry)];
    if (r->door[d] == D_LOCKED) {
        if (!g_has_key) {
            sfx(SFX_LOCK);
            banner("KEYCARD REQUIRED", 50);
            return;
        }
        g_has_key = 0;
        g_key_used = 1;
    }
    set_link(d, D_OPEN);
    stamp_doors();
    sfx(SFX_DOOR);
}

static int stood_door(void) {
    int tx = g_px >> 4;
    int ty = g_py >> 4;
    int tid;
    if (tx < 0 || ty < 0 || tx >= COLS || ty >= ROWS) {
        return -1;
    }
    tid = g_tiles[ty][tx];
    if (tid != TID_DOOR_OPEN && tid != TID_FOG) {
        return -1;
    }
    if (ty == 0) {
        return 0;
    }
    if (ty == 13) {
        return 2;
    }
    if (tx == 0) {
        return 3;
    }
    if (tx == 19) {
        return 1;
    }
    return -1;
}

static void try_vent(void) {
    if (g_weapon != W_PISTOL) {
        return;
    }
    if (g_heat <= 0 && g_lock <= 0) {
        return;
    }
    if (g_lock <= 0 && g_heat >= 55 && g_heat <= 75) {
        g_heat = 0;
        g_over = 3;
        sfx(SFX_VENT);
        banner("VENT", 30);
    } else {
        g_lock = 45;
        g_heat = 100;
        sfx(SFX_ALARM);
    }
}

static void fire_weapon(void) {
    int vx, vy, i, dmg, kind;
    if (g_cd > 0 || g_lock > 0) {
        return;
    }
    g_dot = find_dot();
    if (g_weapon == W_PISTOL) {
        int gain = 22 - g_heat_rank * 5;
        if (gain < 8) {
            gain = 8;
        }
        dmg = g_over > 0 ? 18 : 12;
        kind = g_over > 0 ? SK_OC : SK_PULSE;
        if (g_over > 0) {
            g_over--;
            sfx(SFX_OC);
        } else {
            sfx(SFX_PULSE);
        }
        aim_player(6, &vx, &vy);
        spawn_shot(g_px, g_py, vx, vy, 26, dmg, 0, kind, 0);
        g_heat += gain;
        if (g_heat >= 100) {
            g_heat = 100;
            g_lock = 45;
            sfx(SFX_ALARM);
        }
        g_cd = 8;
        g_combat = 80;
    } else if (g_weapon == W_SCATTER) {
        if (g_ammo_s <= 0) {
            sfx(SFX_DRY);
            g_cd = 10;
            return;
        }
        g_ammo_s--;
        sfx(SFX_SCATTER);
        for (i = -2; i <= 2; i++) {
            int px = -DY[g_face];
            int py = DX[g_face];
            aim_player(5, &vx, &vy);
            vx += px * i;
            vy += py * i;
            spawn_shot(g_px, g_py, vx, vy, 8, 8, 0, SK_SCAT, 0);
        }
        g_cd = 16;
        g_combat = 80;
    } else if (g_weapon == W_RAIL) {
        int x, y, s;
        int hit[MAX_EN];
        if (g_ammo_y < 2) {
            sfx(SFX_DRY);
            g_cd = 10;
            return;
        }
        g_ammo_y -= 2;
        sfx(SFX_RAIL);
        aim_player(6, &vx, &vy);
        x = g_px;
        y = g_py;
        for (i = 0; i < MAX_EN; i++) {
            hit[i] = 0;
        }
        for (s = 0; s < 36; s++) {
            int b;
            x += vx;
            y += vy;
            if (solid_at(x, y)) {
                break;
            }
            for (i = 0; i < MAX_EN; i++) {
                int hw = g_enemy[i].kind == EK_BOSS ? 16 : (g_enemy[i].kind == EK_BRUTE ? 12 : 8);
                if (!g_enemy[i].on || hit[i]) {
                    continue;
                }
                if (iabs(g_enemy[i].x - x) <= hw && iabs(g_enemy[i].y - y) <= hw) {
                    hit[i] = 1;
                    damage_enemy(i, 42, vx, vy, 0, 1);
                }
            }
            for (b = 0; b < MAX_BAR; b++) {
                if (g_bar[b].on && iabs(g_bar[b].x - x) < 8 && iabs(g_bar[b].y - y) < 8) {
                    g_bar[b].hp -= 42;
                    if (g_bar[b].hp <= 0) {
                        g_bar[b].on = 0;
                        queue_boom(g_bar[b].x, g_bar[b].y, 36);
                    }
                }
            }
        }
        g_rail_x = x;
        g_rail_y = y;
        g_rail_t = 4;
        g_cd = 26;
        g_combat = 80;
    } else if (g_weapon == W_MINI) {
        if (g_ammo_y <= 0) {
            sfx(SFX_DRY);
            g_cd = 8;
            return;
        }
        g_ammo_y--;
        sfx(SFX_MINI);
        aim_player(7, &vx, &vy);
        spawn_shot(g_px, g_py, vx, vy, 18, 6, 0, SK_PULSE, 0);
        g_cd = 3;
        g_combat = 80;
    }
}

static void do_melee(void) {
    int i;
    if (g_melee_cd > 0) {
        return;
    }
    g_slash = 8;
    g_melee_cd = 16;
    sfx(SFX_BLADE);
    g_combat = 70;
    for (i = 0; i < MAX_EN; i++) {
        int dx, dy;
        if (!g_enemy[i].on) {
            continue;
        }
        dx = g_enemy[i].x - g_px;
        dy = g_enemy[i].y - g_py;
        if (iabs(dx) > 26 || iabs(dy) > 26 || !in_cone(g_enemy[i].x, g_enemy[i].y)) {
            continue;
        }
        if (g_enemy[i].shield) {
            g_enemy[i].shield = 0;
        }
        damage_enemy(i, 28, DX[g_face] * 6, DY[g_face] * 6, 1, 1);
    }
    for (i = 0; i < MAX_SHOT; i++) {
        if (!g_shot[i].on || !g_shot[i].team || !g_shot[i].slow) {
            continue;
        }
        if (iabs(g_shot[i].x - g_px) < 22 && iabs(g_shot[i].y - g_py) < 22 &&
            in_cone(g_shot[i].x, g_shot[i].y)) {
            g_shot[i].vx = -g_shot[i].vx;
            g_shot[i].vy = -g_shot[i].vy;
            g_shot[i].team = 0;
            g_shot[i].dmg += 4;
            sfx(SFX_PARRY);
        }
    }
}

static void do_grenade(void) {
    int vx, vy;
    if (g_cd > 0) {
        return;
    }
    if (g_grenades <= 0) {
        sfx(SFX_DRY);
        g_cd = 12;
        return;
    }
    g_grenades--;
    aim_player(3, &vx, &vy);
    spawn_shot(g_px, g_py, vx, vy, 18, 48, 0, SK_NADE, 0);
    sfx(SFX_THROW);
    g_cd = 18;
    g_combat = 80;
}

static void take_pick(int i) {
    Pick *p = &g_pick[i];
    int bit = 0;
    int wep = -1;
    if (p->kind == PK_MED) {
        g_hp += 35;
        if (g_hp > hp_max()) {
            g_hp = hp_max();
        }
    } else if (p->kind == PK_SH) {
        g_sp += 25;
        if (g_sp > 50) {
            g_sp = 50;
        }
    } else if (p->kind == PK_FUS) {
        g_ammo_s += 10;
        if (g_ammo_s > 80) {
            g_ammo_s = 80;
        }
    } else if (p->kind == PK_YEL) {
        g_ammo_y += 28;
        if (g_ammo_y > 200) {
            g_ammo_y = 200;
        }
    } else if (p->kind == PK_RED) {
        if (g_grenades < 4) {
            g_grenades++;
        }
    } else if (p->kind == PK_KEY) {
        g_has_key = 1;
        banner("KEYCARD", 50);
    } else if (p->kind == PK_DATA) {
        grant_data(p->val > 0 ? p->val : 1);
        banner("TACTICAL DATA", 40);
    } else if (p->kind == PK_WSC) {
        bit = 2;
        wep = W_SCATTER;
        g_ammo_s += 16;
    } else if (p->kind == PK_WRA) {
        bit = 4;
        wep = W_RAIL;
        g_ammo_y += 36;
    } else if (p->kind == PK_WMI) {
        bit = 8;
        wep = W_MINI;
        g_ammo_y += 50;
    }
    if (bit) {
        g_held |= bit;
        g_unlocks |= bit;
        g_weapon = wep;
        if (g_ammo_s > 80) {
            g_ammo_s = 80;
        }
        if (g_ammo_y > 200) {
            g_ammo_y = 200;
        }
        banner(WEP_NAME[wep], 50);
        flush_disk();
    }
    p->on = 0;
    sfx(SFX_PICK);
}

static void cap_ammo_fields(void) {
    if (g_ammo_s > 200) {
        g_ammo_s = 200;
    }
    if (g_ammo_y > 200) {
        g_ammo_y = 200;
    }
}

static void die(void) {
    g_hp = 0;
    g_state = ST_DEAD;
    g_wait = 40;
    sfx(SFX_DEATH);
    flush_disk();
}

static void cycle_weapon(void) {
    int w = g_weapon;
    int i;
    for (i = 0; i < 4; i++) {
        w = (w + 1) & 3;
        if (g_held & (1 << w)) {
            g_weapon = w;
            sfx(SFX_PICK);
            return;
        }
    }
}

static int enemy_half(const Enemy *e) {
    if (e->kind == EK_BOSS) {
        return 14;
    }
    if (e->kind == EK_BRUTE) {
        return 10;
    }
    return 7;
}

static void step_axis(Enemy *e, int dx, int dy, int step) {
    int sx = 0;
    int sy = 0;
    int hw = enemy_half(e);
    int ox, oy;
    if (dx == 0 && dy == 0) {
        return;
    }
    if (iabs(dx) >= iabs(dy)) {
        sx = dx > 0 ? step : -step;
    } else {
        sy = dy > 0 ? step : -step;
    }
    ox = e->x;
    oy = e->y;
    try_move(&e->x, &e->y, sx, sy, hw, hw);
    if (e->x != ox || e->y != oy) {
        return;
    }
    if (sx) {
        sy = dy > 0 ? step : (dy < 0 ? -step : 0);
        sx = 0;
    } else {
        sx = dx > 0 ? step : (dx < 0 ? -step : 0);
        sy = 0;
    }
    try_move(&e->x, &e->y, sx, sy, hw, hw);
}

static void enemy_bullet(int x, int y, int tx, int ty, int speed, int dmg, int slow) {
    int vx, vy;
    vel_toward(x, y, tx, ty, speed, &vx, &vy);
    spawn_shot(x, y, vx, vy, slow ? 48 : 36, dmg, 1, slow ? SK_ORB : SK_LASER, slow);
    g_combat = 80;
}

static void think_enemy(Enemy *e, int index) {
    int dx = g_px - e->x;
    int dy = g_py - e->y;
    int d2 = dx * dx + dy * dy;
    int hw = enemy_half(e);
    (void)index;
    if (e->kt > 0) {
        try_move(&e->x, &e->y, e->kvx, e->kvy, hw, hw);
        e->kt--;
        return;
    }
    if (e->kind == EK_BOSS) {
        int phase = 0;
        if (e->cd > 0) {
            e->cd--;
            return;
        }
        if (e->hp * 3 < e->maxhp) {
            phase = 2;
        } else if (e->hp * 2 < e->maxhp) {
            phase = 1;
        }
        if (phase == 0 || g_deck == 0) {
            int d;
            for (d = 0; d < 8; d++) {
                spawn_shot(e->x, e->y, DX[d] * 3, DY[d] * 3, 34, 12, 1, SK_LASER, 0);
            }
        }
        if (phase >= 1) {
            enemy_bullet(e->x, e->y, g_px, g_py, 3, 12, 0);
            enemy_bullet(e->x - 12, e->y, g_px, g_py, 3, 12, 0);
        }
        if (phase == 2) {
            int n = 0;
            int i;
            int d0 = g_spiral & 7;
            for (i = 0; i < 6; i++) {
                int d = (d0 + i) & 7;
                spawn_shot(e->x, e->y, DX[d] * 2, DY[d] * 2, 40, 12, 1, SK_ORB, 1);
            }
            g_spiral++;
            for (i = 0; i < MAX_EN; i++) {
                if (g_enemy[i].on && g_enemy[i].kind == EK_DROID) {
                    n++;
                }
            }
            if (n < 2) {
                int sx, sy;
                if (pick_cell(&sx, &sy)) {
                    spawn_enemy(EK_DROID, sx, sy);
                }
            }
        }
        e->cd = phase == 2 ? 34 : 42;
        return;
    }
    if (e->kind == EK_DROID) {
        step_axis(e, dx, dy, 2);
        if (d2 < 15 * 15) {
            kill_enemy((int)(e - g_enemy), 0);
        }
        return;
    }
    if (e->kind == EK_GRUNT) {
        int flee = e->hp * 2 < e->maxhp;
        int mx = dx;
        int my = dy;
        if (flee || d2 < 38 * 38) {
            mx = -dx;
            my = -dy;
        } else if (d2 < 92 * 92) {
            mx = 0;
            my = 0;
        }
        step_axis(e, mx, my, 1);
        if (e->cd > 0) {
            e->cd--;
        } else if (!flee && d2 < 120 * 120 && d2 > 20 * 20 && los(e->x, e->y, g_px, g_py)) {
            enemy_bullet(e->x, e->y, g_px, g_py, 3, 10, 0);
            e->cd = 46;
        }
        return;
    }
    if (e->kind == EK_ELITE) {
        int px = -dy;
        int py = dx;
        if (e->strafe < 0) {
            px = -px;
            py = -py;
        }
        if (px == 0 && py == 0) {
            px = e->strafe;
        }
        {
            int ox = e->x;
            int oy = e->y;
            step_axis(e, px, py, 1);
            if (e->x == ox && e->y == oy) {
                e->strafe = -e->strafe;
            }
        }
        if ((g_tick % 28) == 0) {
            sfx(SFX_STEP);
        }
        if (e->cd > 0) {
            e->cd--;
        }
        if (e->burst > 0 && e->cd <= 0 && los(e->x, e->y, g_px, g_py)) {
            enemy_bullet(e->x, e->y, g_px, g_py, 4, 10, 0);
            e->burst--;
            e->cd = e->burst ? 6 : 50;
        } else if (e->burst <= 0 && e->cd <= 0 && d2 < 140 * 140 && los(e->x, e->y, g_px, g_py)) {
            e->burst = 3;
        }
        if (d2 < 12 * 12 && (g_tick & 7) == 0) {
            hurt_player(10);
        }
        return;
    }
    if ((g_tick & 1) == 0) {
        step_axis(e, dx, dy, 1);
    }
    if ((g_tick % 30) == 0) {
        sfx(SFX_STEP);
    }
    if (e->cd > 0) {
        e->cd--;
    } else {
        int base = 2;
        int a;
        if (iabs(dx) > iabs(dy) * 2) {
            base = dx > 0 ? 0 : 4;
        } else if (iabs(dy) > iabs(dx) * 2) {
            base = dy > 0 ? 2 : 6;
        } else if (dx >= 0 && dy >= 0) {
            base = 1;
        } else if (dx < 0 && dy >= 0) {
            base = 3;
        } else if (dx < 0) {
            base = 5;
        } else {
            base = 7;
        }
        for (a = -2; a <= 2; a++) {
            int d = (base + a) & 7;
            spawn_shot(e->x, e->y, DX[d] * 2, DY[d] * 2, 46, 14, 1, SK_ORB, 1);
        }
        e->cd = 56;
    }
    if (d2 < 14 * 14 && (g_tick & 7) == 0) {
        hurt_player(12);
    }
}

static void sim_shots(void) {
    int i, e;
    for (i = 0; i < MAX_SHOT; i++) {
        Shot *s = &g_shot[i];
        int hit = 0;
        if (!s->on) {
            continue;
        }
        s->x += s->vx;
        s->y += s->vy;
        s->life--;
        if (s->x < 0 || s->y < 0 || s->x >= 320 || s->y >= 224 || s->life <= 0) {
            if (s->kind == SK_NADE) {
                queue_boom(s->x, s->y, s->dmg);
            }
            s->on = 0;
            continue;
        }
        if (solid_at(s->x, s->y)) {
            if (s->kind == SK_NADE) {
                queue_boom(s->x, s->y, s->dmg);
            }
            s->on = 0;
            continue;
        }
        if (s->kind == SK_NADE) {
            for (e = 0; e < MAX_EN; e++) {
                int hw;
                if (!g_enemy[e].on) {
                    continue;
                }
                hw = enemy_half(&g_enemy[e]) + 4;
                if (iabs(g_enemy[e].x - s->x) <= hw && iabs(g_enemy[e].y - s->y) <= hw) {
                    queue_boom(s->x, s->y, s->dmg);
                    s->on = 0;
                    break;
                }
            }
            continue;
        }
        if (s->team == 0) {
            for (e = 0; e < MAX_EN; e++) {
                int hw;
                if (!g_enemy[e].on) {
                    continue;
                }
                hw = enemy_half(&g_enemy[e]);
                if (iabs(g_enemy[e].x - s->x) <= hw && iabs(g_enemy[e].y - s->y) <= hw) {
                    int close = s->kind == SK_SCAT;
                    damage_enemy(e, s->dmg, s->vx, s->vy, close, 0);
                    hit = 1;
                    break;
                }
            }
            if (!hit) {
                for (e = 0; e < MAX_BAR; e++) {
                    if (g_bar[e].on && iabs(g_bar[e].x - s->x) < 8 && iabs(g_bar[e].y - s->y) < 8) {
                        g_bar[e].hp -= s->dmg;
                        if (g_bar[e].hp <= 0) {
                            g_bar[e].on = 0;
                            queue_boom(g_bar[e].x, g_bar[e].y, 36);
                        }
                        hit = 1;
                        break;
                    }
                }
            }
        } else if (iabs(g_px - s->x) < 7 && iabs(g_py - s->y) < 8) {
            hurt_player(s->dmg);
            hit = 1;
        }
        if (hit) {
            s->on = 0;
        }
    }
}

static void sim_mines(void) {
    int i, e;
    for (i = 0; i < MAX_MINE; i++) {
        Mine *m = &g_mine[i];
        int trip = 0;
        if (!m->on) {
            continue;
        }
        if (iabs(g_px - m->x) < 18 && iabs(g_py - m->y) < 18) {
            trip = 1;
        }
        for (e = 0; e < MAX_EN; e++) {
            if (g_enemy[e].on && iabs(g_enemy[e].x - m->x) < 14 && iabs(g_enemy[e].y - m->y) < 14) {
                trip = 1;
            }
        }
        if (m->fuse < 0) {
            if (trip) {
                m->fuse = 18;
            }
        } else {
            m->fuse--;
            if ((m->fuse % 6) == 0) {
                sfx(SFX_MINE);
            }
            if (m->fuse <= 0) {
                m->on = 0;
                queue_boom(m->x, m->y, 30);
            }
        }
    }
}

static void sim_crush(void) {
    int i, e;
    for (i = 0; i < g_ncrush; i++) {
        Crush *c = &g_crush[i];
        if (!c->on) {
            continue;
        }
        c->x += c->dir * 2;
        if (c->x < c->minx || c->x > c->maxx) {
            c->dir = -c->dir;
            c->x += c->dir * 2;
        }
        if (iabs(g_px - c->x) < 12 && iabs(g_py - c->y) < 12) {
            hurt_player(34);
        }
        for (e = 0; e < MAX_EN; e++) {
            if (!g_enemy[e].on || g_enemy[e].kind == EK_BOSS) {
                continue;
            }
            if (iabs(g_enemy[e].x - c->x) < 12 && iabs(g_enemy[e].y - c->y) < 12) {
                damage_enemy(e, 40, c->dir * 4, 0, 0, 1);
            }
        }
    }
}

static void sim_world(const lippus_pad_t *pad) {
    int hx = pad_held(pad, LIPPUS_RIGHT) - pad_held(pad, LIPPUS_LEFT);
    int hy = pad_held(pad, LIPPUS_DOWN) - pad_held(pad, LIPPUS_UP);
    int step = 2;
    int i, any, d, door;
    g_firing = pad_held(pad, LIPPUS_A);
    if (g_weapon == W_MINI && g_firing && g_ammo_y > 0) {
        step = (g_tick & 2) ? 1 : 2;
    }
    if (hx && hy) {
        step = 1;
    }
    if ((hx || hy) && !pad_held(pad, LIPPUS_B)) {
        g_face = hx > 0 && hy > 0 ? 1 : hx < 0 && hy > 0 ? 3 : hx < 0 && hy < 0 ? 5 : hx > 0 && hy < 0 ? 7
                 : hx > 0                                                                          ? 0
                 : hx < 0                                                                          ? 4
                 : hy > 0                                                                          ? 2
                                                                                                   : 6;
    }
    g_moving = hx || hy;
    if (g_moving) {
        try_move(&g_px, &g_py, hx * step, hy * step, 5, 6);
    }
    door = stood_door();
    if (door >= 0) {
        int go = (door == 0 && hy < 0) || (door == 2 && hy > 0) || (door == 3 && hx < 0) ||
                 (door == 1 && hx > 0);
        int nx = g_rx + NX[door];
        int ny = g_ry + NY[door];
        if (go && inside(nx, ny) && g_map[ri(nx, ny)].on && g_map[ri(g_rx, g_ry)].door[door] == D_OPEN) {
            int first = !g_map[ri(nx, ny)].visited;
            enter_at(nx, ny, door, 1);
            if (!first) {
                sfx(SFX_DOOR);
            }
            g_tick++;
            return;
        }
    }
    if (g_edge & LIPPUS_X) {
        d = nearest_door();
        if (d >= 0) {
            use_door(d);
        } else {
            try_vent();
        }
    }
    if (g_edge & LIPPUS_CLICK) {
        do_melee();
    }
    if (g_edge & LIPPUS_Y) {
        do_grenade();
    }
    if (g_firing) {
        fire_weapon();
    } else if (g_weapon == W_PISTOL && g_lock <= 0 && g_heat > 0) {
        g_heat -= 2;
        if (g_heat < 0) {
            g_heat = 0;
        }
    }
    if (g_lock > 0) {
        g_lock--;
        if (g_lock == 0 && g_heat > 60) {
            g_heat = 60;
        }
    }
    if (g_cd > 0) {
        g_cd--;
    }
    if (g_melee_cd > 0) {
        g_melee_cd--;
    }
    if (g_iframes > 0) {
        g_iframes--;
    }
    if (g_combat > 0) {
        g_combat--;
    } else if (g_sp < 50 && (g_tick % 12) == 0) {
        g_sp++;
    }
    if (g_grid_off > 0) {
        g_grid_off--;
    }
    if (g_slash > 0) {
        g_slash--;
    }
    if (g_rail_t > 0) {
        g_rail_t--;
    }
    for (i = 0; i < MAX_EN; i++) {
        if (g_enemy[i].on) {
            think_enemy(&g_enemy[i], i);
        }
    }
    sim_shots();
    sim_mines();
    sim_crush();
    if ((g_tick & 7) == 0) {
        int hz = hazard_dmg(g_px, g_py);
        if (hz) {
            hurt_player(hz);
        }
        for (i = 0; i < MAX_EN; i++) {
            int ed;
            if (!g_enemy[i].on) {
                continue;
            }
            ed = hazard_dmg(g_enemy[i].x, g_enemy[i].y);
            if (ed) {
                damage_enemy(i, ed, g_px - g_enemy[i].x, g_py - g_enemy[i].y, 0, 0);
            }
        }
    }
    for (i = 0; i < MAX_PICK; i++) {
        if (g_pick[i].on && iabs(g_pick[i].x - g_px) < 12 && iabs(g_pick[i].y - g_py) < 12) {
            take_pick(i);
        }
    }
    run_booms();
    cap_ammo_fields();
    if (g_hp <= 0) {
        die();
        g_tick++;
        return;
    }
    if (g_boss_down) {
        g_boss_down = 0;
        g_map[ri(g_rx, g_ry)].cleared = 1;
        g_state = g_deck >= 2 ? ST_WIN : ST_DECK;
        g_wait = 28;
        g_tick++;
        return;
    }
    any = 0;
    for (i = 0; i < MAX_EN; i++) {
        if (g_enemy[i].on) {
            any = 1;
        }
    }
    if (!any) {
        g_map[ri(g_rx, g_ry)].cleared = 1;
    }
    for (i = 0; i < MAX_FX; i++) {
        if (g_fx[i].on && --g_fx[i].t <= 0) {
            g_fx[i].on = 0;
        }
    }
    if ((g_tick % 90) == 0) {
        for (i = 0; i < MAX_EN; i++) {
            if (g_enemy[i].on && g_enemy[i].kind == EK_DROID) {
                sfx(SFX_DROID);
                break;
            }
        }
    }
    g_tick++;
}

static int capture_save(int slot) {
    int i;
    Slot *s;
    if (slot < 0 || slot >= SAVE_SLOTS) {
        return 0;
    }
    s = &g_slot[slot];
    mem_zero(s, (int)sizeof(*s));
    s->deck = (uint8_t)g_deck;
    s->rx = (uint8_t)g_rx;
    s->ry = (uint8_t)g_ry;
    s->face = (uint8_t)g_face;
    s->weapon = (uint8_t)g_weapon;
    s->hp = (uint8_t)(g_hp > 255 ? 255 : g_hp);
    s->sp = (uint8_t)g_sp;
    s->grenades = (uint8_t)g_grenades;
    s->has_key = (uint8_t)g_has_key;
    s->over = (uint8_t)g_over;
    s->ammo_s = (uint8_t)(g_ammo_s > 255 ? 255 : g_ammo_s);
    s->ammo_y = (uint8_t)(g_ammo_y > 255 ? 255 : g_ammo_y);
    s->heat = (uint8_t)g_heat;
    s->key_used = (uint8_t)g_key_used;
    s->held = (uint8_t)g_held;
    s->px = (uint16_t)g_px;
    s->py = (uint16_t)g_py;
    s->seed = g_seed;
    for (i = 0; i < 64; i++) {
        s->doors[i] = (uint8_t)(g_map[i].door[0] | (g_map[i].door[1] << 2) |
                                (g_map[i].door[2] << 4) | (g_map[i].door[3] << 6));
        bit_set(s->visited, i, g_map[i].visited);
        bit_set(s->cleared, i, g_map[i].cleared);
        bit_set(s->looted, i, g_map[i].looted);
    }
    return write_slot(slot);
}

static void apply_save(int slot) {
    int i;
    Slot *s;
    if (slot < 0 || slot >= SAVE_SLOTS || !g_slot_ok[slot]) {
        return;
    }
    s = &g_slot[slot];
    gen_deck(s->seed, s->deck);
    for (i = 0; i < 64; i++) {
        int d;
        for (d = 0; d < 4; d++) {
            g_map[i].door[d] = (uint8_t)((s->doors[i] >> (d * 2)) & 3);
        }
        g_map[i].visited = (uint8_t)bit_get(s->visited, i);
        g_map[i].cleared = (uint8_t)bit_get(s->cleared, i);
        g_map[i].looted = (uint8_t)bit_get(s->looted, i);
    }
    g_rx = s->rx;
    g_ry = s->ry;
    if (!inside(g_rx, g_ry) || !g_map[ri(g_rx, g_ry)].on) {
        g_rx = g_start_x;
        g_ry = g_start_y;
    }
    g_face = s->face & 7;
    g_weapon = s->weapon & 3;
    g_hp = s->hp;
    g_sp = s->sp;
    g_grenades = s->grenades;
    g_has_key = s->has_key;
    g_over = s->over;
    g_ammo_s = s->ammo_s;
    g_ammo_y = s->ammo_y;
    g_heat = s->heat;
    g_key_used = s->key_used;
    g_held = s->held | 1;
    g_lock = 0;
    g_cd = 0;
    g_iframes = 0;
    g_slow = 0;
    g_boss_down = 0;
    g_edge = 0;
    build_geometry();
    g_px = s->px;
    g_py = s->py;
    if (box_hit(g_px, g_py, 5, 6)) {
        g_px = 160;
        g_py = 112;
    }
    spawn_population();
    if (g_hp > hp_max()) {
        g_hp = hp_max();
    }
    if (g_hp < 1) {
        g_hp = 1;
    }
    g_state = ST_PLAY;
    banner("LOADED", 40);
}

static void open_slots(int mode, int back) {
    int i;
    g_slot_mode = mode;
    g_slot_back = back;
    g_confirm = 0;
    g_menu = 0;
    if (mode == 0) {
        for (i = 0; i < SAVE_SLOTS; i++) {
            if (!g_slot_ok[i]) {
                g_menu = i;
                break;
            }
        }
    } else {
        for (i = 0; i < SAVE_SLOTS; i++) {
            if (g_slot_ok[i]) {
                g_menu = i;
                break;
            }
        }
    }
    g_state = ST_SLOT;
}

static void begin_run(void) {
    g_salt++;
    g_seed = sys_ticks() ^ (g_salt * 0x9E3779B9u);
    if (!g_seed) {
        g_seed = 1;
    }
    g_deck = 0;
    g_hp = hp_max();
    g_sp = 50;
    g_heat = 0;
    g_lock = 0;
    g_over = 0;
    g_weapon = W_PISTOL;
    g_held = 1;
    g_ammo_s = 0;
    g_ammo_y = 0;
    g_grenades = 1;
    g_has_key = 0;
    g_key_used = 0;
    g_face = 2;
    g_cd = 0;
    g_melee_cd = 0;
    g_iframes = 0;
    g_combat = 0;
    g_slow = 0;
    g_boss_down = 0;
    g_edge = 0;
    gen_deck(g_seed, 0);
    enter_at(g_start_x, g_start_y, -1, 0);
    g_state = ST_PLAY;
    banner(DECK_NAME[0], 70);
}

static void next_deck(void) {
    g_deck++;
    if (g_deck > 2) {
        g_state = ST_WIN;
        return;
    }
    g_has_key = 0;
    g_key_used = 0;
    g_hp += 40;
    if (g_hp > hp_max()) {
        g_hp = hp_max();
    }
    g_sp = 50;
    g_heat = 0;
    g_lock = 0;
    g_salt++;
    g_seed = sys_ticks() ^ (g_salt * 0x9E3779B9u);
    if (!g_seed) {
        g_seed = 1;
    }
    gen_deck(g_seed, g_deck);
    enter_at(g_start_x, g_start_y, -1, 0);
    g_state = ST_PLAY;
    banner(DECK_NAME[g_deck], 80);
}

static void draw_flames(void) {
    int i, k;
    for (i = 0; i < g_nvent; i++) {
        int blocked = 0;
        if (!g_vent[i].on || ((g_frame / 22 + g_vent[i].tx) % 3) != 2) {
            continue;
        }
        for (k = 1; k <= 2; k++) {
            int cx = g_vent[i].tx + NX[g_vent[i].dir] * k;
            int cy = g_vent[i].ty + NY[g_vent[i].dir] * k;
            int col = k == 1 ? COL_SPARK_YELLOW : COL_PLASMA_ORANGE;
            if ((unsigned)cx >= COLS || (unsigned)cy >= ROWS) {
                break;
            }
            if (solid_tid(g_tiles[cy][cx])) {
                blocked = 1;
            }
            if (blocked) {
                break;
            }
            gfx_fill_rect(cx * 16 + 4 + g_sox, cy * 16 + 3 + g_soy, 8, 10, C(col));
        }
    }
}

static void draw_player(void) {
    const uint8_t *sp;
    int flip = 0;
    int f = g_moving ? ((g_tick / 6) & 1) : 0;
    int ox, oy;
    if (g_iframes && (g_frame & 2)) {
        return;
    }
    if (g_face == 6) {
        sp = f ? &SPR_PLAYER_N_1[0][0] : &SPR_PLAYER_N_0[0][0];
    } else if (g_face == 2) {
        sp = f ? &SPR_PLAYER_S_1[0][0] : &SPR_PLAYER_S_0[0][0];
    } else if (g_face == 1 || g_face == 3) {
        sp = f ? &SPR_PLAYER_SE_1[0][0] : &SPR_PLAYER_SE_0[0][0];
        flip = g_face == 3;
    } else if (g_face == 7 || g_face == 5) {
        sp = f ? &SPR_PLAYER_NE_1[0][0] : &SPR_PLAYER_NE_0[0][0];
        flip = g_face == 5;
    } else {
        sp = f ? &SPR_PLAYER_E_1[0][0] : &SPR_PLAYER_E_0[0][0];
        flip = g_face == 4;
    }
    blit(g_px - 8, g_py - 8, sp, 16, 16, flip, 0);
    if (g_slash > 0) {
        const uint8_t *sl = &SPR_THERMAL_SLASH_E[0][0];
        int fx = 0;
        int fy = 0;
        if (g_face == 2) {
            sl = &SPR_THERMAL_SLASH_S[0][0];
        } else if (g_face == 6) {
            sl = &SPR_THERMAL_SLASH_S[0][0];
            fy = 1;
        } else if (g_face == 4 || g_face == 3 || g_face == 5) {
            fx = 1;
        }
        ox = DX[g_face] * 12;
        oy = DY[g_face] * 12;
        blit(g_px - 8 + ox, g_py - 8 + oy, sl, 16, 16, fx, fy);
    }
}

static void draw_enemy(const Enemy *e) {
    const uint8_t *sp;
    int flipx = 0;
    int flipy = 0;
    int dx = g_px - e->x;
    int dy = g_py - e->y;
    int east = iabs(dx) > iabs(dy);
    if (e->kind == EK_BOSS) {
        blit(e->x - 16, e->y - 16, &SPR_BOSS_CORE_32[0][0], 32, 32, 0, 0);
        return;
    }
    if (e->kind == EK_BRUTE) {
        blit(e->x - 12, e->y - 14, &SPR_ENEMY_BRUTE_24[0][0], 24, 24, dx < 0, 0);
        return;
    }
    if (e->kind == EK_DROID) {
        sp = ((g_tick / 6) & 1) ? &SPR_ENEMY_DROID_1[0][0] : &SPR_ENEMY_DROID_0[0][0];
        blit(e->x - 8, e->y - 8, sp, 16, 16, 0, 0);
        return;
    }
    if (e->kind == EK_ELITE) {
        if (east) {
            sp = &SPR_ENEMY_ELITE_E_0[0][0];
            flipx = dx < 0;
        } else {
            sp = &SPR_ENEMY_ELITE_S_0[0][0];
            flipy = dy < 0;
        }
    } else if (east) {
        sp = &SPR_ENEMY_GRUNT_E_0[0][0];
        flipx = dx < 0;
    } else {
        sp = ((g_tick / 7) & 1) ? &SPR_ENEMY_GRUNT_S_1[0][0] : &SPR_ENEMY_GRUNT_S_0[0][0];
        flipy = dy < 0;
    }
    blit(e->x - 8, e->y - 8, sp, 16, 16, flipx, flipy);
    if (e->kind == EK_ELITE && e->shield) {
        gfx_fill_rect(e->x - 6 + g_sox, e->y - 10 + g_soy, 12, 2, C(COL_ION_CYAN));
    }
}

static const uint8_t *pick_sprite(int kind) {
    if (kind == PK_MED) {
        return &SPR_DROP_MEDKIT[0][0];
    }
    if (kind == PK_SH) {
        return &SPR_DROP_SHIELD_CELL[0][0];
    }
    if (kind == PK_FUS) {
        return &SPR_DROP_FUSION_CELL[0][0];
    }
    if (kind == PK_YEL) {
        return &SPR_DROP_YELLOW_BATTERY[0][0];
    }
    if (kind == PK_RED) {
        return &SPR_DROP_RED_CORE[0][0];
    }
    if (kind == PK_KEY) {
        return &SPR_DROP_KEYCARD[0][0];
    }
    if (kind == PK_DATA) {
        return &SPR_DROP_TACTICAL_DATA[0][0];
    }
    if (kind == PK_WSC) {
        return &ICON_WEP_SCATTERGUN[0][0];
    }
    if (kind == PK_WRA) {
        return &ICON_WEP_RAILGUN[0][0];
    }
    return &ICON_WEP_MINIGUN[0][0];
}

static const uint8_t *shot_sprite(int kind) {
    if (kind == SK_OC || kind == SK_SCAT) {
        return &SPR_PROJ_PULSE_OVERCHARGE[0][0];
    }
    if (kind == SK_LASER) {
        return &SPR_PROJ_ENEMY_LASER[0][0];
    }
    if (kind == SK_ORB) {
        return &SPR_PROJ_BRUTE_ORB[0][0];
    }
    if (kind == SK_NADE) {
        return &SPR_GRENADE_PLASMA_8[0][0];
    }
    return &SPR_PROJ_PULSE_PLAYER[0][0];
}

static void draw_map(int x, int y, int scale) {
    int ry, rx;
    if (y >= 16) {
        gfx_fill_rect(x - 1, y - 1, MAP * scale + 2, MAP * scale + 2, C(COL_HULL_DARK));
    } else {
        gfx_fill_rect(x, y, MAP * scale, MAP * scale, C(COL_HULL_DARK));
    }
    for (ry = 0; ry < MAP; ry++) {
        for (rx = 0; rx < MAP; rx++) {
            int id = ri(rx, ry);
            int col = -1;
            int known, near, d;
            if (!g_map[id].on) {
                continue;
            }
            known = g_map[id].visited;
            near = 0;
            for (d = 0; d < 4; d++) {
                int nx = rx + NX[d];
                int ny = ry + NY[d];
                if (g_map[id].door[d] && inside(nx, ny) && g_map[ri(nx, ny)].visited) {
                    near = 1;
                }
            }
            if (rx == g_rx && ry == g_ry && g_state != ST_TITLE && g_state != ST_UP) {
                col = COL_ICE_GLOW;
            } else if (g_map[id].kind == K_BOSS && !g_map[id].cleared) {
                col = COL_LASER_RED;
            } else if (known) {
                col = COL_STEEL_MID;
            } else if (near) {
                col = COL_SHIELD_BLUE;
            }
            if (col >= 0) {
                gfx_fill_rect(x + rx * scale, y + ry * scale, scale, scale, C(col));
            }
        }
    }
}

static void draw_world(void) {
    int ty, tx, i;
    g_sox = 0;
    g_soy = 0;
    if (g_shake > 0) {
        g_sox = (g_frame & 2) ? 2 : -2;
        g_soy = (g_frame & 1) ? 1 : -1;
        g_shake--;
    }
    gfx_clear(C(COL_VOID_BLACK));
    gfx_clip(0, 0, 320, 224);
    for (ty = 0; ty < ROWS; ty++) {
        for (tx = 0; tx < COLS; tx++) {
            int tid = g_tiles[ty][tx];
            if (tid == TID_HAZARD_GRID_OFF) {
                int p = grid_phase(tx, ty);
                int on = p == 3 || (p == 2 && (g_frame & 4));
                tid = on ? TID_HAZARD_GRID_ON : TID_HAZARD_GRID_OFF;
            }
            tile_at(tx * 16, ty * 16, tid);
        }
    }
    draw_flames();
    for (i = 0; i < MAX_MINE; i++) {
        if (!g_mine[i].on) {
            continue;
        }
        if (g_mine[i].fuse >= 0 && (g_frame & 2)) {
            continue;
        }
        blit(g_mine[i].x - 8, g_mine[i].y - 8, &SPR_HAZARD_MINE[0][0], 16, 16, 0, 0);
    }
    for (i = 0; i < MAX_BAR; i++) {
        if (g_bar[i].on) {
            blit(g_bar[i].x - 8, g_bar[i].y - 8, &SPR_BARREL_PLASMA[0][0], 16, 16, 0, 0);
        }
    }
    for (i = 0; i < g_ncrush; i++) {
        if (g_crush[i].on) {
            blit(g_crush[i].x - 8, g_crush[i].y - 8, &SPR_HAZARD_CRUSHER[0][0], 16, 16, 0, 0);
        }
    }
    for (i = 0; i < MAX_PICK; i++) {
        if (g_pick[i].on) {
            blit(g_pick[i].x - 8, g_pick[i].y - 8, pick_sprite(g_pick[i].kind), 16, 16, 0, 0);
        }
    }
    for (i = 0; i < MAX_EN; i++) {
        if (g_enemy[i].on) {
            draw_enemy(&g_enemy[i]);
        }
    }
    if (g_state != ST_TITLE) {
        draw_player();
    }
    for (i = 0; i < MAX_SHOT; i++) {
        if (!g_shot[i].on) {
            continue;
        }
        blit(g_shot[i].x - 4, g_shot[i].y - 4, shot_sprite(g_shot[i].kind), 8, 8, 0, 0);
    }
    g_dot = find_dot();
    if (g_dot >= 0) {
        blit(g_enemy[g_dot].x - 8, g_enemy[g_dot].y - 8, &SPR_REDDOT_RETICLE[0][0], 16, 16, 0, 0);
    }
    if (g_rail_t > 0) {
        gfx_line(g_px + g_sox, g_py + g_soy, g_rail_x + g_sox, g_rail_y + g_soy, C(COL_ION_CYAN));
    }
    for (i = 0; i < MAX_FX; i++) {
        const uint8_t *sp;
        if (!g_fx[i].on) {
            continue;
        }
        sp = g_fx[i].t > 8 ? &SPR_EXPLOSION_0[0][0]
                           : (g_fx[i].t > 4 ? &SPR_EXPLOSION_1[0][0] : &SPR_EXPLOSION_2[0][0]);
        blit(g_fx[i].x - 8, g_fx[i].y - 8, sp, 16, 16, 0, 0);
    }
    gfx_clip(0, 0, 0, 0);
    g_sox = 0;
    g_soy = 0;
}

static void draw_hud(void) {
    int hp_w, sh_w, i;
    char num[8];
    gfx_fill_rect(0, 224, 320, 16, C(COL_VOID_BLACK));
    gfx_fill_rect(0, 224, 320, 1, C(COL_STEEL_SHADOW));
    blit(2, 225, &ICON_HUD_HP_8[0][0], 8, 8, 0, 0);
    gfx_fill_rect(12, 227, 90, 4, C(COL_HULL_DARK));
    hp_w = hp_max() > 0 ? g_hp * 88 / hp_max() : 0;
    if (hp_w > 0) {
        gfx_fill_rect(13, 228, hp_w, 2, C(g_hp * 4 < hp_max() ? COL_CORAL_GLOW : COL_LASER_RED));
    }
    blit(2, 232, &ICON_HUD_SHIELD_8[0][0], 8, 8, 0, 0);
    gfx_fill_rect(12, 234, 90, 4, C(COL_HULL_DARK));
    sh_w = g_sp * 88 / 50;
    if (sh_w > 0) {
        gfx_fill_rect(13, 235, sh_w, 2, C(COL_ION_CYAN));
    }
    if (g_weapon == W_PISTOL) {
        int heat_w = g_heat * 78 / 100;
        int hx0 = 122 + 55 * 78 / 100;
        int hx1 = 122 + 75 * 78 / 100;
        int col = g_lock ? ((g_frame & 4) ? COL_LASER_RED : COL_BATTERY_GOLD)
                         : (g_heat > 75 ? COL_PLASMA_ORANGE : COL_MOSS_TECH);
        gfx_fill_rect(120, 230, 82, 8, C(COL_HULL_DARK));
        if (heat_w > 0) {
            gfx_fill_rect(122, 232, heat_w, 4, C(col));
        }
        if (hx1 > hx0) {
            gfx_fill_rect(hx0, 228, hx1 - hx0, 1, C(COL_ICE_GLOW));
            gfx_fill_rect(hx0, 238, hx1 - hx0, 1, C(COL_VENOM_GREEN));
        }
        for (i = 0; i < g_over && i < 3; i++) {
            gfx_fill_rect(206 + i * 4, 232, 3, 4, C(COL_NEON_LIME));
        }
    } else {
        const uint8_t *ic = g_weapon == W_SCATTER ? &ICON_WEP_SCATTERGUN[0][0]
                            : g_weapon == W_RAIL  ? &ICON_WEP_RAILGUN[0][0]
                                                  : &ICON_WEP_MINIGUN[0][0];
        blit(116, 224, ic, 16, 16, 0, 0);
        fmt_u(num, (unsigned)(g_weapon == W_SCATTER ? g_ammo_s : g_ammo_y));
        text(134, 230, num, COL_BATTERY_GOLD, 1);
    }
    for (i = 0; i < 4; i++) {
        if (i < g_grenades) {
            blit(214 + i * 10, 230, &ICON_HUD_GRENADE_8[0][0], 8, 8, 0, 0);
        } else {
            gfx_fill_rect(216 + i * 10, 232, 4, 4, C(COL_HULL_DARK));
        }
    }
    if (g_has_key) {
        blit(258, 230, &ICON_HUD_KEYCARD_8[0][0], 8, 8, 0, 0);
    }
    draw_map(300, 224, 2);
}

static void draw_banner(void) {
    if (g_banner_t > 0 && g_banner[0]) {
        text(-1, 8, g_banner, COL_CHROME_WHITE, 1);
        g_banner_t--;
    }
}

static void panel(int x, int y, int w, int h) {
    gfx_fill_rect(x, y, w, h, C(COL_VOID_BLACK));
    gfx_rect(x, y, w, h, C(COL_TITANIUM_LIGHT));
}

static void title_bg(void) {
    int ty, tx;
    gfx_clear(C(COL_VOID_BLACK));
    g_sox = 0;
    g_soy = 0;
    for (ty = 0; ty < 14; ty++) {
        for (tx = 0; tx < 20; tx++) {
            int tid = (ty == 0 || ty == 13 || tx == 0 || tx == 19) ? TID_WALL_HULL : TID_FLOOR_PLATE;
            if (ty == 4 && (tx == 3 || tx == 16)) {
                tid = TID_CRATE_COVER;
            }
            tile_at(tx * 16, ty * 16, tid);
        }
    }
    blit(12, 168, &SPR_PLAYER_E_0[0][0], 16, 16, 0, 0);
    blit(292, 168, &SPR_ENEMY_GRUNT_E_0[0][0], 16, 16, 1, 0);
}

static void draw_vol_hint(void) {
    char line[20];
    int i;
    int n = 0;
    const char *p = "Y+  B-  VOL ";
    while (*p && n < 14) {
        line[n++] = *p++;
    }
    for (i = 0; i < BGM_LVL_MAX; i++) {
        line[n++] = (char)(i < g_bgm_lvl ? '#' : '-');
    }
    line[n] = 0;
    text(-1, 162, line, g_bgm_lvl ? COL_BATTERY_GOLD : COL_STEEL_MID, 1);
}

static void title_foot(void) {
    char num[8];
    text(-1, 176, "PRESS START", COL_NEON_LIME, 1);
    text(-1, 188, "SELECT UPGRADES", COL_BATTERY_GOLD, 1);
    fmt_u(num, (unsigned)g_data);
    text(40, 206, "DATA", COL_TITANIUM_LIGHT, 1);
    text(80, 206, num, COL_NEON_LIME, 1);
    if (any_save()) {
        text(168, 206, "X LOAD", COL_ION_CYAN, 1);
    }
}

static void loot_row(int y, const uint8_t *spr, const char *name, const char *note, int col) {
    int bob = (g_frame / 10) & 1;
    blit(48, y + bob, spr, 16, 16, 0, 0);
    text(72, y, name, col, 1);
    text(72, y + 8, note, COL_TITANIUM_LIGHT, 1);
}

static void draw_title(void) {
    char num[8];
    title_bg();
    panel(28, 8, 264, 224);
    if (g_page == 1) {
        text(-1, 16, "GUNS ON THE FLOOR", COL_ION_CYAN, 1);
        text(-1, 28, "KEYCARD ROOM, ONE PER DECK", COL_TITANIUM_LIGHT, 1);
        loot_row(46, &ICON_WEP_SCATTERGUN[0][0], "SCATTER", "HANGAR", COL_PLASMA_ORANGE);
        loot_row(70, &ICON_WEP_RAILGUN[0][0], "RAIL", "REACTOR", COL_ION_CYAN);
        loot_row(94, &ICON_WEP_MINIGUN[0][0], "MINIGUN", "BRIDGE", COL_BATTERY_GOLD);
        loot_row(126, &ICON_WEP_PISTOL[0][0], "PISTOL", "ALREADY WITH YOU", COL_CHROME_WHITE);
        title_foot();
    } else if (g_page == 2) {
        text(-1, 16, "MEDKIT", COL_NEON_LIME, 1);
        loot_row(30, &SPR_DROP_MEDKIT[0][0], "MEDKIT", "RESTORES HP", COL_NEON_LIME);
        loot_row(54, &SPR_DROP_SHIELD_CELL[0][0], "SHIELD CELL", "RESTORES SHIELD", COL_ION_CYAN);
        text(-1, 76, "AMMO", COL_BATTERY_GOLD, 1);
        loot_row(90, &SPR_DROP_FUSION_CELL[0][0], "FUSION CELL", "SCATTER", COL_PLASMA_ORANGE);
        loot_row(112, &SPR_DROP_YELLOW_BATTERY[0][0], "YELLOW CELL", "RAIL AND MINIGUN", COL_BATTERY_GOLD);
        loot_row(134, &SPR_DROP_RED_CORE[0][0], "RED CORE", "GRENADES", COL_LASER_RED);
        title_foot();
    } else {
        text(-1, 20, "INCURSORE", COL_ION_CYAN, 2);
        text(-1, 56, "VS HEGEMONY", COL_PLASMA_VIOLET, 1);
        if (g_page == 3) {
            text(-1, 80, "STICK MOVE AND AIM", COL_CHROME_WHITE, 1);
            text(-1, 96, "A FIRE    B STRAFE", COL_CHROME_WHITE, 1);
            text(-1, 112, "X DOOR OR VENT", COL_CHROME_WHITE, 1);
            text(-1, 128, "Y GRENADE", COL_CHROME_WHITE, 1);
            text(-1, 144, "CLICK BLADE", COL_CHROME_WHITE, 1);
        } else if (g_page == 4) {
            text(-1, 80, "SELECT SWAPS GUNS", COL_CHROME_WHITE, 1);
            text(-1, 96, "HOLD SELECT FOR GEAR", COL_CHROME_WHITE, 1);
            text(-1, 120, "DEATH RESETS THE RUN", COL_LASER_RED, 1);
            text(-1, 136, "DATA STAYS WITH YOU", COL_NEON_LIME, 1);
        } else {
            text(-1, 84, "STICK FLIPS THESE SLIDES", COL_CHROME_WHITE, 1);
            text(-1, 108, "GUNS, MEDKIT, AMMO", COL_BATTERY_GOLD, 1);
            fmt_u(num, (unsigned)g_data);
            text(96, 132, "DATA", COL_TITANIUM_LIGHT, 1);
            text(136, 132, num, COL_NEON_LIME, 1);
        }
        text(-1, 176, "PRESS START", COL_NEON_LIME, 1);
        text(-1, 192, "SELECT UPGRADES", COL_BATTERY_GOLD, 1);
        if (any_save()) {
            text(-1, 208, "X LOAD", COL_ION_CYAN, 1);
        }
    }
    draw_vol_hint();
}

static void draw_upgrade(void) {
    char num[8];
    int i;
    static const char *const name[3] = {"HULL +10 HP", "COOLANT", "RED DOT"};
    int rank[3];
    int maxr[3];
    rank[0] = g_hp_rank;
    rank[1] = g_heat_rank;
    rank[2] = g_dot_rank;
    maxr[0] = 3;
    maxr[1] = 2;
    maxr[2] = 2;
    draw_title();
    panel(28, 16, 264, 200);
    text(-1, 26, "TACTICAL DATA", COL_ION_CYAN, 1);
    fmt_u(num, (unsigned)g_data);
    text(-1, 40, num, COL_NEON_LIME, 1);
    for (i = 0; i < 3; i++) {
        char row[20];
        int n = 0;
        const char *s = name[i];
        int y = 64 + i * 28;
        while (s[n] && n < 16) {
            row[n] = s[n];
            n++;
        }
        row[n] = 0;
        text(48, y, i == g_menu ? ">" : " ", COL_SPARK_YELLOW, 1);
        text(64, y, row, i == g_menu ? COL_CHROME_WHITE : COL_TITANIUM_LIGHT, 1);
        fmt_u(num, (unsigned)rank[i]);
        text(210, y, num, COL_BATTERY_GOLD, 1);
        text(226, y, "/", COL_STEEL_MID, 1);
        fmt_u(num, (unsigned)maxr[i]);
        text(240, y, num, COL_STEEL_MID, 1);
    }
    text(-1, 160, "COST 3    A BUY", COL_VENOM_GREEN, 1);
    text(-1, 178, "B BACK", COL_TITANIUM_LIGHT, 1);
    text(-1, 196, "GUNS STAY IN THE POOL", COL_STEEL_MID, 1);
}

static void draw_pause(void) {
    int i;
    draw_world();
    draw_hud();
    panel(80, 48, 160, 128);
    text(-1, 58, "SYSTEM", COL_ION_CYAN, 1);
    for (i = 0; i < 4; i++) {
        text(112, 78 + i * 16, i == g_menu ? ">" : " ", COL_SPARK_YELLOW, 1);
        text(128, 78 + i * 16, PAUSE_ITEM[i], i == g_menu ? COL_CHROME_WHITE : COL_TITANIUM_LIGHT, 1);
    }
}

static void draw_slots(void) {
    int i;
    if (g_slot_back == ST_PAUSE) {
        draw_world();
        draw_hud();
    } else {
        title_bg();
    }
    panel(16, 8, 288, 208);
    text(-1, 16, g_slot_mode ? "LOAD SLOT" : "SAVE SLOT", COL_ION_CYAN, 1);
    for (i = 0; i < SAVE_SLOTS; i++) {
        char num[8];
        int y = 36 + i * 18;
        int on = i == g_menu;
        int col = on ? COL_CHROME_WHITE : COL_TITANIUM_LIGHT;
        text(28, y, on ? ">" : " ", COL_SPARK_YELLOW, 1);
        num[0] = (char)('1' + i);
        num[1] = 0;
        text(44, y, num, col, 1);
        if (!g_slot_ok[i]) {
            text(62, y, "EMPTY", on ? COL_STEEL_MID : COL_STEEL_SHADOW, 1);
            continue;
        }
        text(62, y, DECK_NAME[g_slot[i].deck > 2 ? 2 : g_slot[i].deck], col, 1);
        fmt_u(num, g_slot[i].rx);
        text(148, y, num, col, 1);
        text(166, y, ",", COL_STEEL_MID, 1);
        fmt_u(num, g_slot[i].ry);
        text(174, y, num, col, 1);
        text(206, y, "HP", COL_STEEL_MID, 1);
        fmt_u(num, g_slot[i].hp);
        text(228, y, num, COL_NEON_LIME, 1);
    }
    if (g_confirm) {
        text(-1, 188, "OVERWRITE?  A YES  B NO", COL_LASER_RED, 1);
    } else if (g_slot_mode) {
        text(-1, 188, "A LOAD    B BACK", COL_TITANIUM_LIGHT, 1);
    } else {
        text(-1, 188, "A SAVE    B BACK", COL_TITANIUM_LIGHT, 1);
    }
}

static void draw_inv(void) {
    int i;
    char num[8];
    draw_world();
    draw_hud();
    panel(16, 16, 200, 190);
    text(28, 24, "GEAR", COL_ION_CYAN, 1);
    for (i = 0; i < 4; i++) {
        int have = (g_held & (1 << i)) != 0;
        text(28, 42 + i * 16, WEP_NAME[i], have ? COL_CHROME_WHITE : COL_STEEL_SHADOW, 1);
        if (i == W_PISTOL) {
            text(120, 42, "INF", COL_VENOM_GREEN, 1);
        } else if (have) {
            fmt_u(num, (unsigned)(i == W_SCATTER ? g_ammo_s : g_ammo_y));
            text(120, 42 + i * 16, num, COL_BATTERY_GOLD, 1);
        }
    }
    text(28, 112, "GRENADES", COL_CHROME_WHITE, 1);
    fmt_u(num, (unsigned)g_grenades);
    text(120, 112, num, COL_PLASMA_VIOLET, 1);
    text(28, 128, "KEYCARD", COL_CHROME_WHITE, 1);
    text(120, 128, g_has_key ? "YES" : "NO", g_has_key ? COL_LASER_RED : COL_STEEL_MID, 1);
    text(28, 144, "DATA", COL_CHROME_WHITE, 1);
    fmt_u(num, (unsigned)g_data);
    text(120, 144, num, COL_NEON_LIME, 1);
    text(28, 168, "B CLOSE", COL_TITANIUM_LIGHT, 1);
    draw_map(230, 36, 8);
    text(230, 110, DECK_NAME[g_deck > 2 ? 2 : g_deck], COL_ION_CYAN, 1);
}

static void draw_end(const char *a, const char *b, const char *c) {
    draw_world();
    draw_hud();
    panel(50, 70, 220, 90);
    text(-1, 84, a, COL_LASER_RED, 1);
    text(-1, 104, b, COL_CHROME_WHITE, 1);
    text(-1, 124, c, COL_TITANIUM_LIGHT, 1);
}

static void buy_upgrade(void) {
    int *rank = g_menu == 0 ? &g_hp_rank : (g_menu == 1 ? &g_heat_rank : &g_dot_rank);
    int maxr = g_menu == 0 ? 3 : 2;
    if (*rank >= maxr || g_data < 3) {
        sfx(SFX_LOCK);
        return;
    }
    g_data -= 3;
    (*rank)++;
    flush_disk();
    sfx(SFX_VENT);
}

static void tick_title(const lippus_pad_t *pad) {
    if ((g_frame % 180) == 0) {
        g_page = (g_page + 1) % TITLE_PAGES;
    }
    if (pad_pressed(pad, LIPPUS_LEFT)) {
        g_page = (g_page + TITLE_PAGES - 1) % TITLE_PAGES;
    }
    if (pad_pressed(pad, LIPPUS_RIGHT)) {
        g_page = (g_page + 1) % TITLE_PAGES;
    }
    if (pad_pressed(pad, LIPPUS_Y)) {
        bgm_nudge(1);
    }
    if (pad_pressed(pad, LIPPUS_B)) {
        bgm_nudge(-1);
    }
    if (pad_pressed(pad, LIPPUS_START) || pad_pressed(pad, LIPPUS_A)) {
        begin_run();
        return;
    }
    if (pad_pressed(pad, LIPPUS_SELECT)) {
        g_state = ST_UP;
        g_menu = 0;
        return;
    }
    if (pad_pressed(pad, LIPPUS_X) && any_save()) {
        open_slots(1, ST_TITLE);
        draw_slots();
        return;
    }
    draw_title();
    draw_banner();
}

static void tick_up(const lippus_pad_t *pad) {
    if (pad_pressed(pad, LIPPUS_UP)) {
        g_menu = (g_menu + 2) % 3;
    }
    if (pad_pressed(pad, LIPPUS_DOWN)) {
        g_menu = (g_menu + 1) % 3;
    }
    if (pad_pressed(pad, LIPPUS_A)) {
        buy_upgrade();
    }
    if (pad_pressed(pad, LIPPUS_B) || pad_pressed(pad, LIPPUS_START)) {
        g_state = ST_TITLE;
    }
    draw_upgrade();
}

static void tick_play(const lippus_pad_t *pad) {
    int sim = 1;
    g_edge |= pad->pressed;
    if (pad_pressed(pad, LIPPUS_SELECT)) {
        g_sel_track = 1;
        g_sel_hold = 0;
    }
    if (g_sel_track && pad_held(pad, LIPPUS_SELECT)) {
        g_sel_hold += FRAME;
    }
    if (g_sel_track && g_sel_hold >= 300) {
        g_state = ST_INV;
        g_sel_track = 0;
    } else if (g_sel_track && pad_released(pad, LIPPUS_SELECT)) {
        if (g_sel_hold < 300) {
            cycle_weapon();
        }
        g_sel_track = 0;
    }
    if (g_edge & LIPPUS_START) {
        g_edge = (uint16_t)(g_edge & (uint16_t)~LIPPUS_START);
        g_state = ST_PAUSE;
        g_menu = 0;
    }
    if (g_slow > 0) {
        sim = (g_frame % 3) == 0;
        g_slow--;
        if (g_slow == 0) {
            sfx(SFX_SLOW_OUT);
        }
    }
    if (g_state == ST_PLAY && sim) {
        sim_world(pad);
        g_edge = 0;
    }
    if (g_state == ST_INV) {
        return;
    }
    draw_world();
    draw_hud();
    draw_banner();
}

static void tick_pause(const lippus_pad_t *pad) {
    if (pad_pressed(pad, LIPPUS_UP)) {
        g_menu = (g_menu + 3) & 3;
    }
    if (pad_pressed(pad, LIPPUS_DOWN)) {
        g_menu = (g_menu + 1) & 3;
    }
    if (pad_pressed(pad, LIPPUS_B) || (pad_pressed(pad, LIPPUS_START) && g_menu == 0)) {
        g_state = ST_PLAY;
    }
    if (pad_pressed(pad, LIPPUS_A) || (pad_pressed(pad, LIPPUS_START) && g_menu != 0)) {
        if (g_menu == 0) {
            g_state = ST_PLAY;
        } else if (g_menu == 1) {
            open_slots(0, ST_PAUSE);
        } else if (g_menu == 2) {
            open_slots(1, ST_PAUSE);
        } else {
            g_state = ST_TITLE;
        }
    }
    if (g_state == ST_PAUSE) {
        draw_pause();
        draw_banner();
    } else if (g_state == ST_SLOT) {
        draw_slots();
        draw_banner();
    }
}

static void leave_slots(void) {
    g_state = g_slot_back;
    g_confirm = 0;
    if (g_state == ST_PAUSE) {
        g_menu = g_slot_mode ? 2 : 1;
        draw_pause();
        draw_banner();
    } else {
        draw_title();
    }
}

static void tick_slot(const lippus_pad_t *pad) {
    if (g_confirm) {
        if (pad_pressed(pad, LIPPUS_A) || pad_pressed(pad, LIPPUS_START)) {
            if (capture_save(g_menu)) {
                banner("SAVED", 40);
                sfx(SFX_PICK);
            } else {
                banner("FAILED", 40);
                sfx(SFX_LOCK);
            }
            g_confirm = 0;
        } else if (pad_pressed(pad, LIPPUS_B)) {
            g_confirm = 0;
        }
        draw_slots();
        draw_banner();
        return;
    }
    if (pad_pressed(pad, LIPPUS_UP)) {
        g_menu = (g_menu + SAVE_SLOTS - 1) % SAVE_SLOTS;
    }
    if (pad_pressed(pad, LIPPUS_DOWN)) {
        g_menu = (g_menu + 1) % SAVE_SLOTS;
    }
    if (pad_pressed(pad, LIPPUS_B)) {
        leave_slots();
        return;
    }
    if (pad_pressed(pad, LIPPUS_A) || pad_pressed(pad, LIPPUS_START)) {
        if (g_slot_mode) {
            if (!g_slot_ok[g_menu]) {
                sfx(SFX_LOCK);
            } else {
                apply_save(g_menu);
                if (g_state == ST_PLAY) {
                    draw_world();
                    draw_hud();
                    draw_banner();
                    return;
                }
            }
        } else if (g_slot_ok[g_menu]) {
            g_confirm = 1;
        } else if (capture_save(g_menu)) {
            banner("SAVED", 40);
            sfx(SFX_PICK);
        } else {
            banner("FAILED", 40);
            sfx(SFX_LOCK);
        }
    }
    draw_slots();
    draw_banner();
}

static void tick_inv(const lippus_pad_t *pad) {
    if (pad_pressed(pad, LIPPUS_B) || pad_pressed(pad, LIPPUS_SELECT) || pad_pressed(pad, LIPPUS_START)) {
        g_state = ST_PLAY;
    }
    draw_inv();
    draw_banner();
}

static void tick_dead(const lippus_pad_t *pad) {
    if (g_wait > 0) {
        g_wait--;
    } else if (pad_pressed(pad, LIPPUS_A)) {
        begin_run();
        return;
    } else if (pad_pressed(pad, LIPPUS_B)) {
        g_state = ST_TITLE;
        return;
    }
    draw_end("SYSTEM FAILURE", "DATA KEPT", "A RETRY   B TITLE");
}

static void tick_deck(const lippus_pad_t *pad) {
    if (g_wait > 0) {
        g_wait--;
    } else if (pad_pressed(pad, LIPPUS_A)) {
        next_deck();
        return;
    }
    draw_end(DECK_NAME[g_deck], "DECK CLEAR", "A NEXT DECK");
}

static void tick_win(const lippus_pad_t *pad) {
    if (g_wait > 0) {
        g_wait--;
    } else if (pad_pressed(pad, LIPPUS_A) || pad_pressed(pad, LIPPUS_START)) {
        g_state = ST_TITLE;
        return;
    }
    draw_end("HEGEMONY FALLS", "THE SHIP IS YOURS", "A TITLE");
}

void lippus_main(void) {
    load_disk();
    bake_tiles();
    g_state = ST_TITLE;
    g_face = 2;
    bgm_set_theme(BGM_INFILTRATION);
    for (;;) {
        lippus_pad_t pad = pad_poll();
        sfx_tick(FRAME);
        bgm_tick(FRAME);
        if (g_state == ST_TITLE) {
            tick_title(&pad);
        } else if (g_state == ST_UP) {
            tick_up(&pad);
        } else if (g_state == ST_PLAY) {
            tick_play(&pad);
        } else if (g_state == ST_PAUSE) {
            tick_pause(&pad);
        } else if (g_state == ST_INV) {
            tick_inv(&pad);
        } else if (g_state == ST_DEAD) {
            tick_dead(&pad);
        } else if (g_state == ST_DECK) {
            tick_deck(&pad);
        } else if (g_state == ST_SLOT) {
            tick_slot(&pad);
        } else {
            tick_win(&pad);
        }
        gfx_present();
        sys_frame(FRAME);
        g_frame++;
    }
}
