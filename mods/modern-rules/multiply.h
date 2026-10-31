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
/* A Kuriboh that left the field is born again while its side's Multiply is
 * there, in defense. Called while the hand or the field is idle; 0, nothing
 * waits. */
int Multiply_Hold(void);
/* A Kuriboh of `side` on the field, as a record, or -1. */
int Multiply_Kuriboh(int side);
#endif
