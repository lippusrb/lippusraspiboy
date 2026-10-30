#pragma once

#include <stdint.h>

/* Mappa condivisa fra firmware, linker dei giochi e impacchettatore.
   tools/check_layout.py verifica che questi numeri restino allineati
   a sdk/linker/game.ld e al defsym della RAM in CMakeLists.txt. */

#define LIPPUS_API_VERSION 3u

#define LIPPUS_SCREEN_W 320
#define LIPPUS_SCREEN_H 240

/* Il gioco sceglie l'orientamento con una definizione globale:
     const uint32_t lippus_orientation = LIPPUS_PORTRAIT;
   Senza quella definizione il firmware resta a 320x240.
   In verticale lo schermo logico e' 240x320: usa gfx_width() e gfx_height().
   Nessun sensore: il vetro si ruota prima del gioco, i tasti non cambiano. */
#define LIPPUS_LANDSCAPE 0u
#define LIPPUS_PORTRAIT 1u

#define LIPPUS_XIP_BASE 0x10000000u
#define LIPPUS_OS_FLASH_LIMIT 0x00040000u
#define LIPPUS_FLASH_BYTES 0x00200000u
#define LIPPUS_INSTALL_RESERVE 0x00001000u
#define LIPPUS_GAME_FLASH_ORIGIN (LIPPUS_XIP_BASE + LIPPUS_OS_FLASH_LIMIT)

#define LIPPUS_RAM_ORIGIN 0x20000000u
#define LIPPUS_OS_RAM_LENGTH 0x0002D800u
#define LIPPUS_SRAM_END 0x20040000u
#define LIPPUS_GAME_RAM_ORIGIN (LIPPUS_RAM_ORIGIN + LIPPUS_OS_RAM_LENGTH)
#define LIPPUS_GAME_RAM_LENGTH (LIPPUS_SRAM_END - LIPPUS_GAME_RAM_ORIGIN)

#define LIPPUS_RGB(r, g, b)                                                    \
    ((uint16_t)((((uint16_t)(r) & 0xF8u) << 8) |                               \
                (((uint16_t)(g) & 0xFCu) << 3) |                               \
                (((uint16_t)(b) & 0xF8u) >> 3)))
