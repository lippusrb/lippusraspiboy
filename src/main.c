#include "catalog.h"
#include "loader.h"
#include "menu.h"
#include "os.h"
#include "sd.h"
#include "fat.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

extern char __bss_end__[];
extern char __HeapLimit[];
extern char __StackLimit[];

static bool layout_ok(void) {
    return (uintptr_t)__bss_end__ <= LIPPUS_GAME_RAM_ORIGIN &&
           (uintptr_t)__HeapLimit <= LIPPUS_GAME_RAM_ORIGIN &&
           (uintptr_t)__StackLimit <= LIPPUS_GAME_RAM_ORIGIN;
}

static void wait_confirm(void) {
    const uint16_t keys = LIPPUS_A | LIPPUS_B | LIPPUS_START | LIPPUS_CLICK;
    while (pad_raw() & keys) {
        sleep_ms(10);
    }
    while ((pad_raw() & keys) == 0) {
        sleep_ms(20);
        gpio_put(PICO_DEFAULT_LED_PIN, (time_us_32() >> 18) & 1u);
    }
    while (pad_raw() & keys) {
        sleep_ms(10);
    }
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

static bool mount_storage(void) {
    if (!sd_init()) {
        menu_notice("Scheda SD", sd_error());
        return false;
    }
    if (!fat_mount()) {
        menu_notice("Scheda SD", fat_error());
        return false;
    }
    return true;
}

int main(void) {
    static game_info_t games[LIPPUS_MAX_GAMES];
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    log_init();
    lippus_log("boot");

    gfx_init();
    pad_init();
    audio_init();

    if (!layout_ok()) {
        menu_notice("Memoria", "Firmware oltre la RAM giochi");
        for (;;) {
            tight_loop_contents();
        }
    }

    int force_menu = (pad_raw() & LIPPUS_SELECT) != 0;
    menu_splash(force_menu);
    int selected = 0;
    int skip_auto = force_menu;

    for (;;) {
        if (!mount_storage()) {
            wait_confirm();
            continue;
        }
        int count = catalog_scan(games, LIPPUS_MAX_GAMES);
        if (count < 0) {
            menu_notice("Lettura", "Errore sulla scheda");
            wait_confirm();
            continue;
        }
        lippus_log(count == 1 ? "un gioco" : "giochi trovati");

        if (count == 1 && !skip_auto && games[0].compatible) {
            if (!loader_launch(&games[0])) {
                menu_notice("Avvio fallito", loader_error());
                wait_confirm();
            }
            skip_auto = 1;
            continue;
        }

        for (;;) {
            int action = menu_run(games, count, &selected);
            if (action == MENU_RESCAN) {
                break;
            }
            if (action == MENU_LAUNCH && games[selected].compatible) {
                if (!loader_launch(&games[selected])) {
                    menu_notice("Avvio fallito", loader_error());
                    wait_confirm();
                }
                skip_auto = 1;
            }
        }
    }
}
