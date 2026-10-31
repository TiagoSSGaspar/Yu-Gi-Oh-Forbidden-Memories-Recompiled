#ifndef MODERN_RULES_MULTIPLY_H
#define MODERN_RULES_MULTIPLY_H
#include "pc/mods/modapi.h"

/* Kuriboh and Multiply (multiply.c). 1 when installed. */
int Multiply_Install(const MemoriesModHost *host, MemoriesMod *mod);
/* Whether `card` is Multiply, with the setting on. */
int Multiply_Is(int card);
/* Multiply has landed in its zone, on the Kuriboh in `monster`: the side's
 * free monster zones fill with Kuribohs. */
void Multiply_Landed(int monster);
/* Kuribohs that left the field come back as two while their side's
 * Multiply is on the field. 1 while the hand or the field waits. */
int Multiply_Hold(void);
/* A Kuriboh of `side` on the field, as a record, or -1. */
int Multiply_Kuriboh(int side);
#endif
