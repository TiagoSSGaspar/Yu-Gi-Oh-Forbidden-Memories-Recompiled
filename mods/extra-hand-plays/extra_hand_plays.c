/* Extra hand plays (mod.json beside this file; README.md for the rules).
 *
 * The game's turn runs draw (scene state 2), draw resolution (3), hand (4),
 * then the play itself (6 card use, 7 placement, 8) and the field (5). The
 * hand only comes up once because only the draw leads to it. This mod sends
 * the first entry into the field back through the draw instead, with the
 * draw's turn-level work undone and the draw resolution skipped, so the
 * game rebuilds the remaining hand exactly as it does at a turn start --
 * compacted to the left, slid in on its panel -- and runs its own hand state
 * again, for the player or for the CPU's hand script.
 *
 * What the retail hand state assumes and a short hand breaks is covered
 * here, and only while the hand has fewer than five cards, which retail
 * never shows:
 * - the player's hand state colours and dims all five slot objects:
 *   its first colour pass is done here and func_8001B8B8 is replaced by a
 *   copy that skips empty slots;
 * - the player's cursor walks all five slots: a step right past the last
 *   card is masked;
 * - the CPU's swap of upcoming deck cards into the hand (func_8001BAF0)
 *   assumes the deck starts at snapshot slot 16, which only holds for five
 *   cards: its hand window is held to the cards actually in hand.
 * START in an extra hand slides it away as the hand state does for a card
 * used from it, and goes on to the field. */
#include "types.h"
#define AI_GET_HAND_SIZE_RETURNS_S32 /* Native override serves word-sized callers. */
#include "game/ai.h"
#include "game/card_constants.h"
#include "game/display_object.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_display.h"
#include "game/duel_hand.h"
#include "game/duel_scene_state.h"
#include "game/duel_selection_layout.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/func_8001B8B8.h"
#include "game/input.h"
#include "game/script_state.h"
#include "pc/mods/modapi.h"
#include "unmatched.h"

void DuelScene_UpdateDrawPhase(void);
void DuelScene_UpdateDrawResolution(void);
void DuelScene_UpdateHandActions(void);
void DuelScene_UpdateFieldActions(void);
extern u8 gDuel_bEffectState;
extern u8 gDuel_bQuitDialogState;

#define STATE_DRAW 2
#define STATE_HAND 4
#define STATE_FIELD 5
#define STATE_MASK 0xF
#define STATE_STARTED 0x8000
#define STATE_LOOKING 0x4000   /* the hand state's L2/R2 look at the field */
#define HAND_IDLE 1            /* the player's hand state, walking the hand */
#define HAND_STEP_DONE 0x80
#define PANEL_SLIDE_OUT 8      /* D_8009B162: slide the panel away, releasing its cards */
#define PAD_RIGHT 0x2000

static const MemoriesModHost *host;
static void *original_draw, *original_resolution, *original_hand, *original_field;
static void *original_dim, *original_window;

/* No pointers: kept in save states (register_state). */
static struct {
    u8 plays;     /* hand states begun this turn */
    u8 pending;   /* a hand state ran and the field has not started since */
    u8 redraw;    /* the draw now running only rebuilds the hand */
    u8 skipping;  /* START: the hand panel is sliding away */
    u8 done;      /* skipped: no more hands this turn */
} S;

static int setting(const char *key, int fallback, int low, int high)
{
    int n = host->setting(host, key, fallback);
    return n < low ? low : n > high ? high : n;
}
static int human(void) { return D_8009B360[D_8009B1D5] < 0; }
static int hand_count(void)
{
    int i, n = 0;
    for (i = 0; i < HAND_SIZE; i++) n += D_800E9FF0[D_8009B1D5].hand[i] >= 0;
    return n;
}
static DuelSelectionRecord *hand_record(void)
{
    return (DuelSelectionRecord *)(D_800E9F10 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE);
}
static int extra_hand(void) { return S.plays >= 2; }

static int another_play(void)
{
    int sides = setting("sides", 0, 0, 2);
    if (!S.pending || S.done || S.plays >= setting("plays", 2, 1, 5)) return 0;
    if ((sides == 1 && !human()) || (sides == 2 && human())) return 0;
    if (gDuel_wSceneStateFlags != STATE_FIELD || gDuel_bEffectState) return 0;
    if (!D_800E9FF0[0].life_points.signed_value || !D_800E9FF0[1].life_points.signed_value) return 0;
    return hand_count() > 0;
}

/* State 5. Its first step after a play goes back to the draw instead. */
static void field(void)
{
    if (!(gDuel_wSceneStateFlags & STATE_STARTED) && another_play()) {
        /* Let a panel or camera move still running settle first. */
        if (D_8009B162) return;
        S.pending = 0;
        S.redraw = 1;
        gDuel_wSceneStateFlags = STATE_DRAW;
        return;
    }
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) S.pending = 0;
    ((void (*)(void))original_field)();
}

/* State 2. A real draw starts a new turn. A rebuilt hand takes the draw's
 * first step -- compact the hand, spawn its cards, slide the panel in --
 * without what belongs to the turn: the Swords countdown, the turn count
 * and clearing the used marks (none are set before the field, but a card
 * effect could have). */
static void draw(void)
{
    DuelSideState *side = &D_800E9FF0[D_8009B1D5];
    u16 used[DUEL_CARD_RECORD_COUNT];
    s8 swords;
    u8 turns;
    int i;
    if (gDuel_wSceneStateFlags & STATE_STARTED) {
        ((void (*)(void))original_draw)();
        return;
    }
    if (!S.redraw) {
        S.plays = S.pending = S.skipping = S.done = 0;
        ((void (*)(void))original_draw)();
        return;
    }
    swords = side->swords_turns_remaining;
    turns = side->rank.turns_taken;
    for (i = 0; i < DUEL_CARD_RECORD_COUNT; i++)
        used[i] = D_801A7AD8[i].flags & DUEL_CARD_FLAG_USED_THIS_TURN;
    side->swords_turns_remaining = 0;
    ((void (*)(void))original_draw)();
    side->swords_turns_remaining = swords;
    side->rank.turns_taken = turns;
    for (i = 0; i < DUEL_CARD_RECORD_COUNT; i++) {
        DuelCardRecord *card = &D_801A7AD8[i];
        /* Hand records were just rebuilt; the field keeps its marks. */
        if (i % DUEL_CARD_SIDE_RECORD_COUNT < HAND_SIZE || !used[i]) continue;
        if (!(card->flags & DUEL_CARD_FLAG_OCCUPIED)) continue;
        card->flags |= DUEL_CARD_FLAG_USED_THIS_TURN;
        if (card->object) Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
    }
}

/* State 3. A rebuilt hand draws nothing: straight to the hand. */
static void resolution(void)
{
    if (!S.redraw) {
        ((void (*)(void))original_resolution)();
        return;
    }
    S.redraw = 0;
    gDuel_wSceneStateFlags = STATE_HAND;
}

/* START in an extra hand: what the hand state does for a card used from
 * it (its step 5), minus the card -- the panel slides away with the cursor
 * and the cards on it, then the field. */
static int skip(DuelSelectionRecord *side)
{
    if (S.skipping) {
        if (D_8009B162 == 0) {
            S.skipping = 0;
            gDuel_wSceneStateFlags = STATE_FIELD;
        }
        return 1;
    }
    if (!extra_hand() || !human() || (D_8009B174 & 0xF) != HAND_IDLE || !(D_8009B174 & HAND_STEP_DONE) ||
        (gDuel_wSceneStateFlags & STATE_LOOKING) || side->field_15 != 0 || gDuel_bEffectState ||
        gDuel_bQuitDialogState || !(gInput_wPad1Pressed & PAD_BUTTON_START))
        return 0;
    S.skipping = 1;
    S.done = 1;
    D_8009B1B4 = (DuelCardPickCursor *)side;
    D_8009B162 = PANEL_SLIDE_OUT;
    return 1;
}

/* State 4. */
static void hand(void)
{
    DuelSelectionRecord *side = hand_record();
    int count, i;
    u16 repeat;
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) {
        S.plays++;
        S.pending = 1;
        S.skipping = 0;
    } else if (skip(side)) {
        return;
    }
    count = hand_count();
    if (count >= HAND_SIZE || !(gDuel_wSceneStateFlags & STATE_STARTED) || !human() ||
        (D_8009B174 & 0xF) != HAND_IDLE) {
        ((void (*)(void))original_hand)();
        return;
    }
    /* The idle step's first frame colours all five slots: do it for the
     * cards there are. */
    if (!(D_8009B174 & HAND_STEP_DONE) && !(gDuel_wSceneStateFlags & STATE_LOOKING)) {
        D_8009B174 |= HAND_STEP_DONE;
        for (i = 0; i < HAND_SIZE; i++)
            if (D_800EA030[i].object) ((DisplayObject *)D_800EA030[i].object)->field_0C = DUEL_DISPLAY_COLOR_NORMAL;
    }
    repeat = gInput_wPad1Repeat;
    if ((s8)side->field_0E + 1 >= count) gInput_wPad1Repeat = repeat & ~PAD_RIGHT;
    ((void (*)(void))original_hand)();
    gInput_wPad1Repeat = repeat;
}

/* func_8001B8B8 with empty slots skipped. */
static void dim(DuelSelectionRecord *side)
{
    int i;
    for (i = 0; i < HAND_SIZE; i++)
        if (D_800EA030[i].active_09 == 0 && D_800EA030[i].object)
            ((DisplayObject *)D_800EA030[i].object)->field_0C = DUEL_DISPLAY_COLOR_DIMMED;
    if (side->field_15 == 0 && D_800EA030[(s8)side->field_0E].object)
        ((DisplayObject *)D_800EA030[(s8)side->field_0E].object)->field_0C = DUEL_DISPLAY_COLOR_NORMAL;
}

/* The CPU's hand script searches the hand and then upcoming deck cards,
 * the deck from snapshot slot 11 + cards in hand; func_8001BAF0 takes a
 * pick at slot 16 or above as a deck card. With fewer than five cards the
 * two disagree, so an extra hand keeps to the cards in hand. */
static s32 window(void)
{
    s32 n = ((s32 (*)(void))original_window)();
    int count;
    if (!extra_hand() || gAiScript_State.script_base != D_801A8000 ||
        (gDuel_wSceneStateFlags & STATE_MASK) != STATE_HAND)
        return n;
    count = hand_count();
    return (s8)(n > count ? count : n);
}

static void overlay(void)
{
    static const char text[] = "Extra hand: START to skip";
    int width, height, scale, w;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    w = host->text_width(host, text, scale);
    host->fill(host, (width - w) / 2 - 8 * scale, 36 * scale, w + 16 * scale, 20 * scale, 0x000000, 160);
    host->draw_text(host, (width - w) / 2, 46 * scale, text, 0xFFFFFF, scale);
}
static int hint_shown(void)
{
    return setting("hint", 1, 0, 1) && extra_hand() && !S.skipping && !S.done && human() &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == (STATE_HAND | STATE_STARTED) &&
        (D_8009B174 & 0xF) == HAND_IDLE;
}
static void draw_hint(void)
{
    if (hint_shown()) overlay();
}
static unsigned hint_signature(void) { return hint_shown(); }

int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    if (from->api < 4) return 0;
    host = from;
    mod->api = 4;
    mod->overlay = draw_hint;
    mod->overlay_signature = hint_signature;
    return host->register_state(host, &S, sizeof S, 1) &&
        host->hook(host, (void *)DuelScene_UpdateDrawPhase, (void *)draw, &original_draw) &&
        host->hook(host, (void *)DuelScene_UpdateDrawResolution, (void *)resolution, &original_resolution) &&
        host->hook(host, (void *)DuelScene_UpdateHandActions, (void *)hand, &original_hand) &&
        host->hook(host, (void *)DuelScene_UpdateFieldActions, (void *)field, &original_field) &&
        host->hook(host, (void *)func_8001B8B8, (void *)dim, &original_dim) &&
        host->hook(host, (void *)Ai_GetHandSize, (void *)window, &original_window);
}
