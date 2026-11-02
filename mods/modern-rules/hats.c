/* Magical Hats, as Yugi plays it against Kaiba at the castle (the setting
 * "magical_hats"; the card is this mod's, a copy of Fake Trap). Set on the
 * field, it springs as Fake Trap does when the other side attacks (the
 * disc's attack traps, Duel_SelectAttackTrap): the attack stops. Then, when
 * a monster was attacked, up to three hats go up in its owner's free
 * monster zones and the monster is hidden among them: it goes face down in
 * defense into one of the hats' zones, picked at random, the others hold
 * decoys ("Magical Hat", 0/0, face down in defense). The next attacks pick a
 * hat by its zone, as Kaiba does: a decoy dies with no damage, the monster
 * fights as itself.
 * - The decoys are made from the trap's own deck card, spent as it sprang
 *   (func_80024D34, as the ritual makes a card): it holds the hats' picture,
 *   so a decoy turned over shows hats, never the monster hidden.
 * - A decoy does nothing: it does not attack or change its position
 *   (DuelCard_CanActThisTurn), is in no graveyard (spells.c) and goes when
 *   the monster hidden among the hats leaves the field.
 * - With no free zone the hats do not go up: the trap only stopped the
 *   attack. A direct attack, the same. */
#include "types.h"
#include "game/display_object.h"
#include "game/display_object_work_slots.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_can_act_this_turn.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_side_state.h"
#include "game/duel_trap_resolution.h"
#include "game/sound.h"
#include "pc/cards/monster_effects_duel.h"
#include "unmatched.h"
#include "hats.h"
#include "spells.h"
#include "state.h"
#include <string.h>

#define T (gModernRules_State.hats)
#define FAKE_TRAP 690
#define HATS_SOUND 0x0D
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;
static void *original_select, *original_can_act;
static int hats_id, hat_id;
static unsigned seed = 0x2545F491u;

static int on(void) { return host && host->applied(host) && host->setting(host, "magical_hats", 1); }
static int card_id(const char *identity, int *cached)
{
    if (*cached <= 0 && host->card_id) *cached = host->card_id(host, identity);
    return *cached;
}
static int is_hats(int id) { return id > 0 && id == card_id("modern-rules:magical_hats:1", &hats_id); }
int Hats_IsDecoy(int id) { return id > 0 && host && id == card_id("modern-rules:magical_hat:1", &hat_id); }

static int occupied(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }
static int deck_of(DuelCardRecord *card) { return card->data ? (u8)((DuelDeckCardRecord *)card->data)->deck_index : 0xFF; }
static unsigned roll(unsigned n)
{
    seed ^= (unsigned)(host->api >= 2 && host->now_us ? host->now_us(host) : 0);
    seed = seed * 1103515245u + 12345u;
    return n ? (seed >> 16) % n : 0;
}

/* The attack's trap picked: Magical Hats springs as Fake Trap. The attacked
 * monster, if any, is the battle's second object. */
static s32 select_trap(u8 *attacker)
{
    s32 sprang = ((s32 (*)(u8 *))original_select)(attacker);
    if (sprang && on() && D_8009B22A == FAKE_TRAP && D_8009B1B8 < DUEL_CARD_RECORD_COUNT &&
        is_hats(D_801A7AD8[D_8009B1B8].card_id)) {
        DisplayObject *attacked = D_800E9EF0[1];
        T.pending = 1;
        T.side = (s8)!(D_8009B1D5 & 1);
        T.trap_deck = (u8)deck_of(&D_801A7AD8[D_8009B1B8]);
        T.real = (s8)(attacked ? attacked->field_6A : -1);
    }
    return sprang;
}

/* A decoy made on record `r` from the trap's deck card. */
static void decoy(int r)
{
    DuelCardRecord *card = &D_801A7AD8[r];
    DuelDeckCardRecord *deck = &gDuel_aDeckCardRecords[T.trap_deck];
    deck->id = (u16)card_id("modern-rules:magical_hat:1", &hat_id);
    func_80024D34(r, T.trap_deck);
    card->card_id = (s16)deck->id;
    card->flags = (u16)((card->flags | DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_FACE_DOWN |
                         DUEL_CARD_FLAG_DEFENSE_POSITION) & ~DUEL_CARD_FLAG_USED_THIS_TURN);
    card->stat_modifier = card->defense_modifier = 0;
    if (card->object) Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
    T.decoy[T.decoy_count++] = (s8)r;
}

static void raise(void)
{
    int at[1 + HATS_MAX], n = 0, col, j, i, real = T.real;
    DuelCardRecord *card;
    if (real < 0 || real >= DUEL_CARD_RECORD_COUNT || !occupied(real) || real / DUEL_CARD_SIDE_RECORD_COUNT != T.side ||
        T.trap_deck == 0xFF || T.active)
        return;
    at[n++] = real;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE && n < 1 + HATS_MAX; col++)
        if (!occupied(MONSTER_RECORD(T.side, col))) at[n++] = MONSTER_RECORD(T.side, col);
    if (n == 1) return;
    /* The monster into one of the zones, at random. */
    j = (int)roll((unsigned)n);
    if (j) {
        Spells_Move(real, at[j]);
        real = at[j];
    }
    card = &D_801A7AD8[real];
    card->flags |= DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_DEFENSE_POSITION;
    if (card->object) Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
    T.decoy_count = 0;
    for (i = 0; i < n; i++)
        if (at[i] != real) decoy(at[i]);
    T.real = (s8)real;
    T.real_deck = (u8)deck_of(card);
    T.active = 1;
    SD_SEPlayFull(HATS_SOUND);
}

static int decoy_here(int i)
{
    int r = T.decoy[i];
    return occupied(r) && Hats_IsDecoy(D_801A7AD8[r].card_id);
}

int Hats_Hold(void)
{
    int i;
    if (T.pending) {
        T.pending = 0;
        if (on()) raise();
    }
    if (!T.active) return 0;
    if (occupied(T.real) && deck_of(&D_801A7AD8[(int)T.real]) == T.real_deck) return 0;
    /* The monster hidden among the hats has left: the decoys go. */
    for (i = 0; i < T.decoy_count; i++) {
        if (!decoy_here(i)) continue;
        MonsterEffects_Placed(T.decoy[i], 0);
        DuelCard_RemoveFromField(&D_801A7AD8[(int)T.decoy[i]]);
    }
    T.decoy_count = 0;
    T.active = 0;
    return 0;
}

static int can_act(DuelCardRecord *card)
{
    if (Hats_IsDecoy(card->card_id)) return 0;
    return ((int (*)(DuelCardRecord *))original_can_act)(card);
}

int Hats_Install(const MemoriesModHost *from)
{
    host = from;
    hats_id = hat_id = 0;
    memset(&T, 0, sizeof T);
    T.real = -1;
    return host->hook(host, (void *)Duel_SelectAttackTrap, (void *)select_trap, &original_select) &&
        host->hook(host, (void *)DuelCard_CanActThisTurn, (void *)can_act, &original_can_act);
}
