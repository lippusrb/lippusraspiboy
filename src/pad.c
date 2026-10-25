#include "os.h"

#include "board.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

/* Su resta il pin su anche se il gioco e' in verticale: il vetro si ruota a mano.
 * 16 ms: sotto i 30 del frame, e lontano da 1 ms. Due letture uguali scartano
 * il rimbalzo. I fronti restano finche' il gioco non chiama pad_read. */
#define PAD_SAMPLE_US (-16000)

static const uint8_t k_pins[11] = {
    LIPPUS_PIN_UP,    LIPPUS_PIN_DOWN, LIPPUS_PIN_LEFT,  LIPPUS_PIN_RIGHT,
    LIPPUS_PIN_CLICK, LIPPUS_PIN_A,    LIPPUS_PIN_B,     LIPPUS_PIN_X,
    LIPPUS_PIN_Y,     LIPPUS_PIN_START, LIPPUS_PIN_SELECT,
};

static const uint16_t k_bits[11] = {
    LIPPUS_UP,    LIPPUS_DOWN, LIPPUS_LEFT,  LIPPUS_RIGHT, LIPPUS_CLICK, LIPPUS_A,
    LIPPUS_B,     LIPPUS_X,    LIPPUS_Y,     LIPPUS_START, LIPPUS_SELECT,
};

static uint16_t s_prev;
static uint16_t s_stable;
static uint16_t s_pressed;
static uint16_t s_released;
static repeating_timer_t s_timer;

static bool pad_sample(repeating_timer_t *timer) {
    (void)timer;
    uint16_t raw = pad_raw();
    if (raw == s_prev) {
        s_pressed |= (uint16_t)(raw & ~s_stable);
        s_released |= (uint16_t)((~raw) & s_stable);
        s_stable = raw;
    }
    s_prev = raw;
    return true;
}

uint16_t pad_raw(void) {
    uint16_t mask = 0;
    for (int i = 0; i < 11; i++) {
        if (!gpio_get(k_pins[i])) {
            mask |= k_bits[i];
        }
    }
    return mask;
}

void pad_init(void) {
    for (int i = 0; i < 11; i++) {
        gpio_init(k_pins[i]);
        gpio_set_dir(k_pins[i], GPIO_IN);
        gpio_pull_up(k_pins[i]);
    }
    sleep_ms(5);
    s_prev = s_stable = pad_raw();
    s_pressed = 0;
    s_released = 0;
    if (!add_repeating_timer_us(PAD_SAMPLE_US, pad_sample, 0, &s_timer)) {
        lippus_log("timer tasti assente");
    }
}

void pad_reset(void) {
    uint32_t irq = save_and_disable_interrupts();
    s_prev = s_stable = pad_raw();
    s_pressed = 0;
    s_released = 0;
    restore_interrupts(irq);
}

void lippus_pad_read(lippus_pad_t *out) {
    lippus_service();
    uint32_t irq = save_and_disable_interrupts();
    out->held = s_stable;
    out->pressed = s_pressed;
    out->released = s_released;
    s_pressed = 0;
    s_released = 0;
    restore_interrupts(irq);
}
