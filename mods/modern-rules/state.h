#ifndef MODERN_RULES_STATE_H
#define MODERN_RULES_STATE_H
#include "types.h"
#include "game/duel_card_layout.h"

/* The mod's one state buffer: the host keeps a single one per mod
 * (register_state), so the moth's evolution and the equips share it. */
#define MODERN_RULES_STATE_VERSION 3

typedef struct {
    u8 turns[DUEL_CARD_RECORD_COUNT];  /* 0: no cocoon; else 1 + its owner's turns since it was made */
    s16 card[DUEL_CARD_RECORD_COUNT];  /* the card the record held when last seen */
    s16 due[DUEL_CARD_RECORD_COUNT];   /* the card it is to become, 0 for none */
    u8 made[2];                        /* turns_taken + 1 of the side whose placement fused the cocoon */
    s8 evolving;                       /* the record evolving, -1 for none */
    u8 step;
} MothState;

/* An equip on the field, by its magic and trap record. */
typedef struct {
    s16 equip_card;    /* the equip, 0 for none */
    s16 monster_card;  /* the monster it equips, as it was then */
    s8 monster;        /* the monster's record */
    u8 monster_deck;   /* and its deck card: the same card in the same record */
    s16 attack;        /* what it gave: to stat_modifier */
    s16 defense;       /* and to defense_modifier */
} EquipLink;

/* An equip a placement has put onto the monster it plays, waiting for the
 * monster to land in its zone. */
typedef struct {
    s16 card;
    u8 deck;
    s8 from_col;       /* its magic and trap zone when used from the field, else -1 */
    s16 attack, defense;
} EquipPending;

#define EQUIPS_PENDING_MAX 5

typedef struct {
    EquipLink link[DUEL_CARD_RECORD_COUNT];
    EquipPending pending[EQUIPS_PENDING_MAX];
    EquipPending measuring;   /* the equip being applied */
    s16 before_attack, before_defense;
    s8 measured;              /* the monster's record it is applied to, -1 for none */
    u8 pending_count;
    u8 reversed;              /* Reverse Trap turned this placement's equips round */
    u8 cooldown;              /* frames before the next equip is destroyed */
} EquipState;

typedef struct {
    MothState moth;
    EquipState equips;
} ModernRulesState;

extern ModernRulesState gModernRules_State;
#endif
