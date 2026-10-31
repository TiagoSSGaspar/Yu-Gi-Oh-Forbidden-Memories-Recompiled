#ifndef MODERN_RULES_SWAMP_H
#define MODERN_RULES_SWAMP_H
#include "pc/mods/modapi.h"

/* The swamp brothers (swamp.c): their battle hook and line. 1 when the
 * host took the hook; the overlay goes after the one already in `mod`. */
int Swamp_Install(const MemoriesModHost *host, MemoriesMod *mod);
#endif
