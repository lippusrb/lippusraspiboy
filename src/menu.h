#pragma once

#include "catalog.h"

#define MENU_LAUNCH 1
#define MENU_RESCAN 2

void menu_splash(int force_menu);
void menu_notice(const char *title, const char *body);
void menu_install(const char *name, int percent);
void menu_launching(const char *name);
int menu_run(const game_info_t *games, int count, int *index);
/* Dopo un gioco ripristina l'orientamento scelto nel menu (SELECT). */
void menu_restore_orientation(void);
