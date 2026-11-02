#ifndef MODERN_RULES_HARPIES_H
#define MODERN_RULES_HARPIES_H
#include "pc/mods/modapi.h"

/* Mai's harpies (harpies.c): Elegant Egotist's fusion and Harpie's Pet
 * Dragon's bonus. 1 when the host took the fusion event. */
int Harpies_Install(const MemoriesModHost *host);
/* Each Harpie's Pet Dragon's bonus brought up to its side's harpies; while
 * nothing is in play (a battle lifts its monsters off their records). */
void Harpies_Update(void);
#endif
