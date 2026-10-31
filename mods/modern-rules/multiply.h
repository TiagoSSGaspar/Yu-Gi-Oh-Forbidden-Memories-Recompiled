#ifndef MODERN_RULES_MULTIPLY_H
#define MODERN_RULES_MULTIPLY_H
#include "pc/mods/modapi.h"

/* Kuriboh and Multiply (multiply.c); the wall is drawn by 3D Monsters when
 * it is loaded. 1 when the host took the hook. */
int Multiply_Install(const MemoriesModHost *host, MemoriesMod *mod);
/* Whether `card` is Multiply, with the setting on. */
int Multiply_Is(int card);
/* Multiply has landed in its zone, on the Kuriboh in `monster`: the wall
 * comes up. */
void Multiply_Landed(int monster);
/* Whether `side` has a Multiply on the field: its wall up. */
int Multiply_Up(int side);
/* Whether `ids` hold a Multiply. */
int Multiply_InChain(const int *ids, int count);
/* The wall's Kuriboh, gone, is born again while its side's Multiply is
 * there, in defense. Called while the hand or the field is idle; 0, nothing
 * waits. */
int Multiply_Hold(void);
/* The wall's Kuriboh of `side` (the one Multiply went onto, or the one born
 * again in its place), as a record, or -1. */
int Multiply_Wall(int side);
/* The battle has begun (DuelScene_UpdateBattle's first frame): an attack
 * on the wall's Kuriboh goes to the wall. */
void Multiply_BattleBegan(void);
/* The battle is over: a clone dies. */
void Multiply_BattleOver(void);
/* For the CPU (cpu.c): whether record `r` is the wall's Kuriboh, and how
 * often `side` has attacked the other side's wall. */
int Multiply_Walled(int r);
int Multiply_Attacks(int side);
/* The CPU's hand decision: a boss plays Harpie's Feather Duster first
 * against the player's wall. 1 when it did. */
int Multiply_CpuBreak(void);
/* The CPU's hand decision: a wall of its own (a boss as soon as it can, a
 * middle one when behind): Multiply onto its Kuriboh, or the Kuriboh first.
 * 1 when it did. */
int Multiply_CpuRaise(void);
#endif
