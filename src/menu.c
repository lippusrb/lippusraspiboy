#include "menu.h"

#include "os.h"

#include "pico/stdlib.h"

#define COL_INK LIPPUS_RGB(245, 236, 220)
#define COL_AMBER LIPPUS_RGB(255, 196, 72)
#define COL_PANEL LIPPUS_RGB(36, 26, 64)
#define COL_INK_DARK LIPPUS_RGB(32, 18, 36)
#define COL_MUTED LIPPUS_RGB(168, 156, 190)
#define COL_BAD LIPPUS_RGB(255, 120, 120)

/* Preferenza del menu: SELECT la inverte. I giochi usano il loro flag. */
static unsigned s_orient = LIPPUS_LANDSCAPE;

void menu_restore_orientation(void) {
    lippus_gfx_orientation(s_orient);
}

static void menu_apply_orientation(unsigned mode) {
    s_orient = mode == LIPPUS_PORTRAIT ? LIPPUS_PORTRAIT : LIPPUS_LANDSCAPE;
    lippus_gfx_orientation(s_orient);
}

static int screen_w(void) { return lippus_gfx_width(); }

static int screen_h(void) { return lippus_gfx_height(); }

static int menu_visible_rows(void) {
    const int header = 42;
    const int footer = 26;
    const int pitch = 32;
    int rows = (screen_h() - header - footer) / pitch;
    if (rows < 3) {
        rows = 3;
    }
    if (rows > 8) {
        rows = 8;
    }
    return rows;
}

static void backdrop(void) {
    int h = screen_h();
    int w = screen_w();
    for (int y = 0; y < h; y++) {
        int r = 14 + (40 * y) / h;
        int g = 8 + (16 * y) / h;
        int b = 28 + (36 * y) / h;
        lippus_gfx_fill_rect(0, y, w, 1, LIPPUS_RGB(r, g, b));
    }
}

static void center_text(int y, const char *text, uint16_t color, int scale) {
    int width = lippus_gfx_text_width(text, scale);
    int x = (screen_w() - width) / 2;
    if (x < 4) {
        x = 4;
    }
    lippus_gfx_text(x, y, text, color, scale);
}

static void shorten(char *dst, const char *src, int max_chars) {
    int i = 0;
    if (!src) {
        src = "";
    }
    for (; i < max_chars && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static int wr_u(char *dst, unsigned value) {
    char tmp[10];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && n < 10);
    int out = 0;
    while (n > 0) {
        dst[out++] = tmp[--n];
    }
    dst[out] = 0;
    return out;
}

void menu_splash(int force_menu) {
    menu_apply_orientation(LIPPUS_LANDSCAPE);
    lippus_gfx_clip(0, 0, 0, 0);
    backdrop();
    lippus_gfx_fill_rect(0, 0, screen_w(), 4, COL_AMBER);
    center_text(78, "LIPPUS", COL_INK, 3);
    center_text(112, "BOY", COL_AMBER, 2);
    lippus_gfx_fill_rect((screen_w() - 128) / 2, 142, 128, 2, COL_AMBER);
    center_text(160, force_menu ? "menu forzato" : "tieni SELECT per il menu", COL_MUTED, 1);
    lippus_gfx_present();
    sleep_ms(force_menu ? 350 : 650);
}

void menu_notice(const char *title, const char *body) {
    int w = screen_w();
    int box_w = w - 48;
    if (box_w > 272) {
        box_w = 272;
    }
    if (box_w < 160) {
        box_w = w - 16;
    }
    int box_x = (w - box_w) / 2;
    int box_y = screen_h() / 2 - 65;
    if (box_y < 24) {
        box_y = 24;
    }
    lippus_gfx_clip(0, 0, 0, 0);
    backdrop();
    lippus_gfx_fill_rect(box_x, box_y, box_w, 130, COL_PANEL);
    lippus_gfx_rect(box_x, box_y, box_w, 130, COL_AMBER);
    center_text(box_y + 16, title ? title : "Lippus", COL_AMBER, 2);
    center_text(box_y + 52, body ? body : "", COL_INK, 1);
    center_text(box_y + 98, "A continua", COL_MUTED, 1);
    lippus_gfx_present();
}

void menu_install(const char *name, int percent) {
    char shown[22];
    int w = screen_w();
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    int scale = name && lippus_gfx_text_width(name, 2) <= w - 20 ? 2 : 1;
    shorten(shown, name && name[0] ? name : "gioco", scale == 2 ? 16 : 28);
    int bar_w = w - 72;
    if (bar_w > 248) {
        bar_w = 248;
    }
    int bar_x = (w - bar_w) / 2;
    int mid = screen_h() / 2;
    lippus_gfx_clip(0, 0, 0, 0);
    backdrop();
    center_text(mid - 58, "INSTALLAZIONE", COL_AMBER, 2);
    center_text(mid - 24, shown, COL_INK, scale);
    lippus_gfx_rect(bar_x, mid + 18, bar_w, 18, COL_AMBER);
    if (percent > 0) {
        lippus_gfx_fill_rect(bar_x + 2, mid + 20, ((bar_w - 4) * percent) / 100, 14, COL_AMBER);
    }
    center_text(mid + 52, "Non spegnere", COL_MUTED, 1);
    lippus_gfx_present();
}

void menu_launching(const char *name) {
    char shown[22];
    int w = screen_w();
    int scale = name && lippus_gfx_text_width(name, 2) <= w - 20 ? 2 : 1;
    shorten(shown, name && name[0] ? name : "gioco", scale == 2 ? 16 : 28);
    int mid = screen_h() / 2;
    lippus_gfx_clip(0, 0, 0, 0);
    backdrop();
    center_text(mid - 42, "AVVIO", COL_MUTED, 1);
    center_text(mid - 20, shown, COL_AMBER, scale);
    lippus_gfx_present();
    lippus_tone(660);
    sleep_ms(40);
    lippus_tone(880);
    sleep_ms(70);
    lippus_tone(0);
    sleep_ms(180);
}

static void draw_menu(const game_info_t *games, int count, int selected, int window) {
    const int pitch = 32;
    const int visible = menu_visible_rows();
    int w = screen_w();
    int h = screen_h();
    int list_w = w - 32;
    int name_chars = list_w / 16;
    if (name_chars > 16) {
        name_chars = 16;
    }
    if (name_chars < 8) {
        name_chars = 8;
    }
    lippus_gfx_clip(0, 0, 0, 0);
    backdrop();
    lippus_gfx_fill_rect(0, 0, w, 42, LIPPUS_RGB(12, 8, 28));
    lippus_gfx_fill_rect(0, 40, w, 2, COL_AMBER);
    lippus_gfx_text(16, 12, "LIPPUS BOY", COL_AMBER, 2);

    if (count <= 0) {
        int box_w = list_w;
        int box_x = 16;
        lippus_gfx_rect(box_x, 70, box_w, 100, COL_AMBER);
        center_text(86, "Nessun gioco", COL_INK, 2);
        center_text(116, "Copia i .lbg in /GAMES", COL_MUTED, 1);
        center_text(136, "B aggiorna la scheda", COL_MUTED, 1);
    } else {
        char page[16];
        int used = wr_u(page, (unsigned)(selected + 1));
        page[used++] = '/';
        wr_u(page + used, (unsigned)count);
        int page_w = lippus_gfx_text_width(page, 1);
        lippus_gfx_text(w - 16 - page_w, 16, page, COL_MUTED, 1);

        for (int row = 0; row < visible; row++) {
            int index = window + row;
            if (index >= count) {
                break;
            }
            int y = 50 + row * pitch;
            int on = index == selected;
            int bad = !games[index].compatible;
            uint16_t fill = on ? COL_AMBER : COL_PANEL;
            uint16_t ink = on ? COL_INK_DARK : (bad ? COL_BAD : COL_INK);
            lippus_gfx_fill_rect(16, y, list_w, 28, fill);
            if (on) {
                lippus_gfx_fill_rect(16, y, 5, 28, COL_INK);
            } else {
                lippus_gfx_rect(16, y, list_w, 28, LIPPUS_RGB(70, 56, 110));
            }
            char shown[17];
            shorten(shown, games[index].name, name_chars);
            lippus_gfx_text(28, y + 6, shown, ink, 2);
        }
    }

    lippus_gfx_fill_rect(0, h - 26, w, 26, LIPPUS_RGB(12, 8, 28));
    lippus_gfx_fill_rect(0, h - 26, w, 1, COL_AMBER);
    if (count > 0 && !games[selected].compatible) {
        center_text(h - 18, catalog_reason(games[selected].reason), COL_BAD, 1);
    } else if (count > 0) {
        center_text(h - 18, w < 280 ? "A avvia  B  SELECT gira" : "A avvia  B aggiorna  SELECT gira",
                    COL_MUTED, 1);
    } else {
        center_text(h - 18, w < 280 ? "B  SELECT gira" : "B aggiorna  SELECT gira", COL_MUTED, 1);
    }
}

int menu_run(const game_info_t *games, int count, int *index) {
    int visible = menu_visible_rows();
    int selected = index ? *index : 0;
    if (selected < 0 || selected >= count) {
        selected = 0;
    }
    int window = 0;
    if (count > visible && selected >= visible) {
        window = selected - visible + 1;
    }
    while (pad_raw() & (LIPPUS_A | LIPPUS_START | LIPPUS_CLICK | LIPPUS_SELECT)) {
        sleep_ms(10);
    }
    pad_reset();
    menu_restore_orientation();

    int dirty = 1;
    uint32_t repeat_at = 0;
    for (;;) {
        visible = menu_visible_rows();
        lippus_pad_t pad;
        lippus_pad_read(&pad);
        uint32_t now = lippus_ticks();
        int move = 0;
        if (count > 0 && (pad.pressed & (LIPPUS_UP | LIPPUS_DOWN))) {
            move = (pad.pressed & LIPPUS_UP) ? -1 : 1;
            repeat_at = now + 320u;
        } else if (count > 0 && (pad.held & (LIPPUS_UP | LIPPUS_DOWN)) &&
                   (int32_t)(now - repeat_at) >= 0 && repeat_at != 0) {
            move = (pad.held & LIPPUS_UP) ? -1 : 1;
            repeat_at = now + 90u;
        }
        if (!(pad.held & (LIPPUS_UP | LIPPUS_DOWN))) {
            repeat_at = 0;
        }
        if (move && count > 0) {
            selected += move;
            if (selected < 0) {
                selected = count - 1;
            }
            if (selected >= count) {
                selected = 0;
            }
            if (selected < window) {
                window = selected;
            }
            if (selected >= window + visible) {
                window = selected - visible + 1;
            }
            if (index) {
                *index = selected;
            }
            lippus_tone(740);
            sleep_ms(12);
            lippus_tone(0);
            dirty = 1;
        }
        if (pad.pressed & LIPPUS_SELECT) {
            unsigned next =
                s_orient == LIPPUS_PORTRAIT ? LIPPUS_LANDSCAPE : LIPPUS_PORTRAIT;
            menu_apply_orientation(next);
            visible = menu_visible_rows();
            if (selected < window) {
                window = selected;
            }
            if (selected >= window + visible) {
                window = selected - visible + 1;
                if (window < 0) {
                    window = 0;
                }
            }
            lippus_tone(520);
            sleep_ms(20);
            lippus_tone(0);
            while (pad_raw() & LIPPUS_SELECT) {
                sleep_ms(10);
            }
            pad_reset();
            dirty = 1;
        }
        if (pad.pressed & LIPPUS_B) {
            return MENU_RESCAN;
        }
        if (count > 0 && games[selected].compatible &&
            (pad.pressed & (LIPPUS_A | LIPPUS_START | LIPPUS_CLICK))) {
            if (index) {
                *index = selected;
            }
            return MENU_LAUNCH;
        }

        /* Un solo present per cambio selezione/orientamento. */
        if (dirty) {
            draw_menu(games, count, selected, window);
            lippus_gfx_present();
            dirty = 0;
        } else {
            sleep_ms(15);
        }
    }
}
