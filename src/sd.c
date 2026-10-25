#include "sd.h"

#include "board.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

static const char *s_err = "SD";
static bool s_ccs;

const char *sd_error(void) { return s_err; }

static uint8_t crc7(const uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc <<= 1;
            if ((byte ^ crc) & 0x80u) {
                crc ^= 0x09u;
            }
            byte <<= 1;
        }
    }
    return (uint8_t)((crc << 1) | 1u);
}

static uint8_t xchg(uint8_t byte) {
    uint8_t rx = 0xFF;
    spi_write_read_blocking(spi1, &byte, &rx, 1);
    return rx;
}

static void cs_high(void) { gpio_put(LIPPUS_PIN_SD_CS, 1); }
static void cs_low(void) { gpio_put(LIPPUS_PIN_SD_CS, 0); }

static int sd_cmd(uint8_t cmd, uint32_t arg, uint8_t *trail, int trail_len, bool keep_cs) {
    uint8_t packet[6];
    packet[0] = (uint8_t)(0x40u | cmd);
    packet[1] = (uint8_t)(arg >> 24);
    packet[2] = (uint8_t)(arg >> 16);
    packet[3] = (uint8_t)(arg >> 8);
    packet[4] = (uint8_t)arg;
    packet[5] = crc7(packet, 5);

    cs_high();
    xchg(0xFF);
    cs_low();
    spi_write_blocking(spi1, packet, 6);

    int response = 0xFF;
    for (int i = 0; i < 16; i++) {
        response = xchg(0xFF);
        if ((response & 0x80) == 0) {
            break;
        }
    }
    for (int i = 0; i < trail_len; i++) {
        uint8_t byte = xchg(0xFF);
        if (trail) {
            trail[i] = byte;
        }
    }
    if (!keep_cs) {
        cs_high();
        xchg(0xFF);
    }
    return response;
}

bool sd_init(void) {
    s_err = "SD";
    s_ccs = false;

    spi_init(spi1, LIPPUS_SD_BAUD_INIT);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(LIPPUS_PIN_SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LIPPUS_PIN_SD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LIPPUS_PIN_SD_MISO, GPIO_FUNC_SPI);
    gpio_pull_up(LIPPUS_PIN_SD_MISO);
    gpio_init(LIPPUS_PIN_SD_CS);
    gpio_set_dir(LIPPUS_PIN_SD_CS, GPIO_OUT);
    cs_high();

    sleep_ms(2);
    for (int i = 0; i < 16; i++) {
        xchg(0xFF);
    }

    bool idle = false;
    for (int i = 0; i < 20; i++) {
        if (sd_cmd(0, 0, 0, 0, false) == 0x01) {
            idle = true;
            break;
        }
        sleep_ms(10);
    }
    if (!idle) {
        s_err = "SD non risponde";
        return false;
    }

    uint8_t trail[4] = {0, 0, 0, 0};
    int cmd8 = sd_cmd(8, 0x1AAu, trail, 4, false);
    bool v2 = false;
    if (cmd8 == 0x01) {
        v2 = true;
        if (trail[3] != 0xAA) {
            s_err = "SD CMD8";
            return false;
        }
    } else if (cmd8 != 0x05) {
        s_err = "SD CMD8";
        return false;
    }

    bool ready = false;
    uint32_t acmd = v2 ? 0x40000000u : 0u;
    for (int i = 0; i < 100; i++) {
        sd_cmd(55, 0, 0, 0, false);
        if (sd_cmd(41, acmd, 0, 0, false) == 0x00) {
            ready = true;
            break;
        }
        sleep_ms(10);
    }
    if (!ready) {
        s_err = "SD init lenta";
        return false;
    }

    if (sd_cmd(58, 0, trail, 4, false) != 0) {
        s_err = "SD CMD58";
        return false;
    }
    s_ccs = v2 && (trail[0] & 0x40) != 0;
    if (!s_ccs && sd_cmd(16, 512, 0, 0, false) != 0) {
        s_err = "SD blocco";
        return false;
    }

    spi_set_baudrate(spi1, LIPPUS_SD_BAUD_RUN);
    s_err = "";
    return true;
}

bool sd_read_block(uint32_t lba, uint8_t *dst) {
    uint32_t arg = s_ccs ? lba : (lba << 9);
    for (int attempt = 0; attempt < 3; attempt++) {
        int response = sd_cmd(17, arg, 0, 0, true);
        if (response != 0) {
            cs_high();
            xchg(0xFF);
            continue;
        }
        uint32_t start = time_us_32();
        int token = 0xFF;
        while ((uint32_t)(time_us_32() - start) < 250000u) {
            token = xchg(0xFF);
            if (token == 0xFE) {
                break;
            }
            if (token != 0xFF) {
                token = 0x00;
                break;
            }
        }
        if (token != 0xFE) {
            cs_high();
            xchg(0xFF);
            continue;
        }
        spi_read_blocking(spi1, 0xFF, dst, 512);
        xchg(0xFF);
        xchg(0xFF);
        cs_high();
        xchg(0xFF);
        return true;
    }
    s_err = "lettura SD";
    return false;
}
