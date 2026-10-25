#pragma once

/*
 * liblippus — chiamate grafiche, pad e audio per i giochi della console.
 *
 * Il disegno, il display, la SD e l'audio stanno nel firmware del Pico.
 * Un gioco non linka quei driver: riceve una tabella di funzioni all'avvio
 * e la usa tramite gfx_*, pad_*, snd_* e sys_*.
 *
 * Il punto di ingresso e' lippus_main(). Non usare main().
 * L'orientamento di default e' orizzontale, 320x240. Per il verticale:
 *   const uint32_t lippus_orientation = LIPPUS_PORTRAIT;
 * e poi gfx_width() / gfx_height(), che valgono 240 e 320.
 * Lo schermo si gira a mano prima di avviare il gioco. Il firmware non
 * capisce da se come e' messo il pannello. Lo stick resta quello della
 * console: LIPPUS_UP e' sempre il pin su, anche in verticale.
 * Lo stack e' circa 4 KB: gli array grossi vanno in variabili globali
 * o in sys_alloc(), non come variabili locali.
 * START + SELECT tenuti premuti riportano al menu.
 */

#include "lippus/audio.h"
#include "lippus/gfx.h"
#include "lippus/pad.h"
#include "lippus/system.h"

void lippus_main(void);
