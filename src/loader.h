#pragma once

#include "catalog.h"

#include <stdbool.h>

bool loader_launch(const game_info_t *game);
const char *loader_error(void);
