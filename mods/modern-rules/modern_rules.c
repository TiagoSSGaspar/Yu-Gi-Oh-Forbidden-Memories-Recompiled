/* Duelist Kingdom rules (mod.json beside this file; README.md for the rules).
 *
 * Built on Extra hand plays, which it requires: that mod deals the cards a
 * turn draws and brings the hand back up, and this one says how many and
 * when, through the table it provides (extra_hand_plays.h).
 * - Playing a card is optional: START ends the first hand too.
 * - One card a turn: the turn draws are 1, or the opening five and the
 *   turn's card on a side's first draw. The side that goes first is dealt
 *   its five before the duel's first turn, which has no draw; the other
 *   side's first draw is its five and one. The hand holds up to forty, a
 *   whole deck, with no limit at the end of a turn (the Battle City's six is for a mod of
 *   its own, as is its Pot of Greed).
 * - No card lost to a fusion: while the player confirms two or more marked
 *   cards, or puts cards onto one of their monsters, the chain they make is
 *   checked step by step as placement plays it (Fusion_Step in
 *   src/pc/cards/fusion.c): each next card must fuse with what is made so
 *   far, or one of the two must equip the other. A chain with a step that
 *   would throw a card away is refused, so a monster on the field is never
 *   replaced by one that does not fuse with it. An equip used from the
 *   field must equip the monster the player puts it onto (or fuse with it),
 *   which the disc would throw away otherwise. The CPU's hand decision is
 *   held to the same rule: its cards go where none is lost, or it passes.
 * - Petit Moth's evolution, as in the anime: moth.c.
 * - Equips that stay on the field: equips.c. An equip that would have no
 *   free magic and trap zone to stay in is refused, for the player and the
 *   CPU alike, and one on the field is not used again. */
#include "types.h"
#include "game/ai.h"
#include "game/card_constants.h"
#include "game/duel_card.h"
#include "game/duel_card_checks.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_pick_cursor.h"
#include "game/duel_grid.h"
#include "game/duel_hand.h"
#include "game/duel_scene_state.h"
#include "game/duel_selection_layout.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/input.h"
#include "game/script_state.h"
#include "game/sound.h"
#include "pc/mods/modapi.h"
#include "unmatched.h"
#include "../extra-hand-plays/extra_hand_plays.h"
#include "moth.h"
#include "equips.h"
#include "state.h"
#include <string.h>

void DuelScene_UpdateHandActions(void);
void DuelScene_UpdateFieldActions(void);
extern u8 gDuel_bEffectState;
extern u8 gDuel_bQuitDialogState;
extern unsigned char D_800EAE88[];
extern u8 D_801A9800[];   /* the CPU's field script */

#define STATE_HAND 4
#define STATE_FIELD 5
#define STATE_MASK 0xF
#define STATE_STARTED 0x8000
#define STATE_LOOKING 0x4000
#define HAND_IDLE 1
#define HAND_STEP_DONE 0x80
#define HAND_PLACING 4           /* the player's hand state, picking a field zone */
#define HAND_PLACING_MOVING 0x40 /* its cursor still moving onto the field */
#define MONSTER_ROW 2            /* the field cursor's row of the side's monsters */
#define SPELL_ROW 3              /* and of its magic and trap cards */
#define FIELD_CPU 2              /* the CPU's field state, its script picking a move */
#define FIELD_IDLE 3             /* the player's field state, walking the field */
#define FIELD_TARGETING 6        /* the player's field state: the monster an equip from the field goes onto */
#define FIELD_TARGET_LEAVING 0x40 /* its cursor going back to the field (circle) */
#define FIELD_CURSOR_RECORD 2    /* the side's selection records: the field cursor, */
#define TARGET_CURSOR_RECORD 3   /* the cursor picking the equip's monster */
#define AI_SELECTION_SIZE 12
#define AI_PICKS 5               /* D_800EAE88[0..4]: the cards, in the order they go down */
#define AI_ZONE 6                /* D_800EAE88[6]: the zone, 1-5 the monster row's columns */
#define AI_FIELD_CARD 9          /* D_800EAE88[9]: the field card the CPU uses, 6-10 its magic and trap zones */
#define AI_SPELL_SLOT 6
#define AI_PLAY_FIELD_CARD 2     /* AiScript_Run: a field card played, */
#define AI_END_FIELD 3           /* the field turn over */
#define AI_HAND_SLOT_FIRST 11    /* snapshot slots: 11-15 the hand, 16 up the upcoming deck */
#define AI_ENEMY_SLOT_FIRST 56
#define REFUSED_SOUND 8          /* the hand's cancel sound */
#define OPENING_HAND 5
#define HAND_MAX 40

ModernRulesState gModernRules_State;
static const MemoriesModHost *host;
static const ExtraHandApi *api;
static void *original_hand, *original_run, *original_field;

static int setting(const char *key, int fallback)
{
    return host->setting(host, key, fallback);
}
/* The hand of the side whose turn it is, slot by slot: every slot through
 * Extra hand plays (version 4), the five on the disc's otherwise. */
static int hand_slots(void) { return api->version >= 4 ? api->hand_slots() : HAND_SIZE; }
static int hand_card(int slot)
{
    int order, record;
    if (api->version >= 4) return api->hand_card(slot);
    order = D_800907CC[slot + D_8009B1D5 * HAND_SIZE];
    record = order & 0x80 ? (order & 0x7F) + DUEL_CARD_SIDE_RECORD_COUNT : order;
    return D_800E9FF0[D_8009B1D5].hand[slot] >= 0 ? D_801A7AD8[record].card_id : 0;
}
static int hand_mark(int slot) { return api->version >= 4 ? api->hand_mark(slot) : D_800EA030[slot].active_09; }
/* The card under the player's hand cursor: its place on screen is the
 * card's until the hand passes ten (Extra hand plays version 6). */
static int hand_cursor(DuelSelectionRecord *side) { return api->version >= 6 ? api->hand_cursor() : (s8)side->field_0E; }
/* The phase menu open (Extra hand plays version 7): the confirm is the
 * menu's, never refused. */
static int menu_open(void) { return api->version >= 7 && api->menu_open(); }

static int draw_one(void) { return host->applied(host) && setting("draw_one", 1); }

/* Turn draws for Extra hand plays: one a turn after the opening. The
 * opening is a side's draw with nothing drawn yet, not its first counted
 * turn: the side that goes first has its five dealt before the duel's first
 * turn, which has no draw and does not count; the other side draws its five
 * and its turn's card. With the hand held to five (Extra hand plays older
 * than 0.5.0) the opening is what fits. */
static int turn_draws(int side, int turn, int room)
{
    (void)turn;
    if (!draw_one()) return -1;
    if (D_800E9FF0[side].deck_draw_cursor != 0) return 1;
    return api->version >= 4 ? OPENING_HAND + 1 : room;
}

/* Up to forty cards in hand, and no limit at the end of a turn. */
static void hand_limits(int side, int *max, int *keep)
{
    (void)side;
    if (!draw_one()) return;
    *max = HAND_MAX;
    *keep = 0;
}

/* Playing a card is optional: with a full field and nothing that fuses, a
 * hand may have no legal play once a monster can no longer be replaced. */
static int optional_play(void)
{
    return host->applied(host) && setting("optional_play", 1);
}
/* The player's turn in phases (Extra hand plays'
 * set_phases): START opens a menu of the next phases, and Main Phase 2
 * brings the hand back after the field. */
static int gx_phases(void)
{
    return host->applied(host) && setting("gx_phases", 1);
}

static int kind(int id) { return (gDuel_adwCardStats[id - 1] >> CARD_STAT_TYPE_SHIFT) & CARD_STAT_TYPE_MASK; }

/* One step of a chain as placement plays it: a fusion, or one card
 * equipping the other; 0 when the step would throw a card away. */
static int step(int current, int next)
{
    int result;
    if (!current) return next;
    if ((result = Duel_CheckFusion(current, next)) != 0) return result;
    if (Duel_CheckEquip(next, current)) return current;
    if (Duel_CheckEquip(current, next)) return next;
    return 0;
}

/* Whether the cards going down, in order -- `first` (a field monster they
 * go onto, or 0), then the marked cards in the order they were marked, or
 * the card under the cursor when none is marked -- make a chain placement
 * plays through without throwing one away. */
static int chain_keeps_every_card(DuelSelectionRecord *side, int first)
{
    int pick, slot, current = first, next;
    if (side->field_15 == 0) {
        next = hand_card(hand_cursor(side));
        return next && step(current, next);
    }
    for (pick = 1; pick <= HAND_SIZE; pick++) {
        int slots = hand_slots();
        for (slot = 0; slot < slots && hand_mark(slot) != pick; slot++) { }
        if (slot == slots) break;
        if (!(next = hand_card(slot)) || !(current = step(current, next))) return 0;
    }
    return 1;
}

/* The cards going down from the hand, in order: the marked cards in the
 * order they were marked, or the card under the cursor when none is
 * marked. Their count. */
static int chain_cards(DuelSelectionRecord *side, int *ids)
{
    int pick, slot, count = 0;
    if (side->field_15 == 0) {
        ids[0] = hand_card(hand_cursor(side));
        return ids[0] ? 1 : 0;
    }
    for (pick = 1; pick <= HAND_SIZE; pick++) {
        int slots = hand_slots();
        for (slot = 0; slot < slots && hand_mark(slot) != pick; slot++) { }
        if (slot == slots) break;
        ids[count++] = hand_card(slot);
    }
    return count;
}

/* Whether the equips that would stay on the field after `ids` go down onto
 * `first` (a monster, or 0) each have a free magic and trap zone. */
static int equips_fit(int first, const int *ids, int count)
{
    return !Equips_On() || Equips_Kept(first, ids, count) <= Equips_FreeZones(D_8009B1D5 & 1);
}

/* The monster in column `col` of the side's own monster row, or 0. */
static int monster_at(int col)
{
    DuelCardRecord *card = &D_801A7AD8[D_800907D8[MONSTER_ROW * DUEL_FIELD_ROW_SIZE + col +
                                                   D_8009B1D5 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT]];
    return card->flags & DUEL_CARD_FLAG_OCCUPIED ? card->card_id : 0;
}

/* The card in column `col` of the side's own magic and trap row, or 0. */
static int spell_at(int col)
{
    DuelCardRecord *card = &D_801A7AD8[D_800907D8[SPELL_ROW * DUEL_FIELD_ROW_SIZE + col +
                                                   D_8009B1D5 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT]];
    return card->flags & DUEL_CARD_FLAG_OCCUPIED ? card->card_id : 0;
}

/* The monster of the player's own that the field cursor is on, or 0. */
static int monster_under_cursor(void)
{
    return D_8009B1B4->row == MONSTER_ROW ? monster_at(D_8009B1B4->col) : 0;
}

/* State 4, for the player: refuse a confirm that would lose a card. In the
 * hand, two or more marked cards that do not all fuse or equip in order;
 * on the field, cards put onto one of the player's monsters that do not
 * fuse with it or equip it -- the original game puts them down in its
 * place and throws it away. */
static void hand(void)
{
    DuelSelectionRecord *side = (DuelSelectionRecord *)(D_800E9F10 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE);
    u16 pressed = gInput_wPad1Pressed;
    int step_kind = D_8009B174 & 0xF, refuse = 0, monster, ids[HAND_SIZE], count;
    /* A cocoon evolving (moth.c), an equip destroyed with its monster
     * (equips.c): the hand waits for it, from its start. */
    if (step_kind <= HAND_IDLE && (Moth_Hold() || Equips_Hold())) return;
    if (D_8009B360[D_8009B1D5] < 0 && (pressed & PAD_BUTTON_CONFIRM_MASK) && !menu_open() &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED | STATE_LOOKING)) == (STATE_HAND | STATE_STARTED) &&
        (D_8009B174 & HAND_STEP_DONE) && !gDuel_bEffectState && !gDuel_bQuitDialogState) {
        int guard = setting("fusion_guard", 1);
        if (step_kind == HAND_IDLE)
            refuse = guard && side->field_15 >= 2 && !chain_keeps_every_card(side, 0);
        else if (step_kind == HAND_PLACING && !(D_8009B174 & HAND_PLACING_MOVING)) {
            monster = monster_under_cursor();
            if (guard && monster) refuse = !chain_keeps_every_card(side, monster);
            /* The equips that stay need free magic and trap zones. */
            if (!refuse && D_8009B1B4->row == MONSTER_ROW && (count = chain_cards(side, ids)) > 0)
                refuse = !equips_fit(monster, ids, count);
        }
    }
    if (refuse) {
        SD_SEPlayFull(REFUSED_SOUND);
        gInput_wPad1Pressed = pressed & ~PAD_BUTTON_CONFIRM_MASK;
        ((void (*)(void))original_hand)();
        gInput_wPad1Pressed = pressed;
        return;
    }
    ((void (*)(void))original_hand)();
}

/* State 5, for the player: an equip used from the field goes onto a monster
 * the player picks, and the disc puts the two down as a chain, equip first;
 * one that neither equips that monster nor fuses with it is thrown away.
 * The pick is refused instead. */
static void field(void)
{
    DuelSelectionRecord *records = (DuelSelectionRecord *)(D_800E9F10 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE);
    DuelSelectionRecord *spell = &records[FIELD_CURSOR_RECORD], *target = &records[TARGET_CURSOR_RECORD];
    u16 pressed = gInput_wPad1Pressed;
    int equip, monster;
    /* A cocoon destroyed in the player's battle lets its moth out (moth.c)
     * once the field is walked again. */
    if ((D_8009B174 & 0xF) == FIELD_IDLE && Moth_Hold()) return;
    /* An equip destroyed with its monster (equips.c), before the player
     * walks the field or the CPU picks its next move. */
    if (((D_8009B174 & 0xF) == FIELD_IDLE || ((D_8009B174 & 0xF) == FIELD_CPU && !(D_8009B174 & 0x80))) &&
        Equips_Hold())
        return;
    /* An equip on the field is not used again. */
    if (D_8009B360[D_8009B1D5] < 0 && (pressed & PAD_BUTTON_CONFIRM_MASK) && (D_8009B174 & 0xF) == FIELD_IDLE &&
        !menu_open() &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == (STATE_FIELD | STATE_STARTED) &&
        !gDuel_bEffectState && !gDuel_bQuitDialogState && spell->row == SPELL_ROW &&
        Equips_InZone(D_8009B1D5 & 1, spell->col)) {
        SD_SEPlayFull(REFUSED_SOUND);
        gInput_wPad1Pressed = pressed & ~PAD_BUTTON_CONFIRM_MASK;
        ((void (*)(void))original_field)();
        gInput_wPad1Pressed = pressed;
        return;
    }
    if (setting("fusion_guard", 1) && D_8009B360[D_8009B1D5] < 0 && (pressed & PAD_BUTTON_CONFIRM_MASK) &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == (STATE_FIELD | STATE_STARTED) &&
        (D_8009B174 & 0xF) == FIELD_TARGETING && !(D_8009B174 & FIELD_TARGET_LEAVING) && !gDuel_bEffectState &&
        !gDuel_bQuitDialogState && spell->row == SPELL_ROW && (equip = spell_at(spell->col)) != 0 &&
        kind(equip) == CARD_TYPE_EQUIP && target->row == MONSTER_ROW && (monster = monster_at(target->col)) != 0 &&
        !step(equip, monster)) {
        SD_SEPlayFull(REFUSED_SOUND);
        gInput_wPad1Pressed = pressed & ~PAD_BUTTON_CONFIRM_MASK;
        ((void (*)(void))original_field)();
        gInput_wPad1Pressed = pressed;
        return;
    }
    ((void (*)(void))original_field)();
}

/* Whether `ids`, going down in order after `first` (a monster they go onto,
 * or 0), play through without throwing one away. An equip alone needs a
 * monster to go onto. */
static int keeps_every_card(int first, const int *ids, int count)
{
    int i, current = first;
    if (!setting("fusion_guard", 1)) return equips_fit(first, ids, count);
    if (!first && count == 1 && kind(ids[0]) == CARD_TYPE_EQUIP) return 0;
    for (i = 0; i < count; i++)
        if (!(current = step(current, ids[i]))) return 0;
    return equips_fit(first, ids, count);
}

/* The zone (D_800EAE88[6]) the first `count` picks can go to without a
 * card thrown away: the one chosen if they keep every card there, else an
 * empty zone, else a monster they fuse with or equip; 0 for none. */
static int zone_for(const int *ids, int count, int col)
{
    int i;
    if (keeps_every_card(monster_at(col), ids, count)) return D_800EAE88[AI_ZONE];
    for (i = 0; i < DUEL_FIELD_ROW_SIZE; i++)
        if (!monster_at(i) && keeps_every_card(0, ids, count)) return i + 1;
    for (i = 0; i < DUEL_FIELD_ROW_SIZE; i++)
        if (monster_at(i) && keeps_every_card(monster_at(i), ids, count)) return i + 1;
    return 0;
}

/* The CPU's hand decision, held to the rule the player's confirm is held
 * to. Cards put onto one of its monsters must fuse with it or equip it, and
 * a chain must not throw a card away: the cards go to a zone where the
 * whole chain keeps every card, else the longest first part of it does (the
 * rest stay in hand). With no such zone the CPU plays no card, when playing
 * is optional; when it is not, its own choice stands. The picks are the
 * CPU's snapshot slots; a magic card used, or one set in the spell and trap
 * row, is not checked, as for the player. */
static void hold_cpu_to_rule(void)
{
    int ids[AI_PICKS], count, col, zone, n;
    for (count = 0; count < AI_PICKS && D_800EAE88[count]; count++) {
        int slot = D_800EAE88[count];
        if (slot < AI_HAND_SLOT_FIRST || slot >= AI_ENEMY_SLOT_FIRST) return;
        if ((ids[count] = gDuel_aActiveCards[slot].card_id) <= 0) return;
    }
    if (!count || (count == 1 && kind(ids[0]) >= CARD_TYPE_MAGIC && kind(ids[0]) != CARD_TYPE_EQUIP)) return;
    col = D_800EAE88[AI_ZONE] >= 6 ? D_800EAE88[AI_ZONE] - 6 : D_800EAE88[AI_ZONE] - 1;
    if (col < 0 || col >= DUEL_FIELD_ROW_SIZE) return;
    for (n = count; n >= 1; n--) {
        if ((zone = zone_for(ids, n, col)) != 0) {
            memset(D_800EAE88 + n, 0, AI_PICKS - n);
            D_800EAE88[AI_ZONE] = (unsigned char)zone;
            return;
        }
    }
    if (!optional_play() || api->version < 3) return;
    memset(D_800EAE88, 0, AI_SELECTION_SIZE);
    api->pass_hand();
}

/* The CPU's hand decision, held to the fusion rule. */
static s32 run(void)
{
    s32 result = ((s32 (*)(void))original_run)();
    /* An equip on the field is not used again: should the CPU's field
     * script still pick one, its field turn ends there. */
    if (result == AI_PLAY_FIELD_CARD && gAiScript_State.script_base == D_801A9800 &&
        (gDuel_wSceneStateFlags & STATE_MASK) == STATE_FIELD && D_800EAE88[AI_FIELD_CARD] >= AI_SPELL_SLOT &&
        D_800EAE88[AI_FIELD_CARD] < AI_SPELL_SLOT + DUEL_FIELD_ROW_SIZE &&
        Equips_InZone(D_8009B1D5 & 1, D_800EAE88[AI_FIELD_CARD] - AI_SPELL_SLOT))
        return AI_END_FIELD;
    if (!result || gAiScript_State.script_base != D_801A8000 || (gDuel_wSceneStateFlags & STATE_MASK) != STATE_HAND)
        return result;
    if (setting("fusion_guard", 1) || Equips_On()) hold_cpu_to_rule();
    return result;
}

int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    if (from->api < 4) return 0;
    host = from;
    mod->api = 4;
    api = (const ExtraHandApi *)host->find(host, "extra-hand-plays:api");
    if (!api || api->version < EXTRA_HAND_API_VERSION) {
        host->log(host, "modern-rules: Extra hand plays %s", api ? "is too old" : "is not loaded");
        return 0;
    }
    api->set_turn_draws(turn_draws);
    if (api->version >= 2) api->set_optional_play(optional_play);
    if (api->version >= 4) api->set_hand_limits(hand_limits);
    if (api->version >= 5) api->set_phases(gx_phases);
    return host->hook(host, (void *)DuelScene_UpdateHandActions, (void *)hand, &original_hand) &&
        host->hook(host, (void *)DuelScene_UpdateFieldActions, (void *)field, &original_field) &&
        host->hook(host, (void *)AiScript_Run, (void *)run, &original_run) &&
        Moth_Install(host, mod) && Equips_Install(host, mod) &&
        (host->api < 10 ||
         host->register_state(host, &gModernRules_State, sizeof gModernRules_State, MODERN_RULES_STATE_VERSION));
}
