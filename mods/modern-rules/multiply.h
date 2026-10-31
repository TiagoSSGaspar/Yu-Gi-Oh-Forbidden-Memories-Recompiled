#ifndef MODERN_RULES_MULTIPLY_H
#define MODERN_RULES_MULTIPLY_H
#include "pc/mods/modapi.h"

/* Kuriboh and Multiply (multiply.c): the battle hook and its line. 1 when
 * the host took the hook; the overlay goes after the one already in `mod`. */
int Multiply_Install(const MemoriesModHost *host, MemoriesMod *mod);
/* Whether `card` is Multiply, with the setting on. */
int Multiply_Is(int card);
/* Multiply has landed in its zone, on the Kuriboh in `monster`: the wall
 * comes up. */
void Multiply_Landed(int monster);
/* A Kuriboh of `side` on the field, as a record, or -1. */
int Multiply_Kuriboh(int side);
#endif
