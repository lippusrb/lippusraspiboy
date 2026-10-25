#include "loader.h"

#include "fat.h"
#include "menu.h"
#include "os.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include <string.h>

static const char *s_err = "";

const char *loader_error(void) { return s_err; }

static uint32_t crc_begin(void) { return 0xFFFFFFFFu; }

static uint32_t crc_feed(uint32_t crc, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

static uint32_t crc_end(uint32_t crc) { return ~crc; }

static bool range_ok(uint32_t offset, uint32_t len, uint32_t align) {
    if (len == 0 || (offset & (align - 1u)) || (len & (align - 1u))) {
        return false;
    }
    if (offset < LIPPUS_OS_FLASH_LIMIT) {
        return false;
    }
    if (offset + len < offset || offset + len > PICO_FLASH_SIZE_BYTES) {
        return false;
    }
    return true;
}

static bool flash_erase_at(uint32_t offset, uint32_t len) {
    if (!range_ok(offset, len, 4096u)) {
        s_err = "cancellazione rifiutata";
        return false;
    }
    gfx_pause_for_flash();
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(offset, len);
    restore_interrupts(irq);
    gfx_resume_after_flash();
    return true;
}

static bool flash_program_at(uint32_t offset, const uint8_t *data, uint32_t len) {
    if (!range_ok(offset, len, 256u)) {
        s_err = "scrittura rifiutata";
        return false;
    }
    gfx_pause_for_flash();
    uint32_t irq = save_and_disable_interrupts();
    flash_range_program(offset, data, len);
    restore_interrupts(irq);
    gfx_resume_after_flash();
    return true;
}

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t crc32;
    uint32_t payload_bytes;
    uint32_t entry;
    uint32_t load_addr;
    uint32_t api_version;
    char name[32];
} install_rec_t;

#define INSTALL_MAGIC 0x4C50494Eu

static bool record_matches(const install_rec_t *rec, const lbg_header_t *hdr) {
    return rec->magic == INSTALL_MAGIC && rec->crc32 == hdr->crc32 &&
           rec->payload_bytes == hdr->payload_bytes && rec->entry == hdr->entry &&
           rec->load_addr == hdr->load_addr && rec->api_version == hdr->api_version;
}

static void read_record(install_rec_t *rec) {
    uint32_t address = LIPPUS_XIP_BASE + PICO_FLASH_SIZE_BYTES - LIPPUS_INSTALL_RESERVE;
    memcpy(rec, (const void *)address, sizeof *rec);
}

static bool write_record(const lbg_header_t *hdr) {
    install_rec_t rec;
    uint8_t page[256];
    memset(&rec, 0, sizeof rec);
    rec.magic = INSTALL_MAGIC;
    rec.crc32 = hdr->crc32;
    rec.payload_bytes = hdr->payload_bytes;
    rec.entry = hdr->entry;
    rec.load_addr = hdr->load_addr;
    rec.api_version = hdr->api_version;
    memcpy(rec.name, hdr->name, 32);
    memset(page, 0xFF, sizeof page);
    memcpy(page, &rec, sizeof rec);
    return flash_program_at(PICO_FLASH_SIZE_BYTES - LIPPUS_INSTALL_RESERVE, page, sizeof page);
}

static bool install_payload(const lbg_header_t *hdr, fat_file_t *file) {
    uint32_t payload = hdr->payload_bytes;
    uint32_t padded = (payload + 255u) & ~255u;
    uint32_t erase_len = (payload + 4095u) & ~4095u;
    uint32_t slot_end = PICO_FLASH_SIZE_BYTES - LIPPUS_INSTALL_RESERVE;
    if (LIPPUS_OS_FLASH_LIMIT + erase_len > slot_end) {
        s_err = "troppo grande";
        return false;
    }

    menu_install(hdr->name, 0);
    if (!flash_erase_at(PICO_FLASH_SIZE_BYTES - LIPPUS_INSTALL_RESERVE, LIPPUS_INSTALL_RESERVE)) {
        return false;
    }

    uint32_t sectors = erase_len / 4096u;
    for (uint32_t i = 0; i < sectors; i++) {
        if (!flash_erase_at(LIPPUS_OS_FLASH_LIMIT + i * 4096u, 4096u)) {
            return false;
        }
        menu_install(hdr->name, (int)(((i + 1) * 40u) / sectors));
    }

    uint8_t *buf = (uint8_t *)LIPPUS_GAME_RAM_ORIGIN;
    uint32_t crc = crc_begin();
    uint32_t done = 0;
    if (!fat_file_seek(file, hdr->header_bytes)) {
        s_err = "lettura gioco";
        return false;
    }
    while (done < padded) {
        uint32_t chunk = padded - done;
        if (chunk > 4096u) {
            chunk = 4096u;
        }
        memset(buf, 0xFF, chunk);
        if (done < payload) {
            uint32_t need = payload - done;
            if (need > chunk) {
                need = chunk;
            }
            int got = fat_file_read(file, buf, need);
            if (got != (int)need) {
                s_err = "lettura gioco";
                return false;
            }
            crc = crc_feed(crc, buf, need);
        }
        if (!flash_program_at(LIPPUS_OS_FLASH_LIMIT + done, buf, chunk)) {
            return false;
        }
        done += chunk;
        int percent = 40 + (int)((done * 60u) / padded);
        menu_install(hdr->name, percent > 100 ? 100 : percent);
    }
    if (crc_end(crc) != hdr->crc32) {
        s_err = "file danneggiato";
        return false;
    }

    uint32_t check = crc_begin();
    check = crc_feed(check, (const uint8_t *)LIPPUS_GAME_FLASH_ORIGIN, payload);
    if (crc_end(check) != hdr->crc32) {
        s_err = "scrittura flash";
        return false;
    }
    if (!write_record(hdr)) {
        return false;
    }
    return true;
}

static void clear_game_ram(void) {
    uint32_t *ram = (uint32_t *)LIPPUS_GAME_RAM_ORIGIN;
    uint32_t words = LIPPUS_GAME_RAM_LENGTH / 4u;
    for (uint32_t i = 0; i < words; i++) {
        ram[i] = 0;
    }
}

bool loader_launch(const game_info_t *game) {
    s_err = "";
    if (!game || !game->compatible) {
        s_err = catalog_reason(game ? game->reason : GAME_BAD_MAGIC);
        return false;
    }

    fat_file_t file;
    if (!fat_file_open(&file, game->cluster, game->file_size)) {
        s_err = "apertura gioco";
        return false;
    }
    uint8_t raw[64];
    if (fat_file_read(&file, raw, sizeof raw) != (int)sizeof raw) {
        s_err = "lettura gioco";
        return false;
    }
    lbg_header_t hdr;
    memcpy(&hdr, raw, sizeof hdr);
    hdr.name[31] = 0;
    if (memcmp(hdr.magic, "LBG1", 4) != 0 || hdr.crc32 != game->crc32 ||
        hdr.entry != game->entry || hdr.payload_bytes != game->payload_bytes) {
        s_err = "file cambiato";
        return false;
    }

    install_rec_t rec;
    read_record(&rec);
    bool fresh = false;
    if (record_matches(&rec, &hdr)) {
        uint32_t check = crc_begin();
        check = crc_feed(check, (const uint8_t *)LIPPUS_GAME_FLASH_ORIGIN, hdr.payload_bytes);
        fresh = crc_end(check) == hdr.crc32;
    }
    if (!fresh && !install_payload(&hdr, &file)) {
        return false;
    }

    menu_launching(game->name);
    clear_game_ram();
    lippus_tone(0);
    lippus_gfx_clip(0, 0, 0, 0);
    lippus_gfx_orientation((hdr.flags & LBG_FLAG_PORTRAIT) ? LIPPUS_PORTRAIT : LIPPUS_LANDSCAPE);

    typedef void (*game_fn)(const lippus_api_t *);
    game_fn entry = (game_fn)(uintptr_t)hdr.entry;
    lippus_enter_game(entry);

    lippus_tone(0);
    lippus_gfx_clip(0, 0, 0, 0);
    menu_restore_orientation();
    return true;
}
