/* Modern rules (mod.json beside this file; README.md for the rules).
 *
 * Built on Extra hand plays, which it requires: that mod deals the cards a
 * turn draws and brings the hand back up, and this one says how many and
 * when, through the table it provides (extra_hand_plays.h).
 * - One card a turn: the turn draws are 1, or a full hand of five on each
 *   side's first turn, which is the opening hand: the side that goes first
 *   draws nothing more that turn, the other is already at the five slots.
 * - No card lost to a fusion: while the player confirms two or more marked
 *   cards, or puts cards onto one of their monsters, the chain they make is
 *   checked step by step as placement plays it (Fusion_Step in
 *   src/pc/cards/fusion.c): each next card must fuse with what is made so
 *   far, or one of the two must equip the other. A chain with a step that
 *   would throw a card away is refused, so a monster on the field is never
 *   replaced by one that does not fuse with it.
 * - Pot of Greed, a card of this mod's manifest: using it draws two cards
 *   into a bonus hand; what does not fit waits for the next draw. The CPU
 *   uses one as soon as it holds it. While the card is being tried out, a
 *   setting keeps the player's trunk and deck at three copies. */
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
#include "game/save_data.h"
#include "game/script_state.h"
#include "game/sound.h"
#include "pc/cards/cards.h"
#include "pc/debug/cheats.h"
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
#define HAND_PLACING 4           /* the player's hand state, picking a field zone */
#define HAND_PLACING_MOVING 0x40 /* its cursor still moving onto the field */
#define MONSTER_ROW 2            /* the field cursor's row of the side's monsters */
#define AI_SELECTION_SIZE 12
#define AI_HAND_SLOT_FIRST 11
#define REFUSED_SOUND 8          /* the hand's cancel sound */
#define POT_IDENTITY "modern-rules:pot-of-greed:1"   /* mod:id:n, the first card of the entry */
#define POT_DRAWS 2
#define TEST_COPIES_MAX 3
#define TEST_CHECK_FRAMES 60      /* how often the trunk is looked at */

static const MemoriesModHost *host;
static const ExtraHandApi *api;
static void *original_hand, *original_run;
static int pot_card;

static int setting(const char *key, int fallback)
{
    return host->setting(host, key, fallback);
}
/* Asked again until it answers: the card tables are built after Init. */
static int pot(void)
{
    if (pot_card <= 0) pot_card = host->card_id(host, POT_IDENTITY);
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
        next = hand_card((s8)side->field_0E);
        return next && step(current, next);
    }
    for (pick = 1; pick <= HAND_SIZE; pick++) {
        for (slot = 0; slot < HAND_SIZE && D_800EA030[slot].active_09 != pick; slot++) { }
        if (slot == HAND_SIZE) break;
        if (!(next = hand_card(slot)) || !(current = step(current, next))) return 0;
    }
    return 1;
}

/* The monster of the player's own that the field cursor is on, or 0. */
static int monster_under_cursor(void)
{
    DuelCardRecord *card;
    if (D_8009B1B4->row != MONSTER_ROW) return 0;
    card = &D_801A7AD8[D_800907D8[D_8009B1B4->row * DUEL_FIELD_ROW_SIZE + D_8009B1B4->col +
                                  D_8009B1D5 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT]];
    return card->flags & DUEL_CARD_FLAG_OCCUPIED ? card->card_id : 0;
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
    int step_kind = D_8009B174 & 0xF, refuse = 0, monster;
    if (setting("fusion_guard", 1) && D_8009B360[D_8009B1D5] < 0 && (pressed & PAD_BUTTON_CONFIRM_MASK) &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED | STATE_LOOKING)) == (STATE_HAND | STATE_STARTED) &&
        (D_8009B174 & HAND_STEP_DONE) && !gDuel_bEffectState && !gDuel_bQuitDialogState) {
        if (step_kind == HAND_IDLE)
            refuse = side->field_15 >= 2 && !chain_keeps_every_card(side, 0);
        else if (step_kind == HAND_PLACING && !(D_8009B174 & HAND_PLACING_MOVING) &&
                 (monster = monster_under_cursor()) != 0)
            refuse = !chain_keeps_every_card(side, monster);
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

/* For trying the card out: with a save loaded, top the player's trunk up so
 * that trunk and deck hold `test_copies` Pot of Greed between them. Not
 * while a deck screen has the trunk copied out (Cheats_ChestOnScreen), as
 * the debug menu's card gifts do. */
static void frame(void)
{
    static int frames;
    unsigned char *trunk;
    int wanted = setting("test_copies", TEST_COPIES_MAX), id = pot(), in_deck = 0, i;
    if (++frames < TEST_CHECK_FRAMES) return;
    frames = 0;
    if (wanted <= 0 || id <= 0 || !Cheats_SaveLoaded() || Cheats_ChestOnScreen()) return;
    if (wanted > TEST_COPIES_MAX) wanted = TEST_COPIES_MAX;
    for (i = 0; i < DECK_SIZE; i++) in_deck += gDuel_awPlayerDeck[i] == id;
    trunk = Cards_ChestSlot(gDuel_awPlayerDeck, id);
    if (trunk && in_deck + *trunk < wanted) *trunk = (unsigned char)(wanted - in_deck);
}

int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    if (from->api < 4) return 0;
    host = from;
    mod->api = 4;
    mod->frame = frame;
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
