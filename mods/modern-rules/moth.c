/* Petit Moth's evolution, as in the anime (the setting "moth_evolution"):
 * Cocoon of Evolution put onto a Petit Moth, or the two marked in the hand,
 * fuse into the cocoon, which comes down in defense. At its owner's 4th turn
 * after, it becomes Great Moth; at the 6th, Perfectly Ultimate Great Moth.
 * Only a cocoon made of the moth evolves: the fusion marks its side while
 * the placement plays it (the CPU's script, the fusion guard and the fusion
 * helper ask for fusions in the hand), and that side's next summoned Cocoon
 * in the same turn is the one. The evolution is the ritual's
 * (duel_ritual_effect.c): the new card's picture loaded, stored into the
 * card's picture block, the record remade; the hand state waits for it. */
#include "types.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect_request.h"
#include "game/duel_effect_resource_record.h"
#include "game/duel_effect_resource_setup.h"
#include "game/duel_scene_state.h"
#include "game/duel_side_state.h"
#include "game/file_transfer.h"
#include "game/sound.h"
#include "unmatched.h"
#include "moth.h"
#include <stdio.h>
#include <string.h>

#define MOTH_PETIT 278
#define MOTH_COCOON 72
#define MOTH_GREAT 57
#define MOTH_PERFECT 67
#define MOTH_GREAT_TURN 4
#define MOTH_PERFECT_TURN 6
#define SCENE_MASK 0xF
#define SCENE_PLACEMENT 7
#define MOTH_STATE_VERSION 1

static const MemoriesModHost *host;
static struct {
    u8 turns[DUEL_CARD_RECORD_COUNT];  /* 0: no cocoon; else 1 + its owner's turns since it was made */
    s16 card[DUEL_CARD_RECORD_COUNT];  /* the card the record held when last seen */
    s16 due[DUEL_CARD_RECORD_COUNT];   /* the card it is to become, 0 for none */
    u8 made[2];                        /* turns_taken + 1 of the side whose placement fused the cocoon */
    s8 evolving;                       /* the record evolving, -1 for none */
    u8 step;
} S;

static int on(void) { return host->applied(host) && host->setting(host, "moth_evolution", 1); }
static int moth_pair(int a, int b)
{
    return (a == MOTH_PETIT && b == MOTH_COCOON) || (a == MOTH_COCOON && b == MOTH_PETIT);
}

static void fusion(MemoriesModEvent *e)
{
    if (e->phase != MEMORIES_BEFORE || !on() || !moth_pair(e->a, e->b)) return;
    e->result = MOTH_COCOON;
    e->handled = 1;
    if ((gDuel_wSceneStateFlags & SCENE_MASK) == SCENE_PLACEMENT)
        S.made[D_8009B1D5 & 1] = (u8)(D_800E9FF0[D_8009B1D5 & 1].rank.turns_taken + 1);
}

/* A cocoon whose record lost it (destroyed, fused, replaced) is forgotten. */
static void forget_gone(void)
{
    int r;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++)
        if (S.turns[r] && r != S.evolving &&
            (!(D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) || D_801A7AD8[r].card_id != S.card[r]))
            S.turns[r] = 0, S.due[r] = 0;
}

static void monster(MemoriesModEvent *e)
{
    int r = e->b, side;
    if (e->phase != MEMORIES_AFTER || !on() || r < 0 || r >= DUEL_CARD_RECORD_COUNT) return;
    side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    if (e->c == MEMORIES_MONSTER_SUMMON) {
        if (e->a == MOTH_COCOON && S.made[side] == (u8)(D_800E9FF0[side].rank.turns_taken + 1)) {
            S.turns[r] = 1;
            S.card[r] = MOTH_COCOON;
            S.due[r] = 0;
            D_801A7AD8[r].flags |= DUEL_CARD_FLAG_DEFENSE_POSITION;
            Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)D_801A7AD8[r].object);
        }
        S.made[side] = 0;
    }
}

int Moth_Hold(void)
{
    return 0;
}

int Moth_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    (void)mod;
    host = from;
    memset(&S, 0, sizeof S);
    S.evolving = -1;
    if (host->api < 10) return 1;   /* no monster events: no evolution, nothing else changes */
    return host->subscribe(host, MEMORIES_EVENT_FUSION, 0, fusion) > 0 &&
        host->subscribe(host, MEMORIES_EVENT_MONSTER, 0, monster) > 0 &&
        host->register_state(host, &S, sizeof S, MOTH_STATE_VERSION);
}
