#pragma once

/* Testo di una zona, una stanza alla volta dentro lo stesso file.
   Righe di testa, poi una o piu' stanze:

     deck 1
     zone 1
     name CARGO
     warp 4
     exit 0
     secret 3
     room 2 5
     ####################
     ... 14 righe da 20 ...

   Alfabeto, senza toccare i simboli dei template gia' nel gioco:
     # muro   . pavimento   % crepa   & porta segreta
     A F D O  porte ciano, gialla, rossa, viola
     E uscita di zona    W scorciatoia di ponte
     G griglia    V sfiatatoio
     c cassa da spingere    b cassa che si rompe    B barile
     $ nascondiglio    k j q o chiavi a terra
     1..8 pedana, canale 0..7
     * terminale, canale 8, resta premuto
     x bersaglio, canale 9, resta premuto
     @ inizio
   exit e secret sono le maschere dei canali. // commenta, fuori dalla mappa. */

#include "world.h"

static inline int inc_span(const char *s) {
    int n = 0;
    if (!s) {
        return 0;
    }
    while (s[n] && s[n] != '\n' && s[n] != '\r') {
        n++;
    }
    return n;
}

static inline int inc_eq(const char *s, const char *w) {
    int i = 0;
    while (w[i]) {
        if (s[i] != w[i]) {
            return 0;
        }
        i++;
    }
    return s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == 0;
}

static inline int inc_num(const char *s, int *used) {
    int n = 0;
    int i = 0;
    while (s[i] == ' ') {
        i++;
    }
    while (s[i] >= '0' && s[i] <= '9') {
        n = n * 10 + (s[i] - '0');
        i++;
    }
    *used = i;
    return n;
}

static inline int inc_add_prop(IncRoom *r, uint8_t type, uint8_t loot, int tx, int ty, int hp,
                               int secret) {
    IncProp *p;
    if (r->prop_n >= INC_PROPS) {
        return 0;
    }
    p = &r->props[r->prop_n++];
    p->type = type;
    p->loot = loot;
    p->secret = (uint8_t)secret;
    p->on = 1;
    p->x = (int16_t)(tx * INC_TILE + 8);
    p->y = (int16_t)(ty * INC_TILE + 8);
    p->hp = (int16_t)hp;
    return 1;
}

static inline int inc_add_trig(IncRoom *r, uint8_t type, int tx, int ty, int channel, int latch) {
    IncTrig *t;
    if (r->trig_n >= INC_TRIGS || channel < 0 || channel > 15) {
        return 0;
    }
    t = &r->trigs[r->trig_n++];
    t->type = type;
    t->tx = (uint8_t)tx;
    t->ty = (uint8_t)ty;
    t->channel = (uint8_t)channel;
    t->latch = (uint8_t)latch;
    t->on = 0;
    return 1;
}

static inline void inc_paint(IncRoom *r, int x, int y, char ch) {
    uint8_t tile = INC_FLOOR;
    if (ch == '#') {
        tile = INC_WALL;
    } else if (ch == '%') {
        tile = INC_CRACK;
    } else if (ch == '&') {
        tile = INC_SECRET;
    } else if (ch == 'A') {
        tile = INC_DOOR_CYAN;
    } else if (ch == 'F') {
        tile = INC_DOOR_YELLOW;
    } else if (ch == 'D') {
        tile = INC_DOOR_RED;
    } else if (ch == 'O') {
        tile = INC_DOOR_VIOLET;
    } else if (ch == 'E') {
        tile = INC_EXIT;
    } else if (ch == 'W') {
        tile = INC_WARP;
    } else if (ch == 'G') {
        tile = INC_GRID;
    } else if (ch == 'V') {
        tile = INC_VENT;
    } else if (ch == '@') {
        r->spawn_tx = (uint8_t)x;
        r->spawn_ty = (uint8_t)y;
    } else if (ch == 'c') {
        inc_add_prop(r, INC_PROP_PUSH, INC_LOOT_NONE, x, y, 9999, 0);
    } else if (ch == 'b') {
        inc_add_prop(r, INC_PROP_BREAK, INC_LOOT_FUS, x, y, 15, 0);
    } else if (ch == 'B') {
        inc_add_prop(r, INC_PROP_BARREL, INC_LOOT_NONE, x, y, 10, 0);
    } else if (ch == '$') {
        inc_add_prop(r, INC_PROP_CACHE, INC_LOOT_HP, x, y, 30, 1);
    } else if (ch == 'k') {
        inc_add_prop(r, INC_PROP_KEY, INC_LOOT_KEY_C, x, y, 1, 0);
    } else if (ch == 'j') {
        inc_add_prop(r, INC_PROP_KEY, INC_LOOT_KEY_Y, x, y, 1, 0);
    } else if (ch == 'q') {
        inc_add_prop(r, INC_PROP_KEY, INC_LOOT_KEY_R, x, y, 1, 0);
    } else if (ch == 'o') {
        inc_add_prop(r, INC_PROP_KEY, INC_LOOT_KEY_V, x, y, 1, 0);
    } else if (ch >= '1' && ch <= '8') {
        inc_add_trig(r, INC_TRIG_PLATE, x, y, ch - '1', 0);
    } else if (ch == '*') {
        inc_add_trig(r, INC_TRIG_TERM, x, y, 8, 1);
    } else if (ch == 'x') {
        inc_add_trig(r, INC_TRIG_TARGET, x, y, 9, 1);
    }
    r->tiles[y][x] = tile;
}

/* 0 se la zona e' leggibile. Le stanze oltre INC_ROOMS si ignorano. */
static inline int inc_parse(IncZone *z, const char *text) {
    int i = 0;
    int row = -1;
    IncRoom *room = 0;
    if (!z || !text) {
        return -1;
    }
    z->deck = 1;
    z->zone = 1;
    z->warp_deck = 0;
    z->room_n = 0;
    z->exit_mask = 0;
    z->secret_mask = 0;
    z->puzzle = 0;
    z->keys = 0;
    z->secrets_total = 0;
    z->secrets_found = 0;
    z->name[0] = 0;
    while (text[i]) {
        int len;
        const char *line;
        while (text[i] == '\n' || text[i] == '\r') {
            i++;
        }
        if (!text[i]) {
            break;
        }
        line = text + i;
        len = inc_span(line);
        if (row >= 0 && room) {
            int x;
            int n = len > INC_COLS ? INC_COLS : len;
            if (len > 0 && line[0] != '/') {
                for (x = 0; x < INC_COLS; x++) {
                    char ch = x < n ? line[x] : '.';
                    inc_paint(room, x, row, ch);
                }
                row++;
                if (row >= INC_ROWS) {
                    row = -1;
                    room = 0;
                }
            }
        } else if (line[0] == '/' && line[1] == '/') {
            /* commento */
        } else if (inc_eq(line, "deck")) {
            int used = 0;
            z->deck = (uint8_t)inc_num(line + 4, &used);
        } else if (inc_eq(line, "zone")) {
            int used = 0;
            z->zone = (uint8_t)inc_num(line + 4, &used);
        } else if (inc_eq(line, "warp")) {
            int used = 0;
            z->warp_deck = (uint8_t)inc_num(line + 4, &used);
        } else if (inc_eq(line, "exit")) {
            int used = 0;
            z->exit_mask = (uint16_t)inc_num(line + 4, &used);
        } else if (inc_eq(line, "secret")) {
            int used = 0;
            z->secret_mask = (uint16_t)inc_num(line + 6, &used);
        } else if (inc_eq(line, "name")) {
            int k = 0;
            int p = 5;
            while (line[p] && line[p] != '\n' && line[p] != '\r' && k < 7) {
                z->name[k++] = line[p++];
            }
            z->name[k] = 0;
        } else if (inc_eq(line, "room") && z->room_n < INC_ROOMS) {
            int used = 0;
            int gx = inc_num(line + 4, &used);
            int gy = inc_num(line + 4 + used, &used);
            int y, x;
            room = &z->rooms[z->room_n++];
            room->prop_n = 0;
            room->trig_n = 0;
            room->gx = (uint8_t)gx;
            room->gy = (uint8_t)gy;
            room->spawn_tx = 1;
            room->spawn_ty = 1;
            for (y = 0; y < INC_ROWS; y++) {
                for (x = 0; x < INC_COLS; x++) {
                    room->tiles[y][x] = INC_FLOOR;
                }
            }
            row = 0;
        }
        i += len;
        if (text[i] == '\r') {
            i++;
        }
        if (text[i] == '\n') {
            i++;
        }
    }
    if (z->room_n == 0) {
        return -1;
    }
    if (z->deck < 1) {
        z->deck = 1;
    }
    if (z->deck > INC_DECKS) {
        z->deck = INC_DECKS;
    }
    if (z->zone < 1) {
        z->zone = 1;
    }
    if (z->zone > INC_ZONES) {
        z->zone = INC_ZONES;
    }
    for (i = 0; i < z->room_n; i++) {
        int y, x, p;
        IncRoom *rm = &z->rooms[i];
        for (y = 0; y < INC_ROWS; y++) {
            for (x = 0; x < INC_COLS; x++) {
                if (rm->tiles[y][x] == INC_CRACK && z->secrets_total < 255) {
                    z->secrets_total++;
                }
            }
        }
        for (p = 0; p < rm->prop_n; p++) {
            if (rm->props[p].secret && z->secrets_total < 255) {
                z->secrets_total++;
            }
        }
    }
    return 0;
}
