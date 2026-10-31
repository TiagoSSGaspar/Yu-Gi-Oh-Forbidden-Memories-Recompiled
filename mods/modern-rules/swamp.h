#ifndef MODERN_RULES_SWAMP_H
#define MODERN_RULES_SWAMP_H
#include "pc/mods/modapi.h"

/* The swamp brothers (swamp.c): their battle hook and line. 1 when the
 * host took the hook; the overlay goes after the one already in `mod`. */
int Swamp_Install(const MemoriesModHost *host, MemoriesMod *mod);
/* What a monster attacking record `r` meets: a battleguard with its brother
 * on its side, their ATKs together; 0 otherwise. */
int Swamp_Together(int r);
/* The brother of card `id` (Swamp and Lava Battleguard), or 0. */
int Swamp_Brother(int id);
#endif
