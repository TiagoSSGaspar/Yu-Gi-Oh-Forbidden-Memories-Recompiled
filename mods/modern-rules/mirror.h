#ifndef MODERN_RULES_MIRROR_H
#define MODERN_RULES_MIRROR_H
#include "pc/mods/modapi.h"

/* Mirror Force (mirror.c): the trap's hook. 1 when the host took it. */
int Mirror_Install(const MemoriesModHost *host);
/* 1 while the attacker's other monsters in attack position are destroyed,
 * one at a time, after Mirror Force sprang: the hand or the field waits. */
int Mirror_Hold(void);
/* A side's turn starts: in a new duel, what the CPU learnt of the mirror
 * (cpu.c) is forgotten. */
void Mirror_TurnStarts(void);
#endif
