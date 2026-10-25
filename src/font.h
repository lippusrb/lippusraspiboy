#pragma once

#include <stdint.h>

/* NULL se il glifo non esiste. Otto byte, un byte per riga. */
const uint8_t *font_glyph(int code);
