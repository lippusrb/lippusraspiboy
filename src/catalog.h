#pragma once

#include <stdint.h>

#define LIPPUS_MAX_GAMES 24
#define LIPPUS_NAME_LEN 32

enum {
    GAME_OK = 0,
    GAME_BAD_MAGIC,
    GAME_BAD_API,
    GAME_BAD_LOAD,
    GAME_TOO_BIG,
    GAME_BAD_ENTRY,
    GAME_TRUNCATED,
};

typedef struct {
    char name[LIPPUS_NAME_LEN];
    uint32_t cluster;
    uint32_t file_size;
    uint32_t payload_bytes;
    uint32_t load_addr;
    uint32_t entry;
    uint32_t crc32;
    uint16_t api_version;
    uint16_t header_bytes;
    uint8_t compatible;
    uint8_t reason;
} game_info_t;

/* Ritorna il numero di giochi, oppure -1 se la lettura della SD fallisce. */
int catalog_scan(game_info_t *games, int max_games);
const char *catalog_reason(uint8_t reason);
