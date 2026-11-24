/* Scrive levels/campaign.txt, il file unico della campagna.
   Senza argomenti rigenera la campagna di serie e la incorpora.
   Con "pack" incorpora il testo gia' presente, senza toccare le stanze:
   e' il passo che usa un generatore esterno.
   cc -o /tmp/gen_levels gen_levels.c && /tmp/gen_levels
   Lancia da games/lippus_incursor. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAP 8
#define ZONES 45
#define MAXR 18

enum { D_NONE, D_CLOSED, D_LOCKED, D_OPEN };
enum { K_COMBAT, K_START, K_KEY, K_BOSS, K_LOOT };

static const int NX[4] = {0, 1, 0, -1};
static const int NY[4] = {-1, 0, 1, 0};
static const int OPP[4] = {2, 3, 0, 1};
static const char *const SHIP[15] = {
    "HOLD",    "CARGO", "HANGAR", "THRUST", "ENGINE", "DRIVE",   "LAB",    "MEDBAY",
    "BIOLAB",  "BARRACK", "ARMORY", "BUNKS", "CORE",  "NEXUS",   "BRIDGE",
};
static const char *const KIND[] = {"combat", "start", "key", "boss", "loot"};
static const char DOORCH[] = {'.', 'c', 'l', 'o'};

typedef struct {
    uint8_t on, kind, tmpl, dist, door[4];
} Room;

static uint32_t rng;
static Room map[64];
static int parent[64];
static int dist[64];
static uint8_t path[64];
static uint8_t q[64];
static int start_x, start_y;

static unsigned rnd(void) {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 16;
}

static int ri(int x, int y) { return y * MAP + x; }

static int inside(int x, int y) { return (unsigned)x < MAP && (unsigned)y < MAP; }

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
    int qh = 0, qt = 0, start = ri(sx, sy), i, far, best;
    for (i = 0; i < 64; i++) {
        parent[i] = -1;
        dist[i] = -1;
    }
    q[qt++] = (uint8_t)start;
    dist[start] = 0;
    parent[start] = start;
    while (qh < qt) {
        int id = q[qh++];
        int x = id % MAP;
        int y = id / MAP;
        int d;
        for (d = 0; d < 4; d++) {
            int nx, ny, n;
            if (!map[id].door[d]) {
                continue;
            }
            nx = x + NX[d];
            ny = y + NY[d];
            if (!inside(nx, ny)) {
                continue;
            }
            n = ri(nx, ny);
            if (dist[n] >= 0) {
                continue;
            }
            dist[n] = dist[id] + 1;
            parent[n] = id;
            q[qt++] = (uint8_t)n;
        }
    }
    far = start;
    best = 0;
    for (i = 0; i < 64; i++) {
        if (dist[i] > best) {
            best = dist[i];
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

static void gen_deck(uint32_t seed, int tier) {
    int sx = 2, sy = 5, sp = 0, count = 1, guard = 0;
    int stackx[64], stacky[64];
    int i, added, boss, prev, key, loot, far_d;
    rng = seed ? seed : 1;
    start_x = sx;
    start_y = sy;
    memset(map, 0, sizeof map);
    memset(path, 0, sizeof path);
    map[ri(sx, sy)].on = 1;
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
            if (!inside(nx, ny) || map[ri(nx, ny)].on) {
                continue;
            }
            map[ri(nx, ny)].on = 1;
            map[ri(x, y)].door[d] = D_CLOSED;
            map[ri(nx, ny)].door[OPP[d]] = D_CLOSED;
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
    prev = parent[boss];
    i = boss;
    path[ri(sx, sy)] = 1;
    for (guard = 0; i >= 0 && guard++ < 64; guard++) {
        path[i] = 1;
        if (i == ri(sx, sy)) {
            break;
        }
        i = parent[i];
    }
    if (prev >= 0 && prev != boss) {
        int bx = boss % MAP, by = boss / MAP;
        int px = prev % MAP, py = prev / MAP;
        int d = dir_between(bx, by, px, py);
        map[boss].door[d] = D_LOCKED;
        map[prev].door[OPP[d]] = D_LOCKED;
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
        if (!map[a].on || !map[b].on || map[a].door[d]) {
            continue;
        }
        if (a == boss || b == boss) {
            continue;
        }
        map[a].door[d] = D_CLOSED;
        map[b].door[OPP[d]] = D_CLOSED;
        added++;
    }
    bfs_far(sx, sy);
    for (i = 0; i < 64; i++) {
        if (map[i].on && dist[i] > 0) {
            map[i].dist = (uint8_t)dist[i];
        }
    }
    key = -1;
    far_d = -1;
    for (i = 0; i < 64; i++) {
        if (!map[i].on || path[i] || i == boss) {
            continue;
        }
        if (dist[i] > far_d) {
            far_d = dist[i];
            key = i;
        }
    }
    if (key < 0) {
        for (i = 0; i < 64; i++) {
            if (map[i].on && i != boss && i != ri(sx, sy)) {
                key = i;
                break;
            }
        }
    }
    loot = -1;
    for (i = 0; i < 64; i++) {
        if (!map[i].on || i == boss || i == ri(sx, sy) || i == key) {
            continue;
        }
        if (!path[i]) {
            loot = i;
            break;
        }
    }
    if (loot < 0) {
        for (i = 0; i < 64; i++) {
            if (map[i].on && i != boss && i != ri(sx, sy) && i != key) {
                loot = i;
                break;
            }
        }
    }
    for (i = 0; i < 64; i++) {
        int kind = K_COMBAT;
        if (!map[i].on) {
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
        map[i].kind = (uint8_t)kind;
        if (kind == K_START || kind == K_LOOT) {
            map[i].tmpl = 0;
        } else if (kind == K_BOSS) {
            map[i].tmpl = 6;
        } else if (kind == K_KEY) {
            map[i].tmpl = 7;
        } else if (tier == 0) {
            map[i].tmpl = (uint8_t)((rnd() & 1) ? 1 : 0);
        } else if (tier == 1) {
            static const uint8_t tab[4] = {1, 2, 3, 4};
            map[i].tmpl = tab[rnd() % 4];
        } else {
            static const uint8_t tab[4] = {1, 3, 5, 5};
            map[i].tmpl = tab[rnd() % 4];
        }
    }
}

static int tier_of(int zone) {
    int ship = zone / 3;
    if (ship < 5) {
        return 0;
    }
    if (ship < 10) {
        return 1;
    }
    return 2;
}

static void zone_name(int zone, char *out) {
    sprintf(out, "%s %d", SHIP[zone / 3], (zone % 3) + 1);
}

static int embed_campaign(void) {
    FILE *in = fopen("levels/campaign.txt", "rb");
    FILE *out = fopen("campaign.h", "w");
    int c;
    if (!in || !out) {
        perror("campaign");
        return 1;
    }
    fprintf(out, "#pragma once\n\n");
    fprintf(out, "/* Copia di levels/campaign.txt. Non modificare questo file:\n");
    fprintf(out, "   si rigenera con gen_levels pack. */\n\n");
    fprintf(out, "static const char INC_CAMPAIGN[] =\n");
    fputc('"', out);
    while ((c = fgetc(in)) != EOF) {
        if (c == '\\' || c == '"') {
            fputc('\\', out);
            fputc(c, out);
        } else if (c == '\n') {
            fprintf(out, "\\n\"\n\"");
        } else if (c != '\r') {
            fputc(c, out);
        }
    }
    fprintf(out, "\";\n");
    fclose(in);
    fclose(out);
    return 0;
}

static void write_spec(FILE *f) {
    fprintf(f, "# Incursor campaign format 1\n");
    fprintf(f, "# Un solo file per tutta la campagna. Un generatore esterno\n");
    fprintf(f, "# puo' riscriverlo: l'ordine dei blocchi e' l'ordine di gioco.\n");
    fprintf(f, "# Le righe che iniziano con # e le righe vuote si ignorano.\n");
    fprintf(f, "#\n");
    fprintf(f, "# format 1\n");
    fprintf(f, "# zones N                 quante zone seguono, da 1 a 64\n");
    fprintf(f, "#\n");
    fprintf(f, "# Ogni zona e' un blocco fra una riga \"zone\" e una riga \"end\".\n");
    fprintf(f, "# deck e area sono etichette. Il gioco segue l'ordine dei blocchi.\n");
    fprintf(f, "# name                    fino a 9 caratteri, quello che si legge a schermo\n");
    fprintf(f, "# seed                    numero intero, fissa i nemici di ogni stanza\n");
    fprintf(f, "# start X Y               prima stanza, griglia 0..7\n");
    fprintf(f, "# room X Y kind tmpl dist NESW\n");
    fprintf(f, "#\n");
    fprintf(f, "# kind: combat start key boss loot\n");
    fprintf(f, "# tmpl: 0..7, uno degli otto disegni di stanza del gioco\n");
    fprintf(f, "# dist: passi dalla partenza, 0 nella stanza iniziale\n");
    fprintf(f, "# NESW, nord est sud ovest, un carattere ciascuno:\n");
    fprintf(f, "#   . nessuna porta\n");
    fprintf(f, "#   c porta chiusa\n");
    fprintf(f, "#   l porta chiusa a chiave\n");
    fprintf(f, "#   o porta aperta\n");
    fprintf(f, "# La porta va scritta uguale dall'altro lato.\n");
    fprintf(f, "# Servono una stanza start, una key e una boss.\n");
    fprintf(f, "# Al massimo 18 stanze per zona, al massimo 64 zone.\n");
    fprintf(f, "\n");
}

int main(int argc, char **argv) {
    FILE *f;
    int z, i, total = 0;
    int pack_only = argc > 1 && argv[1][0] == 'p';
    if (pack_only) {
        return embed_campaign();
    }
    f = fopen("levels/campaign.txt", "w");
    if (!f) {
        perror("levels/campaign.txt");
        return 1;
    }
    write_spec(f);
    fprintf(f, "format 1\n");
    fprintf(f, "zones %d\n\n", ZONES);
    for (z = 0; z < ZONES; z++) {
        char name[16];
        int n = 0;
        int has_boss = 0, has_key = 0, has_start = 0;
        uint32_t seed = 0xA5000001u + (uint32_t)z * 0x00010001u;
        zone_name(z, name);
        gen_deck(seed, tier_of(z));
        fprintf(f, "zone\n");
        fprintf(f, "deck %d\n", (z / 3) + 1);
        fprintf(f, "area %d\n", (z % 3) + 1);
        fprintf(f, "name %s\n", name);
        fprintf(f, "seed %u\n", seed);
        fprintf(f, "start %d %d\n", start_x, start_y);
        for (i = 0; i < 64; i++) {
            int d;
            int x = i % MAP;
            int y = i / MAP;
            if (!map[i].on) {
                continue;
            }
            if (n >= MAXR) {
                fprintf(stderr, "zona %d troppe stanze\n", z);
                return 1;
            }
            fprintf(f, "room %d %d %s %u %u ", x, y, KIND[map[i].kind], map[i].tmpl, map[i].dist);
            for (d = 0; d < 4; d++) {
                fputc(DOORCH[map[i].door[d]], f);
            }
            fputc('\n', f);
            if (map[i].kind == K_BOSS) {
                has_boss = 1;
            }
            if (map[i].kind == K_KEY) {
                has_key = 1;
            }
            if (map[i].kind == K_START) {
                has_start = 1;
            }
            n++;
        }
        fprintf(f, "end\n\n");
        if (!has_boss || !has_key || !has_start || n < 4) {
            fprintf(stderr, "zona %d incompleta stanze %d\n", z, n);
            return 1;
        }
        total += n;
        printf("%s  %2d stanze\n", name, n);
    }
    fclose(f);
    printf("stanze totali %d\n", total);
    return embed_campaign();
}
