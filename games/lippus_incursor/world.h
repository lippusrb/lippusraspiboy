#pragma once

/* Oggetti della nave, sopra il gioco com'e' adesso.
   Non e' collegato a main.c: sparo, mira, nemici e stanze restano quelli.
   Una zona tiene le stanze del ponte (fino a 18, come il generatore).
   IncZone pesa circa 9 KB: va in RAM statica, non nello stack da 4 KB.
   Il file di testo sta in /GAMES/incursor/ sulla SD; il firmware di oggi
   non sa ancora aprirlo, il parser legge un buffer gia' in memoria. */

#include <stdint.h>

#define INC_COLS 20
#define INC_ROWS 14
#define INC_TILE 16
#define INC_ROOMS 18
#define INC_PROPS 12
#define INC_TRIGS 8
#define INC_DECKS 15
#define INC_ZONES 3
#define INC_EV 16

enum {
    INC_FLOOR = 0,
    INC_WALL = 1,
    INC_CRACK = 2,
    INC_SECRET = 3,
    INC_DOOR_CYAN = 4,
    INC_DOOR_YELLOW = 5,
    INC_DOOR_RED = 6,
    INC_DOOR_VIOLET = 7,
    INC_EXIT = 8,
    INC_WARP = 9,
    INC_GRID = 10,
    INC_VENT = 11
};

enum {
    INC_KEY_CYAN = 1 << 0,
    INC_KEY_YELLOW = 1 << 1,
    INC_KEY_RED = 1 << 2,
    INC_KEY_VIOLET = 1 << 3
};

enum {
    INC_PROP_PUSH = 1,
    INC_PROP_BREAK = 2,
    INC_PROP_BARREL = 3,
    INC_PROP_CACHE = 4,
    INC_PROP_KEY = 5
};

enum {
    INC_LOOT_NONE = 0,
    INC_LOOT_MED = 1,
    INC_LOOT_CELL = 2,
    INC_LOOT_FUS = 3,
    INC_LOOT_YEL = 4,
    INC_LOOT_RED = 5,
    INC_LOOT_KEY_C = 6,
    INC_LOOT_KEY_Y = 7,
    INC_LOOT_KEY_R = 8,
    INC_LOOT_KEY_V = 9,
    INC_LOOT_GUN = 10,
    INC_LOOT_HP = 11,
    INC_LOOT_SP = 12
};

enum {
    INC_TRIG_PLATE = 1,
    INC_TRIG_TERM = 2,
    INC_TRIG_TARGET = 3
};

enum {
    INC_EV_NONE = 0,
    INC_EV_BOOM = 1,
    INC_EV_LOOT = 2,
    INC_EV_SECRET = 3,
    INC_EV_OPEN = 4
};

typedef struct {
    uint8_t type;
    uint8_t loot;
    uint8_t secret;
    uint8_t on;
    int16_t x, y;
    int16_t hp;
} IncProp;

typedef struct {
    uint8_t type;
    uint8_t tx, ty;
    uint8_t channel;
    uint8_t latch;
    uint8_t on;
} IncTrig;

typedef struct {
    uint8_t tiles[INC_ROWS][INC_COLS];
    IncProp props[INC_PROPS];
    IncTrig trigs[INC_TRIGS];
    uint8_t prop_n;
    uint8_t trig_n;
    uint8_t gx, gy;
    uint8_t spawn_tx, spawn_ty;
} IncRoom;

typedef struct {
    uint8_t deck;
    uint8_t zone;
    uint8_t warp_deck;
    uint8_t room_n;
    uint16_t exit_mask;
    uint16_t secret_mask;
    uint16_t puzzle;
    uint8_t keys;
    uint8_t secrets_total;
    uint8_t secrets_found;
    char name[8];
    IncRoom rooms[INC_ROOMS];
} IncZone;

typedef struct {
    uint8_t kind;
    uint8_t loot;
    int16_t x, y;
    int16_t a;
} IncEvent;

static inline int inc_solid(uint8_t t) {
    return t == INC_WALL || t == INC_CRACK || t == INC_SECRET || t == INC_DOOR_CYAN ||
           t == INC_DOOR_YELLOW || t == INC_DOOR_RED || t == INC_DOOR_VIOLET;
}

static inline int inc_door_bit(uint8_t t) {
    if (t == INC_DOOR_CYAN) {
        return INC_KEY_CYAN;
    }
    if (t == INC_DOOR_YELLOW) {
        return INC_KEY_YELLOW;
    }
    if (t == INC_DOOR_RED) {
        return INC_KEY_RED;
    }
    if (t == INC_DOOR_VIOLET) {
        return INC_KEY_VIOLET;
    }
    return 0;
}

static inline int inc_blocked(const IncRoom *r, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= INC_COLS || ty >= INC_ROWS) {
        return 1;
    }
    return inc_solid(r->tiles[ty][tx]);
}

static inline int inc_prop_at(const IncRoom *r, int x, int y, int half) {
    int i;
    for (i = 0; i < r->prop_n; i++) {
        int dx, dy;
        if (!r->props[i].on || r->props[i].type == INC_PROP_KEY) {
            continue;
        }
        dx = r->props[i].x - x;
        dy = r->props[i].y - y;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        if (dx <= half && dy <= half) {
            return i;
        }
    }
    return -1;
}

static inline int inc_push(IncRoom *r, int idx, int dir_x, int dir_y) {
    IncProp *p;
    int nx, ny, tx, ty, other;
    if (idx < 0 || idx >= r->prop_n) {
        return 0;
    }
    p = &r->props[idx];
    if (!p->on || p->type != INC_PROP_PUSH) {
        return 0;
    }
    nx = p->x + dir_x * INC_TILE;
    ny = p->y + dir_y * INC_TILE;
    tx = nx / INC_TILE;
    ty = ny / INC_TILE;
    if (inc_blocked(r, tx, ty)) {
        return 0;
    }
    other = inc_prop_at(r, tx * INC_TILE + 8, ty * INC_TILE + 8, 12);
    if (other >= 0 && other != idx) {
        return 0;
    }
    p->x = (int16_t)(tx * INC_TILE + 8);
    p->y = (int16_t)(ty * INC_TILE + 8);
    return 1;
}

static inline int inc_emit(IncEvent *ev, int n, int cap, uint8_t kind, int x, int y, int a,
                           uint8_t loot) {
    if (!ev || n >= cap) {
        return n;
    }
    ev[n].kind = kind;
    ev[n].loot = loot;
    ev[n].x = (int16_t)x;
    ev[n].y = (int16_t)y;
    ev[n].a = (int16_t)a;
    return n + 1;
}

/* Le casse da spingere non si rompono. La catena dei barili sta in coda. */
static inline int inc_damage(IncZone *z, IncRoom *r, int idx, int dmg, IncEvent *ev, int cap) {
    int q[INC_PROPS];
    uint8_t seen[INC_PROPS];
    int qn = 0;
    int qr = 0;
    int n = 0;
    int i;
    if (!z || !r || idx < 0 || idx >= r->prop_n) {
        return 0;
    }
    for (i = 0; i < INC_PROPS; i++) {
        seen[i] = 0;
    }
    q[qn++] = idx;
    seen[idx] = 1;
    while (qr < qn) {
        IncProp *p = &r->props[q[qr++]];
        int bx, by, tx, ty, oy, ox;
        if (!p->on || p->type == INC_PROP_PUSH || p->type == INC_PROP_KEY) {
            continue;
        }
        p->hp = (int16_t)(p->hp - dmg);
        if (p->hp > 0) {
            continue;
        }
        p->on = 0;
        bx = p->x;
        by = p->y;
        if (p->secret && z->secrets_found < 255) {
            z->secrets_found++;
            n = inc_emit(ev, n, cap, INC_EV_SECRET, bx, by, z->secrets_found, 0);
        }
        if (p->type == INC_PROP_BARREL) {
            n = inc_emit(ev, n, cap, INC_EV_BOOM, bx, by, 38, 0);
            tx = bx / INC_TILE;
            ty = by / INC_TILE;
            for (oy = -1; oy <= 1; oy++) {
                for (ox = -1; ox <= 1; ox++) {
                    int cx = tx + ox;
                    int cy = ty + oy;
                    if (cx < 0 || cy < 0 || cx >= INC_COLS || cy >= INC_ROWS) {
                        continue;
                    }
                    if (r->tiles[cy][cx] == INC_CRACK) {
                        r->tiles[cy][cx] = INC_FLOOR;
                        if (z->secrets_found < 255) {
                            z->secrets_found++;
                        }
                        n = inc_emit(ev, n, cap, INC_EV_OPEN, cx * INC_TILE + 8, cy * INC_TILE + 8,
                                     0, 0);
                    }
                }
            }
            for (i = 0; i < r->prop_n; i++) {
                int dx, dy;
                if (!r->props[i].on || seen[i]) {
                    continue;
                }
                dx = r->props[i].x - bx;
                dy = r->props[i].y - by;
                if (dx * dx + dy * dy <= 34 * 34 && qn < INC_PROPS) {
                    seen[i] = 1;
                    q[qn++] = i;
                }
            }
        }
        if (p->loot != INC_LOOT_NONE) {
            n = inc_emit(ev, n, cap, INC_EV_LOOT, bx, by, 0, p->loot);
        }
    }
    return n;
}

static inline void inc_refresh_plates(IncZone *z, IncRoom *r, int player_tx, int player_ty) {
    int i;
    if (!z || !r) {
        return;
    }
    for (i = 0; i < r->trig_n; i++) {
        IncTrig *t = &r->trigs[i];
        int covered = 0;
        int p;
        if (t->type != INC_TRIG_PLATE) {
            continue;
        }
        if (player_tx == t->tx && player_ty == t->ty) {
            covered = 1;
        }
        for (p = 0; p < r->prop_n; p++) {
            int tx, ty;
            if (!r->props[p].on || r->props[p].type != INC_PROP_PUSH) {
                continue;
            }
            tx = r->props[p].x / INC_TILE;
            ty = r->props[p].y / INC_TILE;
            if (tx == t->tx && ty == t->ty) {
                covered = 1;
            }
        }
        if (t->latch) {
            if (covered) {
                t->on = 1;
            }
        } else {
            t->on = (uint8_t)covered;
        }
        if (t->on && t->channel < 16) {
            z->puzzle = (uint16_t)(z->puzzle | (1u << t->channel));
        } else if (!t->latch && t->channel < 16) {
            z->puzzle = (uint16_t)(z->puzzle & ~(1u << t->channel));
        }
    }
}

static inline int inc_use_term(IncZone *z, IncRoom *r, int tx, int ty) {
    int i;
    for (i = 0; i < r->trig_n; i++) {
        IncTrig *t = &r->trigs[i];
        if (t->type == INC_TRIG_TERM && t->tx == tx && t->ty == ty) {
            t->on = 1;
            if (t->channel < 16) {
                z->puzzle = (uint16_t)(z->puzzle | (1u << t->channel));
            }
            return 1;
        }
    }
    return 0;
}

static inline int inc_shoot_target(IncZone *z, IncRoom *r, int tx, int ty) {
    int i;
    for (i = 0; i < r->trig_n; i++) {
        IncTrig *t = &r->trigs[i];
        if (t->type == INC_TRIG_TARGET && t->tx == tx && t->ty == ty) {
            t->on = 1;
            if (t->channel < 16) {
                z->puzzle = (uint16_t)(z->puzzle | (1u << t->channel));
            }
            return 1;
        }
    }
    return 0;
}

/* Apre la porta colorata se la chiave c'e'. 1 se il tile e' diventato pavimento. */
static inline int inc_try_door(IncZone *z, IncRoom *r, int tx, int ty) {
    uint8_t t;
    int bit;
    if (tx < 0 || ty < 0 || tx >= INC_COLS || ty >= INC_ROWS) {
        return 0;
    }
    t = r->tiles[ty][tx];
    bit = inc_door_bit(t);
    if (!bit || (z->keys & bit) == 0) {
        return 0;
    }
    r->tiles[ty][tx] = INC_FLOOR;
    return 1;
}

static inline void inc_take_keys(IncZone *z, IncRoom *r, int x, int y) {
    int i;
    for (i = 0; i < r->prop_n; i++) {
        IncProp *p = &r->props[i];
        int dx, dy;
        if (!p->on || p->type != INC_PROP_KEY) {
            continue;
        }
        dx = p->x - x;
        dy = p->y - y;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        if (dx > 10 || dy > 10) {
            continue;
        }
        if (p->loot == INC_LOOT_KEY_C) {
            z->keys = (uint8_t)(z->keys | INC_KEY_CYAN);
        } else if (p->loot == INC_LOOT_KEY_Y) {
            z->keys = (uint8_t)(z->keys | INC_KEY_YELLOW);
        } else if (p->loot == INC_LOOT_KEY_R) {
            z->keys = (uint8_t)(z->keys | INC_KEY_RED);
        } else if (p->loot == INC_LOOT_KEY_V) {
            z->keys = (uint8_t)(z->keys | INC_KEY_VIOLET);
        }
        p->on = 0;
    }
}

static inline int inc_exit_open(const IncZone *z) {
    return (z->puzzle & z->exit_mask) == z->exit_mask;
}

static inline int inc_secret_open(const IncZone *z) {
    return (z->puzzle & z->secret_mask) == z->secret_mask;
}

/* La paratia segreta diventa pavimento quando i canali richiesti sono accesi. */
static inline void inc_open_secrets(IncZone *z, IncRoom *r) {
    int y, x;
    if (!inc_secret_open(z)) {
        return;
    }
    for (y = 0; y < INC_ROWS; y++) {
        for (x = 0; x < INC_COLS; x++) {
            if (r->tiles[y][x] == INC_SECRET) {
                r->tiles[y][x] = INC_FLOOR;
            }
        }
    }
}

/* Colpo diretto sulla crepa. Il barile che esplode vicino fa lo stesso. */
static inline int inc_shoot_wall(IncZone *z, IncRoom *r, int tx, int ty, IncEvent *ev, int cap) {
    if (tx < 0 || ty < 0 || tx >= INC_COLS || ty >= INC_ROWS) {
        return 0;
    }
    if (r->tiles[ty][tx] != INC_CRACK) {
        return 0;
    }
    r->tiles[ty][tx] = INC_FLOOR;
    if (z->secrets_found < 255) {
        z->secrets_found++;
    }
    return inc_emit(ev, 0, cap, INC_EV_OPEN, tx * INC_TILE + 8, ty * INC_TILE + 8, 0, 0);
}

/* Indici della palette gia' usata dal gioco. 0 e' il buco. */
static const uint8_t INC_SPR_PUSH[16][16] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1},
    {1, 5, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 5, 1},
    {1, 5, 3, 16, 16, 3, 3, 3, 3, 3, 3, 16, 16, 3, 5, 1},
    {1, 5, 3, 16, 17, 16, 3, 3, 3, 3, 16, 17, 16, 3, 5, 1},
    {1, 5, 3, 3, 16, 17, 16, 3, 3, 16, 17, 16, 3, 3, 5, 1},
    {1, 5, 3, 3, 3, 16, 17, 17, 17, 17, 16, 3, 3, 3, 5, 1},
    {1, 5, 3, 3, 3, 3, 17, 6, 6, 17, 3, 3, 3, 3, 5, 1},
    {1, 5, 3, 3, 3, 3, 17, 6, 6, 17, 3, 3, 3, 3, 5, 1},
    {1, 5, 3, 3, 3, 16, 17, 17, 17, 17, 16, 3, 3, 3, 5, 1},
    {1, 5, 3, 3, 16, 17, 16, 3, 3, 16, 17, 16, 3, 3, 5, 1},
    {1, 5, 3, 16, 17, 16, 3, 3, 3, 3, 16, 17, 16, 3, 5, 1},
    {1, 5, 3, 16, 16, 3, 3, 3, 3, 3, 3, 16, 16, 3, 5, 1},
    {1, 5, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 5, 1},
    {1, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

static const uint8_t INC_SPR_BARREL[16][16] = {
    {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
    {0, 0, 1, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1, 0, 0},
    {0, 0, 1, 12, 13, 13, 13, 13, 13, 13, 13, 13, 12, 1, 0, 0},
    {0, 0, 1, 12, 13, 14, 14, 14, 14, 14, 14, 13, 12, 1, 0, 0},
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
    {0, 0, 1, 12, 13, 14, 17, 17, 17, 17, 14, 13, 12, 1, 0, 0},
    {0, 0, 1, 12, 13, 17, 17, 6, 6, 17, 17, 13, 12, 1, 0, 0},
    {0, 0, 1, 12, 13, 17, 6, 6, 6, 6, 17, 13, 12, 1, 0, 0},
    {0, 0, 1, 12, 13, 17, 17, 6, 6, 17, 17, 13, 12, 1, 0, 0},
    {0, 0, 1, 12, 13, 14, 17, 17, 17, 17, 14, 13, 12, 1, 0, 0},
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
    {0, 0, 1, 12, 13, 14, 14, 14, 14, 14, 14, 13, 12, 1, 0, 0},
    {0, 0, 1, 12, 13, 13, 13, 13, 13, 13, 13, 13, 12, 1, 0, 0},
    {0, 0, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 0, 0},
    {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const uint8_t INC_SPR_BREAK[16][16] = {
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {1, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 1},
    {1, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 16, 1},
    {1, 16, 17, 1, 1, 17, 17, 17, 17, 17, 17, 1, 1, 17, 16, 1},
    {1, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 16, 1},
    {1, 1, 1, 1, 1, 1, 24, 25, 25, 24, 1, 1, 1, 1, 1, 1},
    {1, 24, 25, 25, 25, 25, 25, 26, 26, 25, 25, 25, 25, 25, 24, 1},
    {1, 24, 25, 25, 25, 25, 26, 6, 6, 26, 25, 25, 25, 25, 24, 1},
    {1, 24, 25, 25, 25, 25, 25, 26, 26, 25, 25, 25, 25, 25, 24, 1},
    {1, 1, 1, 1, 1, 1, 24, 25, 25, 24, 1, 1, 1, 1, 1, 1},
    {1, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 16, 1},
    {1, 16, 17, 1, 1, 17, 17, 17, 17, 17, 17, 1, 1, 17, 16, 1},
    {1, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 16, 1},
    {1, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 1},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const uint8_t INC_SPR_PLATE[16][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 0},
    {0, 1, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1, 0},
    {0, 1, 3, 5, 16, 16, 5, 5, 5, 5, 16, 16, 5, 3, 1, 0},
    {0, 1, 3, 5, 16, 17, 5, 5, 5, 5, 17, 16, 5, 3, 1, 0},
    {0, 1, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1, 0},
    {0, 1, 3, 5, 5, 5, 5, 25, 25, 5, 5, 5, 5, 3, 1, 0},
    {0, 1, 3, 5, 5, 5, 5, 25, 25, 5, 5, 5, 5, 3, 1, 0},
    {0, 1, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1, 0},
    {0, 1, 3, 5, 16, 17, 5, 5, 5, 5, 17, 16, 5, 3, 1, 0},
    {0, 1, 3, 5, 16, 16, 5, 5, 5, 5, 16, 16, 5, 3, 1, 0},
    {0, 1, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1, 0},
    {0, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
