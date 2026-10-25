#include "lippus.h"

/* Labirinto in verticale. Lo stick resta quello della console:
   su e' sempre LIPPUS_UP. Gli sprite sono disegnati qui, non copiati. */

const uint32_t lippus_orientation = LIPPUS_PORTRAIT;

#define COLS 28
#define TILE 8
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

static const char *const k_rows[] = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "     #.##### ## #####.#     ",
    "     #.##          ##.#     ",
    "     #.## ###--### ##.#     ",
    "######.## #      # ##.######",
    "      .   #      #   .      ",
    "######.## #      # ##.######",
    "     #.## ######## ##.#     ",
    "     #.##          ##.#     ",
    "     #.## ######## ##.#     ",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#o..##................##..o#",
    "###.##.##.########.##.##.###",
    "#...##.##.########.##.##...#",
    "#.#####.##....##....##.#####",
    "#.#####.#####.##.#####.#####",
    "#o........................o#",
    "#.####.#####.##.#####.####.#",
    "############################",
};

#define ROWS (int)(sizeof k_rows / sizeof k_rows[0])

static const int8_t k_dx[4] = {0, -1, 0, 1};
static const int8_t k_dy[4] = {-1, 0, 1, 0};

/* Secondi di paura, lampeggi e pallini rimasti per Elroy. Dal livello 21 si ripete l'ultimo. */
static const uint8_t k_fright_s[21] = {
    6, 5, 4, 3, 2, 5, 2, 2, 1, 5, 2, 1, 1, 3, 1, 1, 0, 1, 0, 0, 0,
};
static const uint8_t k_fright_flash[21] = {
    5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 3, 3, 5, 3, 3, 0, 3, 0, 0, 0,
};
static const uint8_t k_elroy_dots[21] = {
    20, 30, 40, 40, 40, 50, 50, 50, 60, 60, 60, 80, 80, 80, 100, 100, 100, 100, 120, 120, 120,
};

static const uint16_t k_ghost_col[4] = {
    LIPPUS_RED,
    LIPPUS_RGB(255, 184, 222),
    LIPPUS_CYAN,
    LIPPUS_ORANGE,
};

static uint8_t g_map[31][COLS];
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
static int g_waka;
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
    return ((b->x - g_ox) & 7) == 4 && ((b->y - g_oy) & 7) == 4;
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
    g_door_c = 13;
    g_door_r = 12;
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
    g_fruit_r = g_door_r + 5;
    for (int r = g_door_r + 3; r < ROWS; r++) {
        if (g_map[r][g_door_c] == T_EMPTY) {
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

static int take_pixels(int *acc, int pct) {
    *acc += pct * 5;
    int n = *acc / 200;
    *acc -= n * 200;
    if (n > 3) {
        n = 3;
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

static int in_tunnel(int c, int r) { return r == 14 && (c < 6 || c > 21); }

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
    place(&g_pac, 13, 23, DIR_LEFT);
    if (g_map[23][13] == T_DOT || g_map[23][13] == T_POWER) {
        g_map[23][13] = T_EMPTY;
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

static int in_house(int c, int r) { return r == g_door_r + 1 && c >= g_door_c - 2 && c <= g_door_c + 2; }

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
            snd_voice(2, SND_TRIANGLE, 520, 180, 90);
        } else {
            snd_voice(2, SND_TRIANGLE, 280, 70, 50);
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
        snd_voice(2, SND_TRIANGLE, 740, 200, 120);
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
        if (dx * dx + dy * dy > 36) {
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
            g_eat_x = g->b.x - 8;
            g_eat_y = g->b.y - 10;
            g_state = ST_EAT;
            g_timer = 18;
            snd_voice(2, SND_SQUARE, (uint16_t)(700 + g_combo * 120), 200, 100);
            return;
        }
        g_state = ST_DEAD;
        g_timer = 55;
        g_lives--;
        quiet();
        {
            static const lippus_note_t death[] = {
                {620, 110}, {500, 110}, {370, 150}, {196, 280},
            };
            snd_tune(0, SND_TRIANGLE, 210, death, 4);
        }
        return;
    }
}

static void maybe_fruit(void) {
    int eaten = g_dots0 - g_dots;
    if (g_fruit == 0 && eaten == 70 && (g_fruit_mask & 1) == 0) {
        g_fruit = 9 * 30;
        g_fruit_mask |= 1;
    } else if (g_fruit == 0 && eaten == 170 && (g_fruit_mask & 2) == 0) {
        g_fruit = 9 * 30;
        g_fruit_mask |= 2;
    }
    if (g_fruit > 0) {
        g_fruit--;
    }
}

static void audio_play(int moved) {
    int eaten = g_dots0 - g_dots;
    int hz = 360;
    if (eaten > g_dots0 / 4) {
        hz = 440;
    }
    if (eaten > g_dots0 / 2) {
        hz = 520;
    }
    if (eaten > (g_dots0 * 3) / 4) {
        hz = 640;
    }
    if (g_fright > 0) {
        hz = (g_frame & 16) ? 240 : 190;
        snd_voice(0, SND_TRIANGLE, (unsigned)hz, 140, 0);
    } else {
        snd_voice(0, SND_SQUARE, (unsigned)hz, 90, 0);
    }
    if (moved && (g_frame % 5) == 0) {
        g_waka ^= 1;
        snd_voice(1, SND_SQUARE, g_waka ? 170 : 230, 45, 35);
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

static void draw_pac(int x, int y, int dir, int open) {
    gfx_fill_circle(x, y, 4, LIPPUS_YELLOW);
    if (open == 0 || dir < 0) {
        return;
    }
    int len = open == 1 ? 2 : 4;
    for (int i = 1; i <= len; i++) {
        int wide = i;
        if (k_dx[dir] != 0) {
            gfx_fill_rect(x + k_dx[dir] * i, y - wide, 1, wide * 2 + 1, LIPPUS_BLACK);
        } else {
            gfx_fill_rect(x - wide, y + k_dy[dir] * i, wide * 2 + 1, 1, LIPPUS_BLACK);
        }
    }
}

static void draw_ghost(const ghost_t *g, uint16_t color, int scared) {
    int x = g->b.x;
    int y = g->b.y;
    if (!g->eyes) {
        gfx_fill_circle(x, y - 1, 5, color);
        gfx_fill_rect(x - 5, y - 1, 11, 6, color);
        gfx_fill_rect(x - 5, y + 4, 3, 2, color);
        gfx_fill_rect(x - 1, y + 3, 3, 3, color);
        gfx_fill_rect(x + 3, y + 4, 3, 2, color);
    }
    int px = 0;
    int py = 0;
    if (!scared && g->b.dir >= 0) {
        px = k_dx[g->b.dir];
        py = k_dy[g->b.dir];
    }
    gfx_fill_circle(x - 2, y - 2, 2, LIPPUS_WHITE);
    gfx_fill_circle(x + 2, y - 2, 2, LIPPUS_WHITE);
    if (scared) {
        gfx_fill_rect(x - 4, y + 2, 2, 2, LIPPUS_WHITE);
        gfx_fill_rect(x + 2, y + 2, 2, 2, LIPPUS_WHITE);
    } else {
        gfx_fill_circle(x - 2 + px, y - 2 + py, 1, LIPPUS_BLUE);
        gfx_fill_circle(x + 2 + px, y - 2 + py, 1, LIPPUS_BLUE);
    }
}

static void draw_maze(void) {
    const uint16_t wall = LIPPUS_RGB(24, 24, 210);
    const uint16_t door = LIPPUS_RGB(255, 160, 200);
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
            gfx_fill_rect(g_ox + c0 * TILE, g_oy + r * TILE, (c - c0) * TILE, TILE, wall);
        }
        for (c = 0; c < COLS; c++) {
            int x = g_ox + c * TILE;
            int y = g_oy + r * TILE;
            uint8_t tile = g_map[r][c];
            if (tile == T_DOT) {
                gfx_fill_rect(x + 3, y + 3, 2, 2, LIPPUS_CREAM);
            } else if (tile == T_POWER) {
                if ((g_frame & 16) == 0) {
                    gfx_fill_circle(x + 4, y + 4, 3, LIPPUS_CREAM);
                }
            } else if (tile == T_DOOR) {
                gfx_fill_rect(x, y + 3, TILE, 2, door);
            }
        }
    }
    if (g_fruit > 0) {
        int x = tile_x(g_fruit_c);
        int y = tile_y(g_fruit_r);
        uint16_t color = fruit_color();
        if (g_level >= 11 && g_level <= 12) {
            gfx_fill_rect(x - 3, y - 2, 7, 6, color);
            gfx_fill_rect(x - 1, y - 5, 3, 3, color);
        } else if (g_level >= 13) {
            gfx_fill_rect(x - 1, y - 4, 2, 8, color);
            gfx_fill_rect(x - 3, y - 4, 5, 2, color);
        } else {
            gfx_fill_circle(x, y, g_level >= 7 ? 4 : 3, color);
            gfx_fill_rect(x, y - 6, 1, 3, LIPPUS_GREEN);
        }
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
        gfx_fill_circle(16 + i * 14, gfx_height() - 14, 4, LIPPUS_YELLOW);
    }
}

static void draw_world(void) {
    int span = COLS * TILE;
    gfx_clear(LIPPUS_BLACK);
    draw_maze();
    for (int i = 0; i < 4; i++) {
        ghost_t *g = &g_ghosts[i];
        int scared = g_fright > 0 && !g->eyes;
        uint16_t color = k_ghost_col[i];
        if (scared) {
            int flash = g_fright < g_flash_from && ((g_fright / 8) & 1);
            color = flash ? LIPPUS_WHITE : LIPPUS_RGB(40, 40, 200);
        }
        draw_ghost(g, color, scared);
        if (g->b.x - g_ox < 8) {
            body_t copy = g->b;
            g->b.x += span;
            draw_ghost(g, color, scared);
            g->b = copy;
        }
    }
    int open = g_pac.dir < 0 ? 0 : ((g_frame / 4) % 3);
    draw_pac(g_pac.x, g_pac.y, g_pac.dir, open);
    if (g_pac.x - g_ox < 8) {
        draw_pac(g_pac.x + span, g_pac.y, g_pac.dir, open);
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
        gfx_text((gfx_width() - gfx_text_width("PRONTO", 2)) / 2, g_oy + 17 * TILE, "PRONTO",
                 LIPPUS_YELLOW, 2);
    } else if (g_state == ST_OVER) {
        gfx_text((gfx_width() - gfx_text_width("FINE", 2)) / 2, g_oy + 14 * TILE, "FINE", LIPPUS_RED, 2);
        gfx_text((gfx_width() - gfx_text_width("A NUOVA", 1)) / 2, g_oy + 17 * TILE, "A NUOVA",
                 LIPPUS_CREAM, 1);
    }
}

static void enter_ready(void) {
    g_state = ST_READY;
    g_timer = 70;
    reset_actors();
    quiet();
    {
        static const lippus_note_t intro[] = {
            {392, 90}, {494, 90}, {587, 90}, {784, 180},
        };
        snd_tune(0, SND_SQUARE, 160, intro, 4);
    }
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
            int before_x = g_pac.x;
            int before_y = g_pac.y;
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
                snd_voice(0, SND_TRIANGLE, 880, 180, 200);
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
                        steps = 3;
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
                if (g_state == ST_PLAY) {
                    int moved = g_pac.x != before_x || g_pac.y != before_y;
                    audio_play(moved);
                }
            }
        }
        draw_world();
        gfx_present();
        sys_frame(33);
    }
}
