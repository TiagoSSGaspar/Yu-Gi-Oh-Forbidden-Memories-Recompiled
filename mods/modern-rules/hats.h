#ifndef MODERN_RULES_HATS_H
#define MODERN_RULES_HATS_H
#include "pc/mods/modapi.h"

/* Magical Hats (hats.c): the trap's hook and the decoys'. 1 when the host
 * took them. */
int Hats_Install(const MemoriesModHost *host);
/* 1 while the hats go up after the trap sprang, or the decoys go when the
 * monster hidden among them has left: the hand or the field waits. */
int Hats_Hold(void);
/* Whether card `id` is a decoy hat (no graveyard, no effects). */
int Hats_IsDecoy(int id);
#endif
