/* How well the CPU plays the anime's rules. As in the anime, the duelists
 * of little skill fall for every trick, and the bosses see through them and
 * turn them on the player. The level comes from the game's own opponent
 * table: byte 7 of gDuel_aOpponentData, the will to use Harpie's Feather
 * Duster, is 25, 50 or 75 and follows how strong each opponent is
 * (notes/ai-hard-mode-research.md). With AI Hard Mode applied (it shares
 * "ai-hard-mode:on") every opponent is a boss.
 *
 *   beginner (25)  attacks the wall of Kuribohs for ever, does not see the
 *                  swamp brothers fight together, plays none of the combos;
 *   middle (50)    learns the wall after two attacks on it, sees the
 *                  brothers half of the time, plays the combos when behind;
 *   boss (75)      learns the wall at the first attack and breaks it with
 *                  Harpie's Feather Duster, always sees the brothers, and
 *                  plays every combo as soon as it can.
 *
 * The CPU's decisions are the bytes at D_800EAE88 its scripts leave
 * (ai.h): for the hand, the cards at 0-4 (snapshot slots: 11-15 its hand),
 * the zone at 6 (1-5 its monster row's columns, 6-10 its magic and trap
 * row's) and face down at 8; for the field, the attacker at 9 (1-5) and the
 * target at 10 (0x38 up: 0x3C less the column of the other side's monster
 * row), 10 and 11 both 0 being no action. */
#include "types.h"
#include "game/ai.h"
#include "game/ai_opponent_data.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_grid.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "unmatched.h"
#include "cpu.h"
#include "multiply.h"
#include "swamp.h"
#include <string.h>

extern unsigned char D_800EAE88[];
#define AI_SELECTION_SIZE 12
#define AI_ZONE 6
#define AI_FACE_DOWN 8
#define AI_ATTACKER 9
#define AI_TARGET 10
#define AI_TARGET_FLAG 11
#define AI_TARGET_ROW 0x38
#define AI_TARGET_BASE 0x3C
#define AI_HAND_FIRST 11
#define AI_SPELL_ZONE 6
#define DUSTER_WILL 7
#define GRID(side, row, col) D_800907D8[(row) * DUEL_FIELD_ROW_SIZE + (col) + (side) * DUEL_FIELD_SIDE_GRID_SLOT_COUNT]
#define THEIR_MONSTERS 1
#define MY_MONSTERS 2
#define MY_SPELLS 3

static const MemoriesModHost *cpu_host;
static int (*hard_on)(void);
static unsigned coin;   /* the middle's "half of the time" */

void Cpu_Install(const MemoriesModHost *from)
{
    cpu_host = from;
    hard_on = 0;
    coin = 0x9E3779B9u;
}

int Cpu_Level(void)
{
    int id = gDuel_bOpponentID, will;
    if (!hard_on && cpu_host && cpu_host->api >= 4 && cpu_host->find) hard_on = (int (*)(void))cpu_host->find(cpu_host, "ai-hard-mode:on");
    if (hard_on && hard_on()) return CPU_BOSS;
    if (id < 1 || id >= AI_OPPONENT_COUNT) return CPU_MIDDLE;
    will = gDuel_aOpponentData[id].values[DUSTER_WILL];
    return will >= 75 ? CPU_BOSS : will >= 50 ? CPU_MIDDLE : CPU_BEGINNER;
}

static int cpu_occupied(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }
static int cpu_face_up(int r)
{
    return (D_801A7AD8[r].flags & (DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_FACE_DOWN)) == DUEL_CARD_FLAG_OCCUPIED;
}
static int cpu_attack(int r) { return Duel_CalcCardStats(&D_801A7AD8[r]) & 0xFFFF; }
static int cpu_defense(int r) { return (Duel_CalcCardStats(&D_801A7AD8[r]) >> 16) & 0xFFFF; }

int Cpu_Behind(void)
{
    int side = D_8009B1D5 & 1, col, theirs = -1, mine = -1;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        int t = GRID(side, THEIR_MONSTERS, col), m = GRID(side, MY_MONSTERS, col);
        if (cpu_face_up(t) && cpu_attack(t) > theirs) theirs = cpu_attack(t);
        if (cpu_occupied(m) && cpu_attack(m) > mine) mine = cpu_attack(m);
    }
    return theirs > mine;
}

/* Whether the CPU sees through the wall: it has attacked it as often as
 * its level needs. */
static int sees_wall(int side, int level)
{
    int attacks = Multiply_Attacks(side);
    return level == CPU_BOSS ? attacks >= 1 : level == CPU_MIDDLE ? attacks >= 2 : 0;
}

/* Whether the CPU sees the swamp brothers fight together, this decision. */
static int sees_brothers(int level)
{
    if (level == CPU_BOSS) return 1;
    if (level == CPU_BEGINNER) return 0;
    coin = coin * 1103515245u + 12345u;
    return (coin >> 16) & 1;
}

void Cpu_Attack(void)
{
    int side = D_8009B1D5 & 1, from = D_800EAE88[AI_ATTACKER], to = D_800EAE88[AI_TARGET], level, attack, col,
        best = -1, margin = 0, wall, brothers, target;
    if (from < 1 || from > DUEL_FIELD_ROW_SIZE || to < AI_TARGET_ROW || to > AI_TARGET_BASE) return;
    level = Cpu_Level();
    wall = sees_wall(side, level);
    brothers = sees_brothers(level);
    attack = cpu_attack(GRID(side, MY_MONSTERS, from - 1));
    target = GRID(side, THEIR_MONSTERS, AI_TARGET_BASE - to);
    if (!(wall && Multiply_Walled(target)) && !(brothers && Swamp_Together(target) >= attack)) return;
    /* Useless: the monster it beats by the most instead, or nothing. */
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        int r = GRID(side, THEIR_MONSTERS, col), value;
        if (!cpu_occupied(r) || (wall && Multiply_Walled(r))) continue;
        value = brothers && Swamp_Together(r) ? Swamp_Together(r) :
                D_801A7AD8[r].flags & DUEL_CARD_FLAG_DEFENSE_POSITION ? cpu_defense(r) : cpu_attack(r);
        if (attack - value > margin) {
            margin = attack - value;
            best = col;
        }
    }
    if (best >= 0) {
        D_800EAE88[AI_TARGET] = (u8)(AI_TARGET_BASE - best);
    } else {
        D_800EAE88[AI_TARGET] = D_800EAE88[AI_TARGET_FLAG] = 0;
    }
}

void Cpu_Play(int slot, int col, int spell)
{
    memset(D_800EAE88, 0, AI_SELECTION_SIZE);
    D_800EAE88[0] = (u8)slot;
    D_800EAE88[AI_ZONE] = (u8)(spell ? AI_SPELL_ZONE + col : col + 1);
    D_800EAE88[AI_FACE_DOWN] = 0;
}

int Cpu_HandSlot(int card)
{
    int slot;
    if (card <= 0) return 0;
    for (slot = AI_HAND_FIRST; slot < AI_HAND_FIRST + DUEL_FIELD_ROW_SIZE; slot++)
        if (gDuel_aActiveCards[slot].card_id == card) return slot;
    return 0;
}

int Cpu_FreeZone(int spell)
{
    int side = D_8009B1D5 & 1, col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (!cpu_occupied(GRID(side, spell ? MY_SPELLS : MY_MONSTERS, col))) return col;
    return -1;
}

/* The swamp brothers together on the CPU's field: a boss as soon as it can,
 * a middle one when behind. The brother of a battleguard on its field, or
 * one of the two when both are in its hand. 1 when it played one. */
int Cpu_Brothers(void)
{
    int side = D_8009B1D5 & 1, level = Cpu_Level(), col, slot, free = Cpu_FreeZone(0);
    if (level == CPU_BEGINNER || (level == CPU_MIDDLE && !Cpu_Behind()) || free < 0) return 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        int r = GRID(side, MY_MONSTERS, col), brother;
        if (!cpu_face_up(r) || !(brother = Swamp_Brother(D_801A7AD8[r].card_id))) continue;
        if (Swamp_Together(r)) return 0;   /* together already */
        if ((slot = Cpu_HandSlot(brother))) {
            Cpu_Play(slot, free, 0);
            return 1;
        }
    }
    for (slot = AI_HAND_FIRST; slot < AI_HAND_FIRST + DUEL_FIELD_ROW_SIZE; slot++) {
        int id = gDuel_aActiveCards[slot].card_id, brother = Swamp_Brother(id);
        if (brother && Cpu_HandSlot(brother)) {
            Cpu_Play(slot, free, 0);
            return 1;
        }
    }
    return 0;
}
