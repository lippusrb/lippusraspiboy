#pragma once

#include <stdbool.h>
#include <stdint.h>

bool sd_init(void);
bool sd_read_block(uint32_t lba, uint8_t *dst);
const char *sd_error(void);
