#ifndef MODERN_RULES_ARROW_H
#define MODERN_RULES_ARROW_H
#include "pc/mods/modapi.h"
#include "../extra-hand-plays/extra_hand_plays.h"

/* Living Arrow (arrow.c): Polymerization, Living Arrow and a Zombie from the
 * hand infect an opponent's living monster. `api` takes the cards out of
 * the hand (version 8). 1 when the host took the hook. */
int Arrow_Install(const MemoriesModHost *host, MemoriesMod *mod, const ExtraHandApi *api);
/* The player's hand, idle: the combo's confirm, and the aim that follows.
 * 1 when the frame is the arrow's: no button reaches the hand. */
int Arrow_Hand(void);
/* The CPU's hand: 1 when it played the combo (the hand slides away, its
 * decision is to be emptied). */
int Arrow_Cpu(void);
/* A side's turn begins: the infected of the other side's combos lose their
 * ATK, as that side's turn has ended. */
void Arrow_TurnStarts(int side);
#endif
