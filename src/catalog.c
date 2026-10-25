#include "catalog.h"

#include "fat.h"
#include "os.h"

#include "pico/stdlib.h"

#include <string.h>

const char *catalog_reason(uint8_t reason) {
    switch (reason) {
    case GAME_OK:
        return "";
    case GAME_BAD_MAGIC:
        return "file non valido";
    case GAME_BAD_API:
        return "libreria diversa";
    case GAME_BAD_LOAD:
        return "indirizzo errato";
    case GAME_TOO_BIG:
        return "troppo grande";
    case GAME_BAD_ENTRY:
        return "entry non valida";
    case GAME_TRUNCATED:
        return "file incompleto";
    default:
        return "errore";
    }
}

static void copy_name(char *dst, const char *src) {
    int i = 0;
    for (; i < LIPPUS_NAME_LEN - 1 && src[i]; i++) {
        unsigned char ch = (unsigned char)src[i];
        dst[i] = (ch >= 32 && ch < 127) ? (char)ch : '?';
    }
    dst[i] = 0;
}

static bool ext_is_lbg(const uint8_t ext[3]) {
    char a = (char)ext[0];
    char b = (char)ext[1];
    char c = (char)ext[2];
    if (a >= 'a' && a <= 'z') {
        a = (char)(a - 32);
    }
    if (b >= 'a' && b <= 'z') {
        b = (char)(b - 32);
    }
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 32);
    }
    return a == 'L' && b == 'B' && c == 'G';
}

static int name_cmp(const char *a, const char *b) {
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') {
            ca = (unsigned char)(ca - 32u);
        }
        if (cb >= 'a' && cb <= 'z') {
            cb = (unsigned char)(cb - 32u);
        }
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int fill_game(game_info_t *game, const fat_dirent_t *ent) {
    memset(game, 0, sizeof *game);
    copy_name(game->name, ent->name[0] ? ent->name : "gioco");
    game->cluster = ent->cluster;
    game->file_size = ent->size;
    game->reason = GAME_TRUNCATED;

    fat_file_t file;
    if (!fat_file_open(&file, ent->cluster, ent->size)) {
        return 0;
    }
    uint8_t raw[64];
    int got = fat_file_read(&file, raw, sizeof raw);
    if (got < 0) {
        return -1;
    }
    if (got < 64) {
        return 0;
    }

    lbg_header_t hdr;
    memcpy(&hdr, raw, sizeof hdr);
    if (memcmp(hdr.magic, "LBG1", 4) != 0) {
        game->reason = GAME_BAD_MAGIC;
        return 0;
    }
    hdr.name[31] = 0;
    if (hdr.name[0]) {
        copy_name(game->name, hdr.name);
    }
    game->payload_bytes = hdr.payload_bytes;
    game->load_addr = hdr.load_addr;
    game->entry = hdr.entry;
    game->crc32 = hdr.crc32;
    game->api_version = hdr.api_version;
    game->header_bytes = hdr.header_bytes;

    uint32_t max_payload = PICO_FLASH_SIZE_BYTES - LIPPUS_OS_FLASH_LIMIT - LIPPUS_INSTALL_RESERVE;
    if (hdr.header_bytes < 64 || hdr.header_bytes > 1024) {
        game->reason = GAME_BAD_MAGIC;
    } else if (hdr.api_version != LIPPUS_API_VERSION) {
        game->reason = GAME_BAD_API;
    } else if (hdr.load_addr != LIPPUS_GAME_FLASH_ORIGIN) {
        game->reason = GAME_BAD_LOAD;
    } else if (hdr.payload_bytes < 16u || hdr.payload_bytes > max_payload) {
        game->reason = GAME_TOO_BIG;
    } else if (ent->size < (uint32_t)hdr.header_bytes + hdr.payload_bytes) {
        game->reason = GAME_TRUNCATED;
    } else if ((hdr.entry & 1u) == 0) {
        game->reason = GAME_BAD_ENTRY;
    } else if ((hdr.entry & ~1u) < hdr.load_addr ||
               (hdr.entry & ~1u) >= hdr.load_addr + hdr.payload_bytes) {
        game->reason = GAME_BAD_ENTRY;
    } else {
        game->reason = GAME_OK;
        game->compatible = 1;
    }
    return 0;
}

static bool name_is(const char *value, const char *want) {
    while (*value && *want) {
        char a = *value;
        char b = *want;
        if (a >= 'a' && a <= 'z') {
            a = (char)(a - 32);
        }
        if (b >= 'a' && b <= 'z') {
            b = (char)(b - 32);
        }
        if (a != b) {
            return false;
        }
        value++;
        want++;
    }
    return *value == 0 && *want == 0;
}

static int scan_dir(fat_dir_t *dir, game_info_t *games, int max_games) {
    int count = 0;
    fat_dirent_t ent;
    while (fat_dir_next(dir, &ent)) {
        if (ent.attr & 0x10) {
            continue;
        }
        if (ent.name[0] == '.') {
            continue;
        }
        if (!ext_is_lbg(ent.ext)) {
            continue;
        }
        if (count >= max_games) {
            break;
        }
        int rc = fill_game(&games[count], &ent);
        if (rc < 0) {
            return -1;
        }
        count++;
    }
    if (!fat_dir_ok(dir)) {
        return -1;
    }
    for (int i = 1; i < count; i++) {
        game_info_t key = games[i];
        int j = i - 1;
        while (j >= 0 && name_cmp(games[j].name, key.name) > 0) {
            games[j + 1] = games[j];
            j--;
        }
        games[j + 1] = key;
    }
    return count;
}

int catalog_scan(game_info_t *games, int max_games) {
    fat_dir_t dir;
    fat_dirent_t ent;
    bool have_folder = false;
    uint32_t folder = 0;

    if (!fat_dir_open_root(&dir)) {
        return -1;
    }
    while (fat_dir_next(&dir, &ent)) {
        if ((ent.attr & 0x10) && name_is(ent.name, "GAMES")) {
            have_folder = true;
            folder = ent.cluster;
            break;
        }
    }
    if (!fat_dir_ok(&dir)) {
        return -1;
    }
    if (have_folder) {
        if (!fat_dir_open_cluster(&dir, folder)) {
            return -1;
        }
    } else if (!fat_dir_open_root(&dir)) {
        return -1;
    }
    return scan_dir(&dir, games, max_games);
}
