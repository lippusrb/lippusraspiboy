#pragma once

#include <stdint.h>

/* File .lbg sulla SD: intestazione da 64 byte, poi il binario collegato
   all'indirizzo LIPPUS_GAME_FLASH_ORIGIN. Il payload non include l'intestazione. */

#define LBG_MAGIC_0 'L'
#define LBG_MAGIC_1 'B'
#define LBG_MAGIC_2 'G'
#define LBG_MAGIC_3 '1'

/* Bit 0 di flags. Gli altri bit restano liberi. 0 = orizzontale. */
#define LBG_FLAG_PORTRAIT 0x00000001u

typedef struct __attribute__((packed)) {
    char magic[4];
    uint16_t header_bytes;
    uint16_t api_version;
    uint32_t payload_bytes;
    uint32_t load_addr;
    uint32_t entry;
    uint32_t flags;
    char name[32];
    uint32_t crc32;
    uint32_t reserved;
} lbg_header_t;

_Static_assert(sizeof(lbg_header_t) == 64, "intestazione LBG da 64 byte");
