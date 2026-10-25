#include <stdint.h>

#include "lippus/layout.h"

/* Definizione debole: un gioco la sostituisce con la propria.
   La sezione resta nel binario anche se il gioco non la cita. */
__attribute__((weak, used, section(".lippus_orient")))
const uint32_t lippus_orientation = LIPPUS_LANDSCAPE;
