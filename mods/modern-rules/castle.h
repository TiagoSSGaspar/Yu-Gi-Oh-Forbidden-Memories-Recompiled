#ifndef MODERN_RULES_CASTLE_H
#define MODERN_RULES_CASTLE_H
#include "pc/mods/modapi.h"

/* Castle of Dark Illusions (castle.c): its darkness, lit by Swords of
 * Revealing Light. 1 when the host took the effect event; the overlay goes
 * after the one already in `mod`. */
int Castle_Install(const MemoriesModHost *host, MemoriesMod *mod);
/* Whether monster record `r` is in the dark: a monster beside a face-up
 * Castle of Dark Illusions not yet lit, the castle itself not. */
int Castle_Hides(int r);
/* The player's attack: whether the target cursor (state 6, on the
 * opponent's monsters) is on one in the dark. */
int Castle_Refuses(int row, int col);
#endif
