#pragma once

#include <stddef.h>
#include <stdint.h>

#include "abi.h"

/* Millisecondi dal boot. Il contatore torna a zero circa ogni 49 giorni. */
static inline uint32_t sys_ticks(void) { return lippus_api()->ticks(); }

static inline void sys_sleep(uint32_t ms) { lippus_api()->sleep_ms(ms); }

/* Torna al menu. Equivale a tenere START e SELECT per circa 700 ms,
   controllo che il firmware fa dentro gfx_present, pad_read e sys_sleep. */
static inline void sys_exit(void) { lippus_api()->exit_to_menu(); }

/* Allocatore a bump nella RAM del gioco. Non si libera un singolo blocco:
   sys_alloc_reset() rimette tutto a disposizione. La memoria non e' azzerata.
   Vive nello stub del gioco, non nel firmware. */
void *sys_alloc(size_t bytes);
void sys_alloc_reset(void);

/* Chiamala a fine frame. Dorme quel che serve per restare su frame_ms. */
void sys_frame(uint32_t frame_ms);
