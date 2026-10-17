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
 * used from it, and goes on to the field.
 *
 * Other mods drive the same machinery through the table this mod provides
 * as "extra-hand-plays:api" (extra_hand_plays.h): how many cards a turn
 * draws, and a bonus hand with cards drawn into it (a draw-two spell).
 * Drawn cards fill the first empty slots, so the hand stays packed to the
 * left; cards that do not fit wait, and are drawn into the next hand. */
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
#include "game/card_constants.h"
#include "game/display_object_helpers.h"
#include "game/duel_deck_card.h"
#include "game/duel_draw_resolution.h"
#include "game/duel_grid.h"
#include "game/duel_rank.h"
#include "game/duel_update_draw_card_slide.h"
#include "game/func_80018004.h"
#include "game/sound.h"
#include "game/input.h"
#include "game/script_state.h"
#include "pc/mods/modapi.h"
#include "unmatched.h"
#include "extra_hand_plays.h"

void DuelScene_UpdateDrawPhase(void);
void DuelScene_UpdateDrawResolution(void);
void DuelScene_UpdateHandActions(void);
void DuelScene_UpdateFieldActions(void);
extern u8 gDuel_bEffectState;
extern u8 gDuel_bQuitDialogState;

#define STATE_DRAW 2
#define STATE_RESOLUTION 3
#define STATE_HAND 4
#define STATE_FIELD 5
#define STATE_MASK 0xF
#define STATE_STARTED 0x8000
#define STATE_LOOKING 0x4000   /* the hand state's L2/R2 look at the field */
#define HAND_IDLE 1            /* the player's hand state, walking the hand */
#define HAND_STEP_DONE 0x80
#define PANEL_SLIDE_OUT 8      /* D_8009B162: slide the panel away, releasing its cards */
#define PAD_RIGHT 0x2000
#define DRAWN 0x4000           /* the draw resolution's every card is out */
#define DRAW_GAP 8             /* frames between drawn cards, as retail */
#define SLIDING_CARD_TAG 1     /* DisplayObject_FindAllocatedByTag: a card still sliding in */

static const MemoriesModHost *host;
static void *original_draw, *original_resolution, *original_hand, *original_field;
static void *original_dim, *original_window;

/* No pointers: kept in save states (register_state). */
static struct {
    u8 plays;     /* counted hand states begun this turn */
    u8 hands;     /* every hand state begun this turn, bonus ones too */
    u8 pending;   /* a hand state ran and the field has not started since */
    u8 redraw;    /* the draw now running only rebuilds the hand */
    u8 skipping;  /* START: the hand panel is sliding away */
    u8 done;      /* skipped: no more hands this turn */
    u8 bonus;     /* bonus hands asked for and not yet begun */
    u8 in_bonus;  /* the hand being rebuilt is a bonus one: not counted */
    u8 bonus_draws; /* cards the next bonus hand draws */
    u8 custom;    /* the draw resolution running is this mod's */
    u8 to_draw;   /* cards it has still to draw */
    u8 queued[2]; /* each side's drawn cards that did not fit, still owed */
} S;

static ExtraHandTurnDraws turn_draws;

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
static int extra_hand(void) { return S.hands >= 2; }
static int lp_left(void)
{
    return D_800E9FF0[0].life_points.signed_value && D_800E9FF0[1].life_points.signed_value;
}

static int another_play(void)
{
    int sides = setting("sides", 0, 0, 2);
    if (!S.pending || S.done || S.plays >= setting("plays", 2, 1, 5)) return 0;
    if ((sides == 1 && !human()) || (sides == 2 && human())) return 0;
    if (gDuel_wSceneStateFlags != STATE_FIELD || gDuel_bEffectState || !lp_left()) return 0;
    return hand_count() > 0;
}
/* A bonus hand needs cards: in hand, or coming. */
static int bonus_play(void)
{
    if (!S.bonus || gDuel_wSceneStateFlags != STATE_FIELD || gDuel_bEffectState || !lp_left()) return 0;
    return hand_count() > 0 || S.bonus_draws + S.queued[D_8009B1D5] > 0;
}

/* State 5. Its first step after a play goes back to the draw instead. */
static void field(void)
{
    int bonus;
    if (!(gDuel_wSceneStateFlags & STATE_STARTED) && ((bonus = bonus_play()) || another_play())) {
        /* Let a panel or camera move still running settle first. */
        if (D_8009B162) return;
        if (bonus) {
            S.bonus--;
            S.in_bonus = 1;
        }
        S.pending = 0;
        S.redraw = 1;
        gDuel_wSceneStateFlags = STATE_DRAW;
        return;
    }
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) S.pending = 0;
    ((void (*)(void))original_field)();
}

/* How many of `total` wanted cards fit; the rest wait. */
static void plan_draws(int total)
{
    int side = D_8009B1D5, room = HAND_SIZE - hand_count();
    if (total > 255) total = 255;
    S.to_draw = (u8)(total < room ? total : room);
    S.queued[side] = (u8)(total - S.to_draw);
    S.custom = 1;
}

/* State 2. A real draw starts a new turn. A rebuilt hand takes the draw's
 * first step -- compact the hand, spawn its cards, slide the panel in --
 * without what belongs to the turn: the Swords countdown, the turn count
 * and clearing the used marks (none are set before the field, but a card
 * effect could have). Either way the cards drawn are this mod's to deal
 * when another mod sets the turn's draws, or when a hand is rebuilt. */
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
        int want;
        S.plays = S.hands = S.pending = S.skipping = S.done = 0;
        S.bonus = S.in_bonus = S.bonus_draws = S.custom = S.to_draw = 0;
        ((void (*)(void))original_draw)();
        /* The draw has counted the turn and packed the hand. */
        want = turn_draws ? turn_draws(D_8009B1D5, side->rank.turns_taken, HAND_SIZE - hand_count()) : -1;
        if (want < 0) S.queued[D_8009B1D5] = 0;   /* the disc's refill to five */
        else plan_draws(want + S.queued[D_8009B1D5]);
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
    plan_draws(S.bonus_draws + S.queued[D_8009B1D5]);
    S.bonus_draws = 0;
}

/* The draw resolution's own loop (duel_draw_resolution.c) with each card
 * dealt into the first empty slot instead of the last ones, so a hand that
 * draws fewer cards than it has room for stays packed to the left, as the
 * hand state and the CPU's hand snapshot both need. */
static void deal(void)
{
    DuelSideState *side = D_8009B1C8;
    DisplayObject *object;
    int slot, order, record;
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) {
        gDuel_wSceneStateFlags |= STATE_STARTED;
        D_8009B1ED = 1;
        if (!S.to_draw) gDuel_wSceneStateFlags |= DRAWN;
    }
    if (!(gDuel_wSceneStateFlags & DRAWN)) {
        if ((s8)--D_8009B1ED > 0) return;
        D_8009B1ED = DRAW_GAP;
        if (side->deck_draw_cursor >= DECK_SIZE) {
            /* Out of cards to draw: the side loses, as at a turn's draw. */
            gDuel_bWinnerSide = D_8009B1D5 ^ 1;
            D_800E9FF0[gDuel_bWinnerSide].rank.result_adjustment = DUEL_RANK_ADJUST_DECK_OUT_WIN;
            S.custom = S.redraw = S.to_draw = 0;
            gDuel_wSceneStateFlags = 0xC;
            return;
        }
        for (slot = 0; slot < HAND_SIZE && side->hand[slot] >= 0; slot++) { }
        if (slot == HAND_SIZE) {
            S.to_draw = 0;
            gDuel_wSceneStateFlags |= DRAWN;
            return;
        }
        order = D_800907CC[slot + D_8009B1D5 * HAND_SIZE];
        record = order & 0x80 ? (order & 0x7F) + DUEL_CARD_SIDE_RECORD_COUNT : order;
        Duel_SetupCardRecord(record, side->deck_draw_cursor);
        object = (DisplayObject *)func_80018004(&D_801A7AD8[record], slot * 60 + 0x14E, 0x92);
        object->field_6C = 1;
        object->field_60 = 0xC;
        object->update = (DisplayObjectCallback)Duel_UpdateDrawCardSlide;
        D_800EA030[slot].object = (u8 *)object;
        side->hand[slot] = ((DuelDeckCardRecord *)D_801A7AD8[record].data)->deck_index;
        side->deck_draw_cursor++;
        if (!--S.to_draw) gDuel_wSceneStateFlags |= DRAWN;
        SD_SEPlayFull(0xA);
        return;
    }
    if (DisplayObject_FindAllocatedByTag(SLIDING_CARD_TAG)) return;
    S.custom = S.redraw = 0;
    gDuel_wSceneStateFlags = hand_count() ? STATE_HAND : STATE_FIELD;
    if (hand_count() == HAND_SIZE && Duel_HasAllExodiaPieces()) gDuel_wSceneStateFlags = 0xE;
}

/* State 3. */
static void resolution(void)
{
    if (S.custom) deal();
    else ((void (*)(void))original_resolution)();
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
        if (!S.in_bonus) S.plays++;
        S.in_bonus = 0;
        S.hands++;
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
 * two disagree, so a short hand keeps to the cards in hand. */
static s32 window(void)
{
    s32 n = ((s32 (*)(void))original_window)();
    int count = hand_count();
    if (count >= HAND_SIZE || gAiScript_State.script_base != D_801A8000 ||
        (gDuel_wSceneStateFlags & STATE_MASK) != STATE_HAND)
        return n;
    return (s8)(n > count ? count : n);
}

/* The table other mods find as "extra-hand-plays:api". */
static void set_turn_draws(ExtraHandTurnDraws fn) { turn_draws = fn; }
static void bonus_hand(int draws)
{
    if (draws < 0) draws = 0;
    if (S.bonus < 255) S.bonus++;
    S.bonus_draws = (u8)(S.bonus_draws + draws > 255 ? 255 : S.bonus_draws + draws);
}
static int queued(int side) { return side == 0 || side == 1 ? S.queued[side] : 0; }
static const ExtraHandApi api = {EXTRA_HAND_API_VERSION, set_turn_draws, bonus_hand, queued};

static void line(const char *text, int row)
{
    int width, height, scale, w, top;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    w = host->text_width(host, text, scale);
    top = (36 + row * 24) * scale;
    host->fill(host, (width - w) / 2 - 8 * scale, top, w + 16 * scale, 20 * scale, 0x000000, 160);
    host->draw_text(host, (width - w) / 2, top + 10 * scale, text, 0xFFFFFF, scale);
}
static int hand_idle(void)
{
    return !S.skipping && human() &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == (STATE_HAND | STATE_STARTED) &&
        (D_8009B174 & 0xF) == HAND_IDLE;
}
/* Bit 0: the skip hint; bits 1 up: cards waiting to be drawn. */
static unsigned hint_signature(void)
{
    unsigned shown = 0;
    if (!hand_idle()) return 0;
    if (setting("hint", 1, 0, 1) && extra_hand() && !S.done) shown |= 1;
    return shown | (unsigned)S.queued[D_8009B1D5] << 1;
}
static void draw_hint(void)
{
    unsigned shown = hint_signature();
    char text[48];
    int row = 0;
    if (shown & 1) line("Extra hand: START to skip", row++);
    if (shown >> 1) {
        snprintf(text, sizeof text, "%u card%s waiting to be drawn", shown >> 1, shown >> 1 == 1 ? "" : "s");
        line(text, row);
    }
}

int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    if (from->api < 4) return 0;
    host = from;
    mod->api = 4;
    mod->overlay = draw_hint;
    mod->overlay_signature = hint_signature;
    return host->register_state(host, &S, sizeof S, 2) &&
        host->provide(host, "api", (void *)&api) &&
        host->hook(host, (void *)DuelScene_UpdateDrawPhase, (void *)draw, &original_draw) &&
        host->hook(host, (void *)DuelScene_UpdateDrawResolution, (void *)resolution, &original_resolution) &&
        host->hook(host, (void *)DuelScene_UpdateHandActions, (void *)hand, &original_hand) &&
        host->hook(host, (void *)DuelScene_UpdateFieldActions, (void *)field, &original_field) &&
        host->hook(host, (void *)func_8001B8B8, (void *)dim, &original_dim) &&
        host->hook(host, (void *)Ai_GetHandSize, (void *)window, &original_window);
}
