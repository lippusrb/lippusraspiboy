#include "lippus.h"
#include "sprites.h"

/* Maze Mouse, in verticale. Celle 16x16; il topo e' 32x32 con l'arte al centro. */

const uint32_t lippus_orientation = LIPPUS_PORTRAIT;

#define COLS 14
#define TILE 16
#define DIR_UP 0
#define DIR_LEFT 1
#define DIR_DOWN 2
#define DIR_RIGHT 3

#define T_EMPTY 0
#define T_WALL 1
#define T_DOT 2
#define T_POWER 3
#define T_DOOR 4

#define ST_READY 0
#define ST_PLAY 1
#define ST_EAT 2
#define ST_DEAD 3
#define ST_WIN 4
#define ST_OVER 5
#define ST_ATTRACT 6
#define ST_NAME 7

#define TUNNEL_R 7
#define HIT_R2 100
#define MAZES 10
#define SCORE_N 20
#define RECORD_NAME "MAZEMOUS"
#define MAX_LIVES 4

/* Un giro vale circa 1200 punti, circa 4000 se mangi un gruppo di gatti.
   10000 arriva dopo qualche livello; 30000 e 70000 restituiscono una vita persa. */
static const int k_life_bonus[] = {10000, 30000, 70000};
#define LIFE_BONUS_N (int)(sizeof k_life_bonus / sizeof k_life_bonus[0])

/* Dieci labirinti. '@' e' la partenza del topo, '--' la porta dei gatti. */
static const char k_mazes[MAZES][16][15] = {
    {
        "##############", "#o..........o#", "#.##.####.##.#", "#.##.####.##.#", "#............#",
        "##.##....##.##", "##.##.##.##.##", "    ......    ", "##.##.##.##.##", "##.##.##.##.##",
        "#............#", "##.###--###.##", "##.#      #.##", "#.....@......#", "#o.####.####o#",
        "##############",
    },
    {
        "##############", "#o..........o#", "#............#", "##.###--###.##", "##.#      #.##",
        "#............#", "#.#........#.#", "    ......    ", "#.#.##..##.#.#", "#.#........#.#",
        "#.##.####.##.#", "#............#", "#.##.#..#.##.#", "#@...........#", "#o#........#o#",
        "##############",
    },
    {
        "##############", "#o#........#o#", "#.#.##..##.#.#", "#............#", "#.##.####.##.#",
        "#............#", "#.##.####.##.#", "    ......    ", "#.##.####.##.#", "#............#",
        "##.###--###.##", "##.#      #.##", "#............#", "#@#........#.#", "#o#........#o#",
        "##############",
    },
    {
        "##############", "#o.##....##.o#", "#...#.##.#...#", "#.###.##.###.#", "#............#",
        "#.###....###.#", "#...#.##.#...#", "    ......    ", "#...#.##.#...#", "#.###....###.#",
        "#............#", "##.###--###.##", "##.#      #.##", "#.....@......#", "#o.##....##.o#",
        "##############",
    },
    {
        "##############", "#o..........o#", "#.###.##.###.#", "#............#", "##.###--###.##",
        "##.#      #.##", "#............#", "    ......    ", "#.###.##.###.#", "#............#",
        "#.###.##.###.#", "#............#", "#.###.##.###.#", "#@...........#", "#o..........o#",
        "##############",
    },
    {
        "##############", "#o..........o#", "#.##.#..#.##.#", "#....#..#....#", "#.##.#..#.##.#",
        "#............#", "##.###--###.##", "    #    #    ", "##.#      #.##", "#............#",
        "#.##.#..#.##.#", "#....#..#....#", "#.##.#..#.##.#", "#@...........#", "#o..........o#",
        "##############",
    },
    {
        "##############", "#o..........o#", "#.##########.#", "#.#........#.#", "#.#.######.#.#",
        "#.#........#.#", "#.#.##--##.#.#", "    #.  .#    ", "#.#.#.  .#.#.#", "#.#......#.#.#",
        "#.########.#.#", "#..........#.#", "#.##########.#", "#@...........#", "#o..........o#",
        "##############",
    },
    {
        "##############", "#o##......##o#", "#............#", "#.##########.#", "#.#........#.#",
        "#.#.######.#.#", "#.#........#.#", "    ......    ", "#.#.######.#.#", "#.#........#.#",
        "#............#", "##.###--###.##", "##.#      #.##", "#@...........#", "#o##......##o#",
        "##############",
    },
    {
        "##############", "#o#.#....#.#o#", "#.#.#.##.#.#.#", "#...#.##.#...#", "#.#...##...#.#",
        "#.#.######.#.#", "#............#", "    ......    ", "#.##########.#", "#............#",
        "##.###--###.##", "##.#      #.##", "#............#", "#@...........#", "#o#.#....#.#o#",
        "##############",
    },
    {
        "##############", "#o..........o#", "#.###.##.###.#", "#.#........#.#", "#.#.###..###.#",
        "#.#........#.#", "#.###.##.###.#", "    ......    ", "#............#", "#.##.####.##.#",
        "#............#", "#............#", "##.###--###.##", "##.#      #.##", "#o....@.....o#",
        "##############",
    },
};

#define ROWS 16

static const int8_t k_dx[4] = {0, -1, 0, 1};
static const int8_t k_dy[4] = {-1, 0, 1, 0};

/* Secondi di sonno, lampeggi e formaggi rimasti prima che il gatto rosso acceleri. */
static const uint8_t k_fright_s[21] = {
    6, 5, 4, 3, 2, 5, 2, 2, 1, 5, 2, 1, 1, 3, 1, 1, 0, 1, 0, 0, 0,
};
static const uint8_t k_fright_flash[21] = {
    5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 3, 3, 5, 3, 3, 0, 3, 0, 0, 0,
};
static const uint8_t k_elroy_dots[21] = {
    20, 30, 40, 40, 40, 50, 50, 50, 60, 60, 60, 80, 80, 80, 100, 100, 100, 100, 120, 120, 120,
};

static uint8_t g_map[16][COLS];
static int g_ox;
static int g_oy;
static int g_door_c;
static int g_door_r;
static int g_start_c;
static int g_start_r;
static int g_dots;
static int g_dots0;
static int g_score;
static int g_lives;
static int g_level;
static int g_bonus_i;
static int g_state;
static int g_timer;
static int g_fright;
static int g_flash_from;
static int g_combo;
static int g_waves[8];
static int g_wave_i;
static int g_wave_t;
static int g_scatter;
static int g_frame;
static int g_fruit_c;
static int g_fruit_r;
static int g_fruit;
static int g_fruit_mask;
static int g_fruit_banner;
static int g_fruit_pts;
static int g_fruit_bx;
static int g_fruit_by;
static int g_pac_acc;
static int g_ghost_acc[4];
static int g_chewing;
static int g_ate;
static int g_use_global;
static int g_global_dots;
static int g_ghost_dots[4];
static int g_no_dot_timer;
static int g_eat_x;
static int g_eat_y;
static int g_eat_pts;
static uint32_t g_rng = 0x6D2B79F5u;

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

typedef struct {
    int x;
    int y;
    int dir;
    int next;
} body_t;

typedef struct {
    body_t b;
    int wait;
    int eyes;
    int held;
    int home_c;
    int home_r;
} ghost_t;

static body_t g_pac;
static ghost_t g_ghosts[4];

static unsigned rnd(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 16;
}

static int opposite(int dir) { return (dir + 2) & 3; }

static int centered(const body_t *b) {
    return ((b->x - g_ox) & (TILE - 1)) == (TILE / 2) &&
           ((b->y - g_oy) & (TILE - 1)) == (TILE / 2);
}

static void cell_of(const body_t *b, int *c, int *r) {
    int rel = b->x - g_ox;
    int span = COLS * TILE;
    if (rel < 0) {
        rel += span;
    }
    *c = rel / TILE;
    *r = (b->y - g_oy) / TILE;
}

static void wrap_body(body_t *b) {
    int span = COLS * TILE;
    int rel = b->x - g_ox;
    if (rel < 0) {
        b->x += span;
    } else if (rel >= span) {
        b->x -= span;
    }
}

static int tile_x(int c) { return g_ox + c * TILE + TILE / 2; }
static int tile_y(int r) { return g_oy + r * TILE + TILE / 2; }

static int open_cell(int c, int r, int ghost) {
    if (r < 0 || r >= ROWS) {
        return 0;
    }
    if (c < 0) {
        c += COLS;
    } else if (c >= COLS) {
        c -= COLS;
    }
    uint8_t tile = g_map[r][c];
    if (tile == T_WALL) {
        return 0;
    }
    if (tile == T_DOOR && !ghost) {
        return 0;
    }
    return 1;
}

static int level_slot(void);

static int maze_no(void) {
    int lv = g_level < 1 ? 1 : g_level;
    return (lv - 1) % MAZES;
}

static int pace_ghost(int pct) {
    int lv = g_level < 1 ? 1 : g_level;
    if (lv > MAZES) {
        pct += (lv - MAZES) * 2;
    }
    if (pct > 140) {
        pct = 140;
    }
    return pct;
}

static int pace_mouse(int pct) {
    int lv = g_level < 1 ? 1 : g_level;
    if (lv > MAZES) {
        pct += (lv - MAZES) / 2;
    }
    if (pct > 120) {
        pct = 120;
    }
    return pct;
}

static int fright_seconds(void) {
    int s = k_fright_s[level_slot()];
    int lv = g_level < 1 ? 1 : g_level;
    if (lv > MAZES) {
        s -= (lv - MAZES) / 3;
    }
    if (s < 0) {
        s = 0;
    }
    return s;
}

static void load_map(void) {
    int maze = maze_no();
    g_dots = 0;
    g_door_c = 7;
    g_door_r = 11;
    g_start_c = 1;
    g_start_r = 13;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char ch = k_mazes[maze][r][c];
            uint8_t tile = T_EMPTY;
            if (ch == '#') {
                tile = T_WALL;
            } else if (ch == '.') {
                tile = T_DOT;
                g_dots++;
            } else if (ch == 'o') {
                tile = T_POWER;
                g_dots++;
            } else if (ch == '@') {
                g_start_c = c;
                g_start_r = r;
            } else if (ch == '-') {
                tile = T_DOOR;
                g_door_c = c;
                g_door_r = r;
            }
            g_map[r][c] = tile;
        }
    }
    g_dots0 = g_dots;
    g_fruit = 0;
    g_fruit_mask = 0;
    g_fruit_c = g_door_c;
    g_fruit_r = g_door_r + 2;
    for (int r = g_door_r + 1; r < ROWS; r++) {
        if (g_map[r][g_door_c] == T_EMPTY || g_map[r][g_door_c] == T_DOT) {
            g_fruit_r = r;
            break;
        }
    }
}

static void place(body_t *b, int c, int r, int dir) {
    b->x = tile_x(c);
    b->y = tile_y(r);
    b->dir = dir;
    b->next = dir;
}

/* 80% dell'originale vale 2 pixel a frame: il primo livello resta al passo di prima. */
static int level_slot(void) {
    int lv = g_level;
    if (lv < 1) {
        lv = 1;
    }
    if (lv > 21) {
        lv = 21;
    }
    return lv - 1;
}

/* Due volte -15%: a 80% sono ~2.9 px/frame, topo e gatti insieme. */
static int take_pixels(int *acc, int pct) {
    *acc += pct * 289;
    int n = *acc / 8000;
    *acc -= n * 8000;
    if (n > 6) {
        n = 6;
    }
    return n;
}

static int pac_pct(int chewing) {
    int lv = g_level < 1 ? 1 : g_level;
    if (chewing) {
        if (lv == 1) {
            return 71;
        }
        if (lv <= 4 || lv >= 21) {
            return 79;
        }
        return 87;
    }
    if (lv == 1) {
        return 80;
    }
    if (lv <= 4 || lv >= 21) {
        return 90;
    }
    return 100;
}

static int ghost_normal_pct(void) {
    if (g_level <= 1) {
        return 75;
    }
    if (g_level <= 4) {
        return 85;
    }
    return 95;
}

static int tunnel_pct(void) {
    if (g_level <= 1) {
        return 40;
    }
    if (g_level <= 4) {
        return 45;
    }
    return 50;
}

static int fright_spd_pct(void) {
    if (g_level <= 1) {
        return 50;
    }
    if (g_level <= 4) {
        return 55;
    }
    return 60;
}

static int elroy_tier(void) {
    if (g_use_global) {
        return 0;
    }
    int dots = k_elroy_dots[level_slot()];
    if (g_dots <= dots / 2) {
        return 2;
    }
    if (g_dots <= dots) {
        return 1;
    }
    return 0;
}

static int elroy_spd_pct(int tier) {
    if (g_level <= 1) {
        return tier == 1 ? 80 : 85;
    }
    if (g_level <= 4) {
        return tier == 1 ? 90 : 95;
    }
    return tier == 1 ? 100 : 105;
}

static int in_tunnel(int c, int r) { return r == TUNNEL_R && (c < 3 || c > 10); }

static int ghost_move_pct(int index, const ghost_t *g) {
    int c;
    int r;
    int tier = index == 0 ? elroy_tier() : 0;
    cell_of(&g->b, &c, &r);
    if (in_tunnel(c, r) && tier == 0) {
        return pace_ghost(tunnel_pct());
    }
    if (g_fright > 0) {
        return pace_ghost(fright_spd_pct());
    }
    if (tier == 2) {
        return pace_ghost(elroy_spd_pct(2));
    }
    if (tier == 1) {
        return pace_ghost(elroy_spd_pct(1));
    }
    return pace_ghost(ghost_normal_pct());
}

static int ghost_limit(int index) {
    if (index == 2) {
        return g_level <= 1 ? 30 : 0;
    }
    if (index == 3) {
        if (g_level <= 1) {
            return 60;
        }
        if (g_level == 2) {
            return 50;
        }
        return 0;
    }
    return 0;
}

static int global_gate(int index) {
    if (index == 1) {
        return 7;
    }
    if (index == 2) {
        return 17;
    }
    if (index == 3) {
        return 32;
    }
    return 0;
}

static void end_global_if_empty(void) {
    if (!g_ghosts[1].held && !g_ghosts[2].held && !g_ghosts[3].held) {
        g_use_global = 0;
    }
}

static void release_ready(void) {
    for (int i = 1; i <= 3; i++) {
        if (!g_ghosts[i].held) {
            continue;
        }
        int limit = g_use_global ? global_gate(i) : ghost_limit(i);
        int count = g_use_global ? g_global_dots : g_ghost_dots[i];
        if (count >= limit) {
            g_ghosts[i].held = 0;
        }
    }
    end_global_if_empty();
}

static void on_dot_eaten(void) {
    g_no_dot_timer = 0;
    g_ate = 1;
    g_chewing = 1;
    if (g_use_global) {
        g_global_dots++;
    } else {
        for (int i = 1; i <= 3; i++) {
            if (g_ghosts[i].held) {
                g_ghost_dots[i]++;
                break;
            }
        }
    }
    release_ready();
}

static void idle_release(void) {
    if (g_ate) {
        g_ate = 0;
        return;
    }
    if (++g_no_dot_timer < 4 * 30) {
        return;
    }
    g_no_dot_timer = 0;
    for (int i = 1; i <= 3; i++) {
        if (g_ghosts[i].held) {
            g_ghosts[i].held = 0;
            break;
        }
    }
    end_global_if_empty();
}

static int fruit_points(void) {
    int lv = g_level;
    if (lv <= 1) {
        return 100;
    }
    if (lv == 2) {
        return 300;
    }
    if (lv <= 4) {
        return 500;
    }
    if (lv <= 6) {
        return 700;
    }
    if (lv <= 8) {
        return 1000;
    }
    if (lv <= 10) {
        return 2000;
    }
    if (lv <= 12) {
        return 3000;
    }
    return 5000;
}

static const uint16_t *fruit_sprite(void) {
    /* Stessi scaglioni dei punti: un frutto diverso a ogni salto. */
    static const uint16_t *const k_by_level[] = {
        k_frutto_ciliegie, k_frutto_fragola, k_frutto_arancia, k_frutto_arancia,
        k_frutto_mela,      k_frutto_mela,    k_frutto_anguria, k_frutto_anguria,
        k_frutto_uva,       k_frutto_uva,     k_frutto_limone,  k_frutto_limone,
        k_frutto_banana,    k_frutto_banana,  k_frutto_ananas,
    };
    int lv = g_level;
    if (lv < 1) {
        lv = 1;
    }
    if (lv > (int)(sizeof k_by_level / sizeof k_by_level[0])) {
        lv = (int)(sizeof k_by_level / sizeof k_by_level[0]);
    }
    return k_by_level[lv - 1];
}

static void blit_spr(int cx, int cy, const uint16_t *pixels) {
    gfx_blit(cx - SPR_W / 2, cy - SPR_H / 2, SPR_W, SPR_H, pixels, 1, LIPPUS_COLOR_KEY);
}

static void blit_cat_spr(int cx, int cy, const uint16_t *pixels) {
    gfx_blit(cx - CAT_SPR / 2, cy - CAT_SPR / 2, CAT_SPR, CAT_SPR, pixels, 1, LIPPUS_COLOR_KEY);
}

static void blit_mouse(int cx, int cy, const uint16_t *pixels) {
    gfx_blit(cx - MOUSE_SPR / 2, cy - MOUSE_SPR / 2, MOUSE_SPR, MOUSE_SPR, pixels, 1, LIPPUS_COLOR_KEY);
}

static const uint16_t *mouse_sprite(int dir) {
    /* Sheet: 4 frame di camminata. La sinistra e' lo specchio della destra. */
    static const uint16_t *const tab[4][MOUSE_FRAMES] = {
        {k_topo_su0, k_topo_su1, k_topo_su2, k_topo_su3},
        {k_topo_sinistra0, k_topo_sinistra1, k_topo_sinistra2, k_topo_sinistra3},
        {k_topo_giu0, k_topo_giu1, k_topo_giu2, k_topo_giu3},
        {k_topo_destra0, k_topo_destra1, k_topo_destra2, k_topo_destra3},
    };
    int frame = (g_frame / 4) % MOUSE_FRAMES;
    if (dir < DIR_UP || dir > DIR_RIGHT) {
        dir = DIR_LEFT;
    }
    return tab[dir][frame];
}

static const uint16_t *cat_sprite(int index, int dir) {
    /* Sheet: riga giu/sx/dx/su, 3 frame di camminata. */
    static const uint16_t *const tab[4][4][CAT_FRAMES] = {
        {
            {k_gatto_arancio_giu0, k_gatto_arancio_giu1, k_gatto_arancio_giu2},
            {k_gatto_arancio_sinistra0, k_gatto_arancio_sinistra1, k_gatto_arancio_sinistra2},
            {k_gatto_arancio_destra0, k_gatto_arancio_destra1, k_gatto_arancio_destra2},
            {k_gatto_arancio_su0, k_gatto_arancio_su1, k_gatto_arancio_su2},
        },
        {
            {k_gatto_grigio_giu0, k_gatto_grigio_giu1, k_gatto_grigio_giu2},
            {k_gatto_grigio_sinistra0, k_gatto_grigio_sinistra1, k_gatto_grigio_sinistra2},
            {k_gatto_grigio_destra0, k_gatto_grigio_destra1, k_gatto_grigio_destra2},
            {k_gatto_grigio_su0, k_gatto_grigio_su1, k_gatto_grigio_su2},
        },
        {
            {k_gatto_marrone_giu0, k_gatto_marrone_giu1, k_gatto_marrone_giu2},
            {k_gatto_marrone_sinistra0, k_gatto_marrone_sinistra1, k_gatto_marrone_sinistra2},
            {k_gatto_marrone_destra0, k_gatto_marrone_destra1, k_gatto_marrone_destra2},
            {k_gatto_marrone_su0, k_gatto_marrone_su1, k_gatto_marrone_su2},
        },
        {
            {k_gatto_crema_giu0, k_gatto_crema_giu1, k_gatto_crema_giu2},
            {k_gatto_crema_sinistra0, k_gatto_crema_sinistra1, k_gatto_crema_sinistra2},
            {k_gatto_crema_destra0, k_gatto_crema_destra1, k_gatto_crema_destra2},
            {k_gatto_crema_su0, k_gatto_crema_su1, k_gatto_crema_su2},
        },
    };
    int d;
    int frame = (g_frame / 5) % CAT_FRAMES;
    if (index < 0) {
        index = 0;
    }
    if (index > 3) {
        index = 3;
    }
    if (dir == DIR_DOWN) {
        d = 0;
    } else if (dir == DIR_LEFT) {
        d = 1;
    } else if (dir == DIR_RIGHT) {
        d = 2;
    } else {
        d = 3;
    }
    return tab[index][d][frame];
}

static void setup_waves(void) {
    int s1;
    int c3;
    int s4;
    if (g_level <= 1) {
        s1 = 7 * 30;
        c3 = 20 * 30;
        s4 = 5 * 30;
    } else if (g_level <= 4) {
        s1 = 7 * 30;
        c3 = 1033 * 30;
        s4 = 1;
    } else {
        s1 = 5 * 30;
        c3 = 1037 * 30;
        s4 = 1;
    }
    g_waves[0] = s1;
    g_waves[1] = 20 * 30;
    g_waves[2] = s1;
    g_waves[3] = 20 * 30;
    g_waves[4] = 5 * 30;
    g_waves[5] = c3;
    g_waves[6] = s4;
    g_waves[7] = 0;
    g_wave_i = 0;
    g_wave_t = g_waves[0];
    g_scatter = 1;
}

static void reset_actors(void) {
    place(&g_pac, g_start_c, g_start_r, DIR_LEFT);
    if (g_map[g_start_r][g_start_c] == T_DOT || g_map[g_start_r][g_start_c] == T_POWER) {
        g_map[g_start_r][g_start_c] = T_EMPTY;
        g_dots--;
    }
    place(&g_ghosts[0].b, g_door_c, g_door_r - 1, DIR_LEFT);
    place(&g_ghosts[1].b, g_door_c, g_door_r + 1, DIR_UP);
    place(&g_ghosts[2].b, g_door_c - 1, g_door_r + 1, DIR_RIGHT);
    place(&g_ghosts[3].b, g_door_c + 1, g_door_r + 1, DIR_LEFT);
    for (int i = 0; i < 4; i++) {
        g_ghosts[i].wait = 0;
        g_ghosts[i].eyes = 0;
        g_ghosts[i].held = i != 0;
        g_ghosts[i].home_c = g_door_c + (i == 2 ? -1 : (i == 3 ? 1 : 0));
        g_ghosts[i].home_r = i == 0 ? g_door_r - 1 : g_door_r + 1;
        g_ghost_acc[i] = 0;
        g_ghost_dots[i] = 0;
    }
    g_pac_acc = 0;
    g_chewing = 0;
    g_ate = 0;
    g_use_global = 1;
    g_global_dots = 0;
    g_no_dot_timer = 0;
    g_fright = 0;
    g_flash_from = 0;
    g_combo = 0;
    g_fruit_banner = 0;
    setup_waves();
}

static void quiet(void) {
    for (unsigned i = 0; i < SND_VOICES; i++) {
        snd_voice(i, SND_OFF, 0, 0, 0);
    }
}

static int in_house(int c, int r) {
    return r == g_door_r + 1 && c >= g_door_c - 1 && c <= g_door_c + 1;
}

static void ghost_target(int index, int *tc, int *tr) {
    ghost_t *g = &g_ghosts[index];
    int c;
    int r;
    cell_of(&g->b, &c, &r);
    if (g->eyes) {
        *tc = g_door_c;
        *tr = g_door_r;
        return;
    }
    if (in_house(c, r) || r == g_door_r) {
        *tc = g_door_c;
        *tr = g_door_r - 1;
        return;
    }
    if (g_scatter) {
        *tc = (index == 0 || index == 2) ? COLS - 2 : 1;
        *tr = (index == 0 || index == 1) ? 1 : ROWS - 2;
        return;
    }
    int pc;
    int pr;
    cell_of(&g_pac, &pc, &pr);
    int pdx = g_pac.dir >= 0 ? k_dx[g_pac.dir] : 0;
    int pdy = g_pac.dir >= 0 ? k_dy[g_pac.dir] : 0;
    if (index == 0) {
        *tc = pc;
        *tr = pr;
    } else if (index == 1) {
        *tc = pc + pdx * 4;
        *tr = pr + pdy * 4;
    } else if (index == 2) {
        int ax = pc + pdx * 2;
        int ay = pr + pdy * 2;
        int bc;
        int br;
        cell_of(&g_ghosts[0].b, &bc, &br);
        *tc = ax + (ax - bc);
        *tr = ay + (ay - br);
    } else {
        int dc = pc - c;
        int dr = pr - r;
        if (dc < 0) {
            dc = -dc;
        }
        if (dr < 0) {
            dr = -dr;
        }
        if (dc + dr > 8) {
            *tc = pc;
            *tr = pr;
        } else {
            *tc = 1;
            *tr = ROWS - 2;
        }
    }
}

static int choose_dir(int index) {
    ghost_t *g = &g_ghosts[index];
    int c;
    int r;
    cell_of(&g->b, &c, &r);
    int opts[4];
    int count = 0;
    int back = g->b.dir >= 0 ? opposite(g->b.dir) : -1;
    for (int dir = 0; dir < 4; dir++) {
        if (dir == back) {
            continue;
        }
        if (!open_cell(c + k_dx[dir], r + k_dy[dir], 1)) {
            continue;
        }
        opts[count++] = dir;
    }
    if (count == 0) {
        if (back >= 0 && open_cell(c + k_dx[back], r + k_dy[back], 1)) {
            return back;
        }
        return -1;
    }
    if (g_fright > 0 && !g->eyes) {
        return opts[rnd() % (unsigned)count];
    }
    int tc;
    int tr;
    ghost_target(index, &tc, &tr);
    int best = opts[0];
    int best_d = 1 << 30;
    for (int i = 0; i < count; i++) {
        int nc = c + k_dx[opts[i]] - tc;
        int nr = r + k_dy[opts[i]] - tr;
        int dist = nc * nc + nr * nr;
        if (dist < best_d) {
            best_d = dist;
            best = opts[i];
        }
    }
    return best;
}

static void step_pac(void) {
    if (g_pac.dir >= 0 && g_pac.next == opposite(g_pac.dir)) {
        g_pac.dir = g_pac.next;
    }
    if (centered(&g_pac)) {
        int c;
        int r;
        cell_of(&g_pac, &c, &r);
        int dir = g_pac.dir;
        if (g_pac.next >= 0 && open_cell(c + k_dx[g_pac.next], r + k_dy[g_pac.next], 0)) {
            dir = g_pac.next;
        } else if (dir < 0 || !open_cell(c + k_dx[dir], r + k_dy[dir], 0)) {
            dir = -1;
        }
        g_pac.dir = dir;
    }
    if (g_pac.dir < 0) {
        return;
    }
    g_pac.x += k_dx[g_pac.dir];
    g_pac.y += k_dy[g_pac.dir];
    wrap_body(&g_pac);
}

static void step_ghost(int index) {
    ghost_t *g = &g_ghosts[index];
    if (g->held) {
        return;
    }
    if (g->wait > 0) {
        g->wait--;
        return;
    }
    if (centered(&g->b)) {
        int c;
        int r;
        cell_of(&g->b, &c, &r);
        if (g->eyes && r == g_door_r && (c == g_door_c || c == g_door_c - 1)) {
            place(&g->b, g->home_c, g->home_r, DIR_UP);
            g->eyes = 0;
            g->held = 1;
            g_ghost_dots[index] = 0;
            if ((g_use_global && g_global_dots >= global_gate(index)) ||
                (!g_use_global && ghost_limit(index) <= 0)) {
                g->held = 0;
                g->wait = 20;
            }
            return;
        }
        g->b.dir = choose_dir(index);
    }
    if (g->b.dir < 0) {
        return;
    }
    g->b.x += k_dx[g->b.dir];
    g->b.y += k_dy[g->b.dir];
    wrap_body(&g->b);
}

static void reverse_ghosts(void) {
    for (int i = 0; i < 4; i++) {
        if (g_ghosts[i].eyes || g_ghosts[i].wait > 0 || g_ghosts[i].held || g_ghosts[i].b.dir < 0) {
            continue;
        }
        g_ghosts[i].b.dir = opposite(g_ghosts[i].b.dir);
    }
}

static void tick_mode(void) {
    if (g_fright > 0) {
        g_fright--;
        if (g_fright > 0) {
            return;
        }
        g_combo = 0;
        reverse_ghosts();
    }
    if (g_wave_i >= 7 || g_waves[g_wave_i] == 0) {
        return;
    }
    if (--g_wave_t > 0) {
        return;
    }
    if (g_wave_i < 7) {
        g_wave_i++;
    }
    g_wave_t = g_waves[g_wave_i];
    g_scatter = (g_wave_i % 2) == 0;
    reverse_ghosts();
}

static void add_score(int pts) {
    int gained = 0;
    g_score += pts;
    while (g_bonus_i < LIFE_BONUS_N && g_score >= k_life_bonus[g_bonus_i]) {
        g_bonus_i++;
        if (g_lives < MAX_LIVES) {
            g_lives++;
            gained = 1;
        }
    }
    if (gained) {
        snd_voice(3, SND_TRIANGLE, 1046, 90, 140);
    }
}

static void eat_dot(void) {
    if (!centered(&g_pac)) {
        return;
    }
    int c;
    int r;
    cell_of(&g_pac, &c, &r);
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
        return;
    }
    uint8_t tile = g_map[r][c];
    if (tile == T_DOT) {
        g_map[r][c] = T_EMPTY;
        g_dots--;
        add_score(10);
        on_dot_eaten();
        snd_voice(1, SND_SQUARE, 1180, 24, 16);
    } else if (tile == T_POWER) {
        int frames = fright_seconds() * 30;
        g_map[r][c] = T_EMPTY;
        g_dots--;
        add_score(50);
        on_dot_eaten();
        if (frames > 0) {
            g_fright = frames;
            g_flash_from = k_fright_flash[level_slot()] * 16;
            if (g_flash_from > g_fright) {
                g_flash_from = g_fright;
            }
            g_combo = 0;
            reverse_ghosts();
            snd_voice(2, SND_TRIANGLE, 280, 90, 160);
        } else {
            snd_voice(2, SND_NOISE, 200, 40, 40);
        }
    } else if (centered(&g_pac)) {
        g_chewing = 0;
    }
    if (g_fruit > 0 && c == g_fruit_c && r == g_fruit_r) {
        g_fruit = 0;
        g_fruit_pts = fruit_points();
        add_score(g_fruit_pts);
        g_fruit_bx = tile_x(c);
        g_fruit_by = tile_y(r);
        g_fruit_banner = 30;
        snd_voice(2, SND_SQUARE, 880, 70, 40);
    }
}

static void collide(void) {
    for (int i = 0; i < 4; i++) {
        ghost_t *g = &g_ghosts[i];
        if (g->eyes || g->wait > 0 || g->held) {
            continue;
        }
        int dx = g_pac.x - g->b.x;
        int dy = g_pac.y - g->b.y;
        if (dx * dx + dy * dy > HIT_R2) {
            continue;
        }
        if (g_fright > 0) {
            g->eyes = 1;
            g_combo++;
            if (g_combo > 4) {
                g_combo = 4;
            }
            g_eat_pts = 200 << (g_combo - 1);
            add_score(g_eat_pts);
            g_eat_x = g->b.x - 10;
            g_eat_y = g->b.y - 14;
            g_state = ST_EAT;
            g_timer = 18;
            snd_voice(2, SND_TRIANGLE, (uint16_t)(420 + g_combo * 80), 100, 70);
            return;
        }
        g_state = ST_DEAD;
        g_timer = 55;
        g_lives--;
        quiet();
        snd_voice(0, SND_NOISE, 420, 160, 180);
        return;
    }
}

static void maybe_fruit(void) {
    int eaten = g_dots0 - g_dots;
    int first = g_dots0 / 3;
    int second = (2 * g_dots0) / 3;
    if (first < 1) {
        first = 1;
    }
    if (g_fruit == 0 && eaten == first && (g_fruit_mask & 1) == 0) {
        g_fruit = 9 * 30;
        g_fruit_mask |= 1;
    } else if (g_fruit == 0 && eaten == second && (g_fruit_mask & 2) == 0) {
        g_fruit = 9 * 30;
        g_fruit_mask |= 2;
    }
    if (g_fruit > 0) {
        g_fruit--;
    }
}

static void read_stick(const lippus_pad_t *pad) {
    int want = -1;
    if (pad_pressed(pad, LIPPUS_UP)) {
        want = DIR_UP;
    } else if (pad_pressed(pad, LIPPUS_DOWN)) {
        want = DIR_DOWN;
    } else if (pad_pressed(pad, LIPPUS_LEFT)) {
        want = DIR_LEFT;
    } else if (pad_pressed(pad, LIPPUS_RIGHT)) {
        want = DIR_RIGHT;
    } else if (pad_held(pad, LIPPUS_UP)) {
        want = DIR_UP;
    } else if (pad_held(pad, LIPPUS_DOWN)) {
        want = DIR_DOWN;
    } else if (pad_held(pad, LIPPUS_LEFT)) {
        want = DIR_LEFT;
    } else if (pad_held(pad, LIPPUS_RIGHT)) {
        want = DIR_RIGHT;
    }
    if (want >= 0) {
        g_pac.next = want;
    }
}

static void draw_mouse(int x, int y, int dir) {
    blit_mouse(x, y, mouse_sprite(dir < 0 ? DIR_LEFT : dir));
}

static void draw_cat(const ghost_t *g, int index, int asleep) {
    int x = g->b.x;
    int y = g->b.y;
    if (g->eyes) {
        blit_spr(x, y, k_gatto_occhi);
        return;
    }
    if (asleep) {
        int flash = g_fright < g_flash_from && ((g_fright / 8) & 1);
        if (flash) {
            gfx_fill_circle(x, y - 1, 10, LIPPUS_WHITE);
            gfx_fill_rect(x - 8, y - 2, 16, 12, LIPPUS_WHITE);
        } else {
            blit_cat_spr(x, y, k_gatto_sonno);
        }
        return;
    }
    blit_cat_spr(x, y, cat_sprite(index, g->b.dir < 0 ? DIR_LEFT : g->b.dir));
}

static void draw_maze(void) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x = g_ox + c * TILE;
            int y = g_oy + r * TILE;
            uint8_t tile = g_map[r][c];
            if (tile == T_WALL) {
                int run = (c > 0 && g_map[r][c - 1] == T_WALL) ||
                          (c + 1 < COLS && g_map[r][c + 1] == T_WALL);
                gfx_blit(x, y, SPR_W, SPR_H, run ? k_muro_tubo : k_muro, 0, 0);
            } else if (tile == T_DOT) {
                blit_spr(x + TILE / 2, y + TILE / 2, k_formaggio);
            } else if (tile == T_POWER) {
                if ((g_frame & 16) == 0) {
                    blit_spr(x + TILE / 2, y + TILE / 2, k_ciotola);
                }
            } else if (tile == T_DOOR) {
                gfx_blit(x, y, SPR_W, SPR_H, k_porta, 1, LIPPUS_COLOR_KEY);
            }
        }
    }
    if (g_fruit > 0) {
        blit_spr(tile_x(g_fruit_c), tile_y(g_fruit_r), fruit_sprite());
    }
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

static void prefix_u(char *dst, const char *prefix, unsigned value) {
    while (*prefix) {
        *dst++ = *prefix++;
    }
    format_u(dst, value);
}

static void blit_life(int x, int y) {
    static uint16_t icon[64];
    static int ready;
    if (!ready) {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                icon[row * 8 + col] = k_vita[(row * 2) * 16 + col * 2];
            }
        }
        ready = 1;
    }
    gfx_blit(x, y, 8, 8, icon, 1, LIPPUS_COLOR_KEY);
}

static void draw_hud(void) {
    char score[16];
    char next[8];
    char level[8];
    int y = 2;
    prefix_u(score, "SCORE ", (unsigned)g_score);
    gfx_text(2, y, score, LIPPUS_CREAM, 1);
    int x = 2 + gfx_text_width(score, 1);
    if (g_bonus_i < LIFE_BONUS_N) {
        gfx_text(x, y, "/", LIPPUS_CREAM, 1);
        x += gfx_text_width("/", 1);
        format_u(next, (unsigned)k_life_bonus[g_bonus_i]);
        gfx_text(x, y, next, LIPPUS_GREEN, 1);
    }

    prefix_u(level, "Lv ", (unsigned)g_level);
    int level_x = gfx_width() - 2 - gfx_text_width(level, 1);
    gfx_text(level_x, y, level, LIPPUS_AMBER, 1);

    int lives = g_lives;
    if (lives < 0) {
        lives = 0;
    }
    if (lives > MAX_LIVES) {
        lives = MAX_LIVES;
    }
    if (lives > 0) {
        int mice_w = lives * 9 - 1;
        int mx = level_x - 4 - mice_w;
        for (int i = 0; i < lives; i++) {
            blit_life(mx, y);
            mx += 9;
        }
    }
}

static void draw_world(void) {
    int span = COLS * TILE;
    gfx_clear(LIPPUS_BLACK);
    draw_maze();
    for (int i = 0; i < 4; i++) {
        ghost_t *g = &g_ghosts[i];
        int asleep = g_fright > 0 && !g->eyes;
        draw_cat(g, i, asleep);
        if (g->b.x - g_ox < TILE) {
            body_t copy = g->b;
            g->b.x += span;
            draw_cat(g, i, asleep);
            g->b = copy;
        }
    }
    draw_mouse(g_pac.x, g_pac.y, g_pac.dir);
    if (g_pac.x - g_ox < TILE) {
        draw_mouse(g_pac.x + span, g_pac.y, g_pac.dir);
    }
    if (g_state == ST_EAT) {
        char pts[8];
        format_u(pts, (unsigned)g_eat_pts);
        gfx_text(g_eat_x, g_eat_y, pts, LIPPUS_CYAN, 1);
    }
    if (g_fruit_banner > 0) {
        char pts[8];
        format_u(pts, (unsigned)g_fruit_pts);
        gfx_text(g_fruit_bx - gfx_text_width(pts, 1) / 2, g_fruit_by - 10, pts, LIPPUS_CREAM, 1);
    }
    draw_hud();
    if (g_state == ST_READY) {
        gfx_text((gfx_width() - gfx_text_width("READY", 2)) / 2, g_oy + 5 * TILE, "READY", LIPPUS_YELLOW, 2);
    } else if (g_state == ST_OVER) {
        gfx_text((gfx_width() - gfx_text_width("GAME OVER", 2)) / 2, g_oy + 6 * TILE, "GAME OVER",
                 LIPPUS_RED, 2);
    }
}

static void center_text(int y, const char *text, uint16_t color, int scale) {
    gfx_text((gfx_width() - gfx_text_width(text, scale)) / 2, y, text, color, scale);
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
    for (int i = 0; i < g_score_n; i++) {
        char num[8];
        if (n + 16 >= (int)LIPPUS_RECORD_MAX) {
            break;
        }
        g_rec[n++] = g_scores[i].name[0];
        g_rec[n++] = g_scores[i].name[1];
        g_rec[n++] = g_scores[i].name[2];
        g_rec[n++] = ' ';
        format_u(num, (unsigned)g_scores[i].score);
        for (int k = 0; num[k]; k++) {
            g_rec[n++] = num[k];
        }
        g_rec[n++] = '\n';
    }
    sys_record_write(RECORD_NAME, g_rec, n);
}

static void score_insert(int score) {
    int at = g_score_n;
    int last;
    for (int i = 0; i < g_score_n; i++) {
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
    for (int i = last; i > at; i--) {
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

static void draw_score_row(int y, int rank, const score_t *row, uint16_t color) {
    char pos[4];
    char name[4];
    char num[8];
    int pos_w;
    format_u(pos, (unsigned)(rank + 1));
    name[0] = row->name[0];
    name[1] = row->name[1];
    name[2] = row->name[2];
    name[3] = 0;
    format_u(num, (unsigned)row->score);
    pos_w = gfx_text_width(pos, 1);
    gfx_text(72 - pos_w, y, pos, color, 1);
    gfx_text(88, y, name, color, 1);
    gfx_text(184 - gfx_text_width(num, 1), y, num, color, 1);
}

static void draw_press_start(void) {
    if ((g_frame & 16) == 0) {
        center_text(gfx_height() - 18, "PRESS START", LIPPUS_YELLOW, 1);
    }
}

static void draw_demo(void) {
    int t = g_attract_t;
    const char *caption = "FRUIT SCORES POINTS";
    center_text(28, "MAZE MOUSE", LIPPUS_AMBER, 2);
    if (t < 110) {
        int x = 28 + t * 2;
        caption = "EAT EVERY CHEESE";
        if (x > 210) {
            x = 210;
        }
        for (int i = 0; i < 4; i++) {
            int cx = 56 + i * 36;
            if (x + 6 < cx) {
                blit_spr(cx, 150, k_formaggio);
            }
        }
        blit_mouse(x, 150, mouse_sprite(DIR_RIGHT));
    } else if (t < 220) {
        int asleep = t > 165;
        caption = "MILK PUTS CATS TO SLEEP";
        blit_spr(120, 128, k_ciotola);
        blit_mouse(120, 88, mouse_sprite(DIR_DOWN));
        blit_cat_spr(64, 176, asleep ? k_gatto_sonno : cat_sprite(0, DIR_RIGHT));
        blit_cat_spr(176, 176, asleep ? k_gatto_sonno : cat_sprite(2, DIR_LEFT));
    } else if (t < 320) {
        int catx = 36 + (t - 220);
        caption = "DO NOT GET CAUGHT";
        if (catx > 150) {
            catx = 150;
        }
        blit_cat_spr(catx, 150, cat_sprite(0, DIR_RIGHT));
        blit_mouse(196, 150, mouse_sprite(DIR_RIGHT));
        if (catx < 140 || (g_frame & 8) == 0) {
            blit_spr(120, 210, k_vita);
        }
    } else {
        blit_mouse(72, 150, mouse_sprite(DIR_RIGHT));
        blit_spr(140, 150, fruit_sprite());
        center_text(196, "+1000", LIPPUS_CREAM, 2);
    }
    center_text(248, caption, LIPPUS_CREAM, 1);
}

static void draw_scores(void) {
    center_text(8, "TOP 20", LIPPUS_AMBER, 2);
    if (g_score_n == 0) {
        center_text(120, "NO SCORES YET", LIPPUS_CREAM, 1);
        return;
    }
    for (int i = 0; i < g_score_n; i++) {
        uint16_t color = (i == g_rank) ? LIPPUS_YELLOW : LIPPUS_CREAM;
        draw_score_row(32 + i * 12, i, &g_scores[i], color);
    }
}

static void draw_attract(void) {
    gfx_clear(LIPPUS_BLACK);
    if (g_attract_page == 0) {
        draw_demo();
    } else {
        draw_scores();
    }
    draw_press_start();
}

static void draw_name(void) {
    char shown[4];
    gfx_clear(LIPPUS_BLACK);
    center_text(48, "NEW HIGH SCORE", LIPPUS_AMBER, 1);
    center_text(72, "YOUR NAME", LIPPUS_CREAM, 1);
    shown[0] = g_initials[0];
    shown[1] = g_initials[1];
    shown[2] = g_initials[2];
    shown[3] = 0;
    center_text(120, shown, LIPPUS_YELLOW, 3);
    if ((g_frame & 8) == 0) {
        int x = (gfx_width() - gfx_text_width(shown, 3)) / 2 + g_name_i * 24;
        gfx_fill_rect(x, 148, 20, 3, LIPPUS_YELLOW);
    }
    center_text(200, "UP DOWN", LIPPUS_CREAM, 1);
    center_text(216, "START ENTERS", LIPPUS_CREAM, 1);
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
    quiet();
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
    quiet();
    g_state = ST_OVER;
    g_timer = 120;
}

static void enter_ready(void) {
    g_state = ST_READY;
    g_timer = 70;
    reset_actors();
    quiet();
    snd_voice(0, SND_TRIANGLE, 700, 80, 50);
}

static void new_game(void) {
    g_score = 0;
    g_lives = 3;
    g_level = 1;
    g_bonus_i = 0;
    load_map();
    enter_ready();
}

void lippus_main(void) {
    g_ox = (gfx_width() - COLS * TILE) / 2;
    g_oy = 20;
    g_rank = -1;
    scores_load();
    enter_attract(0);
    for (;;) {
        lippus_pad_t pad = pad_poll();
        g_frame++;
        if (g_fruit_banner > 0) {
            g_fruit_banner--;
        }
        if (g_state == ST_ATTRACT) {
            if (pad_pressed(&pad, LIPPUS_START)) {
                g_rank = -1;
                new_game();
                draw_world();
            } else {
                /* Anteprima ~14 s, classifica ~3 s. */
                int hold = g_attract_page == 1 ? 90 : 420;
                if (++g_attract_t >= hold) {
                    g_attract_t = 0;
                    g_attract_page ^= 1;
                }
                draw_attract();
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
            read_stick(&pad);
            if (--g_timer <= 0) {
                g_state = ST_PLAY;
            }
        } else if (g_state == ST_EAT) {
            if (--g_timer <= 0) {
                g_state = ST_PLAY;
            }
        } else if (g_state == ST_DEAD) {
            if (--g_timer <= 0) {
                if (g_lives > 0) {
                    enter_ready();
                } else {
                    enter_game_over();
                }
            }
        } else if (g_state == ST_WIN) {
            if (--g_timer <= 0) {
                g_level++;
                load_map();
                enter_ready();
            }
        } else {
            read_stick(&pad);
            {
                int steps = take_pixels(&g_pac_acc, pace_mouse(pac_pct(g_chewing)));
                for (int s = 0; s < steps; s++) {
                    step_pac();
                    eat_dot();
                    if (g_dots == 0) {
                        break;
                    }
                }
            }
            if (g_dots == 0) {
                g_state = ST_WIN;
                g_timer = 40;
                quiet();
                snd_voice(0, SND_TRIANGLE, 640, 120, 80);
            } else {
                idle_release();
                tick_mode();
                maybe_fruit();
                for (int i = 0; i < 4 && g_state == ST_PLAY; i++) {
                    ghost_t *g = &g_ghosts[i];
                    int steps = 0;
                    if (g->held) {
                        steps = 0;
                    } else if (g->wait > 0) {
                        steps = 1;
                    } else if (g->eyes) {
                        steps = 6;
                    } else {
                        steps = take_pixels(&g_ghost_acc[i], ghost_move_pct(i, g));
                    }
                    for (int s = 0; s < steps && g_state == ST_PLAY; s++) {
                        step_ghost(i);
                        if (g->held || g->wait > 0) {
                            break;
                        }
                        collide();
                    }
                }
            }
        }
        draw_world();
        gfx_present();
        sys_frame(33);
    }
}
