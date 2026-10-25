#include "lippus.h"

/* Maze Mouse, in verticale. Celle e sprite 16x16: piu' grandi dell'8x8
   originale, con ancora abbastanza corridoi sullo schermo 240x320. */

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

#define TUNNEL_R 7
#define MOUSE_C 7
#define MOUSE_R 13
#define HIT_R2 100

static const char *const k_rows[] = {
    "##############",
    "#o..........o#",
    "#.##.####.##.#",
    "#.##.####.##.#",
    "#............#",
    "##.##....##.##",
    "##.##.##.##.##",
    "    ......    ",
    "##.##.##.##.##",
    "##.##.##.##.##",
    "#............#",
    "##.###--###.##",
    "##.#      #.##",
    "#............#",
    "#o.####.####o#",
    "##############",
};

#define ROWS (int)(sizeof k_rows / sizeof k_rows[0])

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

static const uint16_t k_cat_col[4] = {
    LIPPUS_ORANGE,
    LIPPUS_RGB(170, 174, 184),
    LIPPUS_RGB(96, 72, 58),
    LIPPUS_RGB(214, 186, 140),
};

static uint8_t g_map[16][COLS];
static int g_ox;
static int g_oy;
static int g_door_c;
static int g_door_r;
static int g_dots;
static int g_dots0;
static int g_score;
static int g_lives;
static int g_level;
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

static void load_map(void) {
    g_dots = 0;
    g_door_c = 7;
    g_door_r = 11;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char ch = k_rows[r][c];
            uint8_t tile = T_EMPTY;
            if (ch == '#') {
                tile = T_WALL;
            } else if (ch == '.') {
                tile = T_DOT;
                g_dots++;
            } else if (ch == 'o') {
                tile = T_POWER;
                g_dots++;
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

/* Tile 16: *10 al posto di *5, cosi' a ~80% restano ~4 px/frame. */
static int take_pixels(int *acc, int pct) {
    *acc += pct * 10;
    int n = *acc / 200;
    *acc -= n * 200;
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
        return tunnel_pct();
    }
    if (g_fright > 0) {
        return fright_spd_pct();
    }
    if (tier == 2) {
        return elroy_spd_pct(2);
    }
    if (tier == 1) {
        return elroy_spd_pct(1);
    }
    return ghost_normal_pct();
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

static uint16_t fruit_color(void) {
    int lv = g_level;
    if (lv <= 2) {
        return LIPPUS_RED;
    }
    if (lv <= 4) {
        return LIPPUS_ORANGE;
    }
    if (lv <= 6) {
        return LIPPUS_RGB(180, 0, 48);
    }
    if (lv <= 8) {
        return LIPPUS_GREEN;
    }
    if (lv <= 10) {
        return LIPPUS_CYAN;
    }
    if (lv <= 12) {
        return LIPPUS_YELLOW;
    }
    return LIPPUS_WHITE;
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
    place(&g_pac, MOUSE_C, MOUSE_R, DIR_LEFT);
    if (g_map[MOUSE_R][MOUSE_C] == T_DOT || g_map[MOUSE_R][MOUSE_C] == T_POWER) {
        g_map[MOUSE_R][MOUSE_C] = T_EMPTY;
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
        g_score += 10;
        on_dot_eaten();
        snd_voice(1, SND_SQUARE, 1180, 24, 16);
    } else if (tile == T_POWER) {
        int frames = k_fright_s[level_slot()] * 30;
        g_map[r][c] = T_EMPTY;
        g_dots--;
        g_score += 50;
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
        g_score += g_fruit_pts;
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
            g_score += g_eat_pts;
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
    uint16_t fur = LIPPUS_RGB(176, 132, 86);
    uint16_t ear = LIPPUS_RGB(120, 78, 46);
    gfx_fill_circle(x - 5, y - 6, 3, ear);
    gfx_fill_circle(x + 5, y - 6, 3, ear);
    gfx_fill_circle(x, y, 5, fur);
    gfx_fill_rect(x - 2, y - 1, 2, 2, LIPPUS_BLACK);
    if (dir >= 0) {
        gfx_fill_rect(x + k_dx[dir] * 5, y + k_dy[dir] * 5, 3, 3, LIPPUS_RGB(255, 150, 170));
        if (dir == DIR_LEFT) {
            gfx_fill_rect(x + 5, y, 5, 2, ear);
        } else if (dir == DIR_RIGHT) {
            gfx_fill_rect(x - 9, y, 5, 2, ear);
        } else if (dir == DIR_UP) {
            gfx_fill_rect(x + 3, y + 4, 2, 5, ear);
        } else {
            gfx_fill_rect(x + 3, y - 9, 2, 5, ear);
        }
    }
}

static void draw_cat(const ghost_t *g, uint16_t color, int asleep) {
    int x = g->b.x;
    int y = g->b.y;
    int px = 0;
    int py = 0;
    if (!asleep && g->b.dir >= 0) {
        px = k_dx[g->b.dir];
        py = k_dy[g->b.dir];
    }
    if (!g->eyes) {
        gfx_fill_rect(x - 6, y - 11, 5, 5, color);
        gfx_fill_rect(x + 2, y - 11, 5, 5, color);
        gfx_fill_circle(x, y - 3, 6, color);
        gfx_fill_rect(x - 6, y - 3, 13, 8, color);
        if (g->b.dir == DIR_LEFT) {
            gfx_fill_rect(x + 5, y, 5, 3, color);
        } else if (g->b.dir == DIR_RIGHT) {
            gfx_fill_rect(x - 10, y, 5, 3, color);
        } else if (g->b.dir == DIR_DOWN) {
            gfx_fill_rect(x + 4, y - 9, 3, 5, color);
        } else {
            gfx_fill_rect(x + 4, y + 4, 3, 5, color);
        }
    }
    if (asleep) {
        gfx_fill_rect(x - 4, y - 4, 3, 2, LIPPUS_BLACK);
        gfx_fill_rect(x + 2, y - 4, 3, 2, LIPPUS_BLACK);
    } else {
        gfx_fill_rect(x - 4, y - 5, 3, 3, LIPPUS_WHITE);
        gfx_fill_rect(x + 2, y - 5, 3, 3, LIPPUS_WHITE);
        gfx_fill_rect(x - 3 + px, y - 4 + py, 2, 2, LIPPUS_BLACK);
        gfx_fill_rect(x + 3 + px, y - 4 + py, 2, 2, LIPPUS_BLACK);
    }
}

static void draw_maze(void) {
    const uint16_t brick = LIPPUS_RGB(150, 78, 52);
    const uint16_t mortar = LIPPUS_RGB(86, 46, 34);
    const uint16_t pipe = LIPPUS_RGB(132, 140, 152);
    const uint16_t shine = LIPPUS_RGB(206, 212, 220);
    for (int r = 0; r < ROWS; r++) {
        int c = 0;
        while (c < COLS) {
            if (g_map[r][c] != T_WALL) {
                c++;
                continue;
            }
            int c0 = c;
            while (c < COLS && g_map[r][c] == T_WALL) {
                c++;
            }
            int x0 = g_ox + c0 * TILE;
            int y0 = g_oy + r * TILE;
            int w = (c - c0) * TILE;
            gfx_fill_rect(x0, y0, w, TILE, brick);
            gfx_fill_rect(x0, y0 + TILE - 2, w, 2, mortar);
            if (c - c0 >= 2) {
                gfx_fill_rect(x0, y0 + 5, w, 3, pipe);
                gfx_fill_rect(x0, y0 + 5, w, 1, shine);
            }
        }
        for (c = 0; c < COLS; c++) {
            int x = g_ox + c * TILE;
            int y = g_oy + r * TILE;
            uint8_t tile = g_map[r][c];
            if (tile == T_DOT) {
                gfx_fill_rect(x + 4, y + 5, 8, 6, LIPPUS_YELLOW);
                gfx_fill_rect(x + 5, y + 4, 6, 2, LIPPUS_YELLOW);
                gfx_fill_rect(x + 7, y + 8, 2, 2, LIPPUS_RGB(196, 140, 24));
            } else if (tile == T_POWER) {
                if ((g_frame & 16) == 0) {
                    gfx_fill_rect(x + 3, y + 7, 10, 5, LIPPUS_WHITE);
                    gfx_fill_rect(x + 4, y + 4, 8, 5, LIPPUS_RGB(198, 226, 255));
                }
            } else if (tile == T_DOOR) {
                gfx_fill_rect(x, y + TILE / 2 - 2, TILE, 4, LIPPUS_RGB(92, 58, 36));
            }
        }
    }
    if (g_fruit > 0) {
        int x = tile_x(g_fruit_c);
        int y = tile_y(g_fruit_r);
        gfx_fill_circle(x, y, 5, fruit_color());
        gfx_fill_rect(x - 1, y - 7, 2, 4, LIPPUS_RGB(80, 48, 16));
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

static void draw_hud(void) {
    char score[8];
    char level[4];
    format_u(score, (unsigned)g_score);
    format_u(level, (unsigned)g_level);
    gfx_text(8, 6, score, LIPPUS_CREAM, 1);
    gfx_text(gfx_width() - 8 - gfx_text_width(level, 1), 6, level, LIPPUS_AMBER, 1);
    int extras = g_lives - 1;
    if (extras < 0) {
        extras = 0;
    }
    for (int i = 0; i < extras; i++) {
        int x = 16 + i * 18;
        int y = gfx_height() - 14;
        gfx_fill_circle(x - 3, y - 4, 2, LIPPUS_RGB(120, 78, 46));
        gfx_fill_circle(x + 3, y - 4, 2, LIPPUS_RGB(120, 78, 46));
        gfx_fill_circle(x, y, 4, LIPPUS_RGB(176, 132, 86));
    }
}

static void draw_world(void) {
    int span = COLS * TILE;
    gfx_clear(LIPPUS_BLACK);
    draw_maze();
    for (int i = 0; i < 4; i++) {
        ghost_t *g = &g_ghosts[i];
        int asleep = g_fright > 0 && !g->eyes;
        uint16_t color = k_cat_col[i];
        if (asleep) {
            int flash = g_fright < g_flash_from && ((g_fright / 8) & 1);
            color = flash ? LIPPUS_WHITE : LIPPUS_RGB(96, 116, 168);
        }
        draw_cat(g, color, asleep);
        if (g->b.x - g_ox < TILE) {
            body_t copy = g->b;
            g->b.x += span;
            draw_cat(g, color, asleep);
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
        gfx_text((gfx_width() - gfx_text_width("VIA", 2)) / 2, g_oy + 5 * TILE, "VIA", LIPPUS_YELLOW, 2);
    } else if (g_state == ST_OVER) {
        gfx_text((gfx_width() - gfx_text_width("FINE", 2)) / 2, g_oy + 6 * TILE, "FINE", LIPPUS_RED, 2);
        gfx_text((gfx_width() - gfx_text_width("A ANCORA", 1)) / 2, g_oy + 8 * TILE, "A ANCORA",
                 LIPPUS_CREAM, 1);
    }
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
    load_map();
    enter_ready();
}

void lippus_main(void) {
    g_ox = (gfx_width() - COLS * TILE) / 2;
    g_oy = 24;
    new_game();
    for (;;) {
        lippus_pad_t pad = pad_poll();
        g_frame++;
        if (g_fruit_banner > 0) {
            g_fruit_banner--;
        }
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
                    g_state = ST_OVER;
                    quiet();
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
                int steps = take_pixels(&g_pac_acc, pac_pct(g_chewing));
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
