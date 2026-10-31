#ifndef MODERN_RULES_EQUIPS_H
#define MODERN_RULES_EQUIPS_H
#include "pc/mods/modapi.h"

/* Equips that stay on the field (equips.c). 1 when the host took its hooks;
 * its overlay goes after the one already in `mod`. */
int Equips_Install(const MemoriesModHost *host, MemoriesMod *mod);
/* Whether the setting is on, for the duel being played. */
int Equips_On(void);
/* 1 while an equip whose monster left the field is being destroyed: the
 * hand or the field waits, its function not called. Also takes the bonus
 * of an equip that left the field off its monster. */
int Equips_Hold(void);
/* How many equips stay on the field when `ids` go down in order onto
 * `first` (a monster, or 0): the ones that equip the card made so far,
 * since the last fusion. */
int Equips_Kept(int first, const int *ids, int count);
/* The free magic and trap zones of `side`. */
int Equips_FreeZones(int side);
/* Whether the magic and trap zone `col` of `side` holds an equip in use. */
int Equips_InZone(int side, int col);
#endif
