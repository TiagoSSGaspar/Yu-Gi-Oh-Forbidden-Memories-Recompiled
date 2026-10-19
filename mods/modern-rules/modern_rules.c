/* Modern rules (mod.json beside this file; README.md for the rules).
 *
 * Built on Extra hand plays, which it requires: that mod deals the cards a
 * turn draws and brings the hand back up, and this one says how many and
 * when, through the table it provides (extra_hand_plays.h).
 * - One card a turn: the turn draws are 1, or a full hand of five on each
 *   side's first turn, which is the opening hand: the side that goes first
 *   draws nothing more that turn, the other is already at the five slots.
 * - No card lost to a fusion: while the player confirms two or more marked
 *   cards, the chain they make is checked step by step as placement plays
 *   it (Fusion_Step in src/pc/cards/fusion.c): each next card must fuse
 *   with what is in hand so far, or one of the two must equip the other.
 *   A chain with a step that would throw a card away is refused.
 * - Pot of Greed, a card of this mod's manifest: using it draws two cards
 *   into a bonus hand; what does not fit waits for the next draw. The CPU
 *   uses one as soon as it holds it. */
#include "types.h"
#include "game/ai.h"
#include "game/card_constants.h"
#include "game/duel_card.h"
#include "game/duel_card_checks.h"
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
#include <string.h>

void DuelScene_UpdateHandActions(void);
extern u8 gDuel_bEffectState;
extern u8 gDuel_bQuitDialogState;
extern unsigned char D_800EAE88[];

#define STATE_HAND 4
#define STATE_MASK 0xF
#define STATE_STARTED 0x8000
#define STATE_LOOKING 0x4000
#define HAND_IDLE 1
#define HAND_STEP_DONE 0x80
#define AI_SELECTION_SIZE 12
#define AI_HAND_SLOT_FIRST 11
#define REFUSED_SOUND 8          /* the hand's cancel sound */
#define POT_IDENTITY "modern-rules:pot-of-greed"
#define POT_DRAWS 2

static const MemoriesModHost *host;
static const ExtraHandApi *api;
static void *original_hand, *original_run;
static int pot_card = -1;

static int setting(const char *key, int fallback)
{
    return host->setting(host, key, fallback);
}
static int pot(void)
{
    if (pot_card < 0) pot_card = host->card_id(host, POT_IDENTITY);
    return pot_card;
}
static int hand_card(int slot)
{
    int order = D_800907CC[slot + D_8009B1D5 * HAND_SIZE];
    int record = order & 0x80 ? (order & 0x7F) + DUEL_CARD_SIDE_RECORD_COUNT : order;
    return D_800E9FF0[D_8009B1D5].hand[slot] >= 0 ? D_801A7AD8[record].card_id : 0;
}

/* Turn draws for Extra hand plays: one a turn after the opening five. */
static int turn_draws(int side, int turn, int room)
{
    (void)side;
    if (!host->applied(host) || !setting("draw_one", 1)) return -1;
    return turn <= 1 ? room : 1;
}

/* Whether the marked cards, in the order they were marked, make a chain
 * placement plays through without throwing one away. */
static int chain_keeps_every_card(void)
{
    int pick, slot, current = 0, next, result;
    for (pick = 1; pick <= HAND_SIZE; pick++) {
        for (slot = 0; slot < HAND_SIZE && D_800EA030[slot].active_09 != pick; slot++) { }
        if (slot == HAND_SIZE) break;
        next = hand_card(slot);
        if (!next) return 0;
        if (!current) { current = next; continue; }
        if ((result = Duel_CheckFusion(current, next)) != 0) current = result;
        else if (Duel_CheckEquip(next, current)) { }
        else if (Duel_CheckEquip(current, next)) current = next;
        else return 0;
    }
    return 1;
}

/* State 4: refuse the confirm of a chain that would lose a card. */
static void hand(void)
{
    DuelSelectionRecord *side = (DuelSelectionRecord *)(D_800E9F10 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE);
    u16 pressed = gInput_wPad1Pressed;
    if (setting("fusion_guard", 1) && D_8009B360[D_8009B1D5] < 0 &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED | STATE_LOOKING)) == (STATE_HAND | STATE_STARTED) &&
        (D_8009B174 & 0xF) == HAND_IDLE && (D_8009B174 & HAND_STEP_DONE) && !gDuel_bEffectState && !gDuel_bQuitDialogState &&
        (pressed & PAD_BUTTON_CONFIRM_MASK) && side->field_15 >= 2 && !chain_keeps_every_card()) {
        SD_SEPlayFull(REFUSED_SOUND);
        gInput_wPad1Pressed = pressed & ~PAD_BUTTON_CONFIRM_MASK;
        ((void (*)(void))original_hand)();
        gInput_wPad1Pressed = pressed;
        return;
    }
    ((void (*)(void))original_hand)();
}

/* The CPU's hand decision: a Pot of Greed in hand is used first, face up,
 * on its own. The retail script (or AI Hard Mode) decides again in the
 * bonus hand that follows. */
static s32 run(void)
{
    s32 result = ((s32 (*)(void))original_run)();
    int slot, kept = 0, id = pot();
    if (!result || id <= 0 || !setting("pot_of_greed", 1) || gAiScript_State.script_base != D_801A8000 ||
        (gDuel_wSceneStateFlags & STATE_MASK) != STATE_HAND)
        return result;
    /* The CPU's snapshot numbers the cards in hand from slot 11, packed. */
    for (slot = 0; slot < HAND_SIZE; slot++) {
        if (D_800E9FF0[D_8009B1D5].hand[slot] < 0) continue;
        if (hand_card(slot) == id) {
            memset(D_800EAE88, 0, AI_SELECTION_SIZE);
            D_800EAE88[0] = (unsigned char)(AI_HAND_SLOT_FIRST + kept);
            break;
        }
        kept++;
    }
    return result;
}

/* Pot of Greed's effect: the card's use asks for a bonus hand that draws.
 * Both of the use's handler calls (b 0 and 1) are taken; the per-frame
 * update (c 1) is left alone, since no retail effect was started. */
static void effect(MemoriesModEvent *event)
{
    if (event->phase != MEMORIES_BEFORE || event->c != 0 || event->a <= 0 || event->a != pot()) return;
    event->handled = 1;
    if (event->b == 0 && setting("pot_of_greed", 1)) api->bonus_hand(POT_DRAWS);
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
    return host->subscribe(host, MEMORIES_EVENT_EFFECT, 0, effect) &&
        host->hook(host, (void *)DuelScene_UpdateHandActions, (void *)hand, &original_hand) &&
        host->hook(host, (void *)AiScript_Run, (void *)run, &original_run);
}
