#ifndef MODERN_RULES_SPELLS_H
#define MODERN_RULES_SPELLS_H
#include "pc/mods/modapi.h"
#include "../extra-hand-plays/extra_hand_plays.h"

/* Change of Heart and Monster Reborn (spells.c): their effects, the
 * graveyard they need, their pick and the CPU's plays. 1 when the host took
 * the events; the overlay goes after the one already in `mod`. */
int Spells_Install(const MemoriesModHost *host, MemoriesMod *mod, const ExtraHandApi *api);
/* Whether card `card`, used now by side `side`, would do nothing (Change of
 * Heart with no monster to take or no zone for it; Monster Reborn with an
 * empty graveyard or no zone): its use is refused. */
int Spells_Refuses(int card, int side);
/* A side's turn starts: the monsters taken by Change of Heart go back. */
void Spells_TurnStarts(int side);
/* 1 while the monsters taken go back, at the hand's or the field's start. */
int Spells_Hold(void);
/* The CPU's hand decision: Change of Heart or Monster Reborn played. */
int Spells_Cpu(void);
/* Monster record `r` is the one the player's pick is on (drawn lit). */
int Spells_Aimed(int r);
#endif
