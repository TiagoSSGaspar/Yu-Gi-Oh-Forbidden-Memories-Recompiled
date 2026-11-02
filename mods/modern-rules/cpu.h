#ifndef MODERN_RULES_CPU_H
#define MODERN_RULES_CPU_H
#include "pc/mods/modapi.h"

/* How well the CPU plays the anime's rules (cpu.c), by the game's own
 * opponent table: a beginner falls for every trick, a boss sees through
 * them and plays them against the player. */
#define CPU_BEGINNER 0
#define CPU_MIDDLE 1
#define CPU_BOSS 2

void Cpu_Install(const MemoriesModHost *host);
int Cpu_Level(void);
/* Whether the CPU is behind: a face-up monster of the player's stronger
 * than all of its own. */
int Cpu_Behind(void);
/* The CPU's field decision: an attack that the wall of Kuribohs or the
 * swamp brothers would make useless, for a CPU that sees it, goes to the
 * player's monster the attacker beats by the most, or nowhere. */
void Cpu_Attack(void);
/* The CPU's hand decision, written: the hand card in snapshot slot `slot`
 * down, face up, in monster zone `col` (onto the monster there, for an
 * equip), or in the magic and trap row's column `col` when `spell`. */
void Cpu_Play(int slot, int col, int spell);
/* The snapshot slot of `card` in the CPU's hand (its first five), or 0. */
int Cpu_HandSlot(int card);
/* The CPU's first free monster zone, or -1. */
int Cpu_FreeZone(int spell);
/* The CPU's hand decision: the swamp brothers together on its field. 1 when
 * it played one of them. */
int Cpu_Brothers(void);
#endif
