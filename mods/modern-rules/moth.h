#ifndef MODERN_RULES_MOTH_H
#define MODERN_RULES_MOTH_H
#include "pc/mods/modapi.h"

/* Petit Moth's evolution (moth.c): its events, state and overlay. 1 when
 * the host took them. */
int Moth_Install(const MemoriesModHost *host, MemoriesMod *mod);
/* 1 while a cocoon evolves: the hand state waits, its function not called. */
int Moth_Hold(void);
#endif
