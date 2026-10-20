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
 * START in an extra hand -- or in the first, when another mod makes playing
 * a card optional -- slides it away as the hand state does for a card used
 * from it, and goes on to the field.
 *
 * Other mods drive the same machinery through the table this mod provides
 * as "extra-hand-plays:api" (extra_hand_plays.h): how many cards a turn
 * draws, and a bonus hand with cards drawn into it (a draw-two spell).
 * Drawn cards fill the first empty slots, so the hand stays packed to the
 * left; cards that do not fit wait, and are drawn into the next hand. */
#include "types.h"
#define AI_GET_HAND_SIZE_RETURNS_S32 /* Native override serves word-sized callers. */
#define D_800EAE88_VISIBLE
#define D_800EAE88_AS_BYTES
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
#include "game/display_object_work_slots.h"
#include "game/duel_action_lock.h"
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
#include "hand_internal.h"
#include <string.h>

void DuelScene_UpdateStartup(void);
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
#define STATE_PLACEMENT 7      /* placement, as the hand state starts it */
#define CPU_HAND_STEP 2        /* the CPU's hand state: its script, then the cards */
#define AI_HAND_SLOT_FIRST 11  /* the CPU's snapshot: 11-15 the hand */
#define REFUSED_SOUND 8        /* the hand's cancel sound */
#define HAND_IDLE 1            /* the player's hand state, walking the hand */
#define HAND_STEP_DONE 0x80
#define PANEL_SLIDE_OUT 8      /* D_8009B162: slide the panel away, releasing its cards */
#define PAD_RIGHT 0x2000
#define DRAWN 0x4000           /* the draw resolution's every card is out */
#define DRAW_GAP 8             /* frames between drawn cards, as retail */
#define SETTLE_FRAMES 30       /* half a second */
#define SLIDING_CARD_TAG 1     /* DisplayObject_FindAllocatedByTag: a card still sliding in */
#define HIDDEN_Y 0x292         /* below the screen: where the draw makes the hand's cards */
#define SLIDE_IN 0x140         /* a drawn card comes in from this far right (Duel_UpdateDrawCardSlide) */

static const MemoriesModHost *host;
const MemoriesModHost *ehp_host;
EhpState ehp_state;
static void *original_startup, *original_draw, *original_resolution, *original_hand, *original_field;
static void *original_dim, *original_window;

static ExtraHandTurnDraws turn_draws;
static ExtraHandOptionalPlay optional_play;

static int setting(const char *key, int fallback, int low, int high)
{
    int n = host->setting(host, key, fallback);
    return n < low ? low : n > high ? high : n;
}
static int human(void) { return D_8009B360[D_8009B1D5] < 0; }
static int hand_count(void) { return Ehp_Count(D_8009B1D5); }
static DuelSelectionRecord *hand_record(void)
{
    return (DuelSelectionRecord *)(D_800E9F10 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE);
}
static int extra_hand(void) { return S.hands >= 2; }
/* START may end the hand: in an extra one, or in the first when another mod
 * makes playing a card optional. */
static int skippable(void) { return extra_hand() || (optional_play && optional_play()); }
static int lp_left(void)
{
    return D_800E9FF0[0].life_points.signed_value && D_800E9FF0[1].life_points.signed_value;
}

static int monster(int id)
{
    return id > 0 && ((gDuel_adwCardStats[id - 1] >> CARD_STAT_TYPE_SHIFT) & CARD_STAT_TYPE_MASK) < CARD_TYPE_MAGIC;
}
/* The monster plays a turn allows are spent: magic, trap and equip cards
 * only from now on. */
static int monsters_spent(void) { return S.monsters >= setting("monsters", 1, 1, 5); }
/* A card the hand can still play: any while monsters may go down, else one
 * that is not a monster. */
static int playable_left(void)
{
    int slot;
    for (slot = 0; slot < EHP_SLOTS; slot++)
        if (Ehp_Get(D_8009B1D5, slot) >= 0 && (!monsters_spent() || !monster(Ehp_CardId(Ehp_Get(D_8009B1D5, slot)))))
            return 1;
    return 0;
}

static int another_play(void)
{
    int sides = setting("sides", 0, 0, 2);
    if (!S.pending || S.done) return 0;
    if ((sides == 1 && !human()) || (sides == 2 && human())) return 0;
    if (gDuel_wSceneStateFlags != STATE_FIELD || gDuel_bEffectState || !lp_left()) return 0;
    return playable_left();
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
        /* Let a panel or camera move still running settle first, for a
         * while: the field state itself starts over one (it sets its own),
         * and one left running for good must not hold the hand back. */
        if (D_8009B162 && S.settle < SETTLE_FRAMES) {
            S.settle++;
            return;
        }
        S.settle = 0;
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

/* State 1, the duel's start: nothing carries over from the last duel. The
 * side that goes first is dealt its hand from here, without the draw that
 * resets a turn. */
static void startup(void)
{
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) {
        memset(&S, 0, sizeof S);
        memset(S.xhand, -1, sizeof S.xhand);
    }
    ((void (*)(void))original_startup)();
}

/* The panel is laid out for the hand as it will be once drawn. */
static void lay_out(void)
{
    int max = Ehp_HandMax(), count = hand_count() + (S.custom ? S.to_draw : 0);
    S.layout = (u8)(count > max ? max : count);
    S.focus = 0;
}

/* How many of `total` wanted cards fit; the rest wait. */
static void plan_draws(int total)
{
    int side = D_8009B1D5, room = Ehp_HandMax() - hand_count();
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
    /* Cards past the fifth fill the five's gaps first; the rest are spawned
     * beside the five, and the borrowed records start empty. */
    Ehp_Pack(D_8009B1D5);
    Ehp_ClearBorrowed(D_8009B1D5);
    if (!S.redraw) {
        int want;
        S.plays = S.hands = S.pending = S.skipping = S.done = S.monsters = 0;
        S.bonus = S.in_bonus = S.bonus_draws = S.custom = S.to_draw = 0;
        ((void (*)(void))original_draw)();
        Ehp_SpawnExtras(HIDDEN_Y);
        /* The draw has counted the turn and packed the hand. */
        want = turn_draws ? turn_draws(D_8009B1D5, side->rank.turns_taken, Ehp_HandMax() - hand_count()) : -1;
        if (want >= 0) {
            plan_draws(want + S.queued[D_8009B1D5]);
        } else {
            /* The disc's refill to five. A hand still full (a turn ended
             * with START, playing nothing) has none to draw, which the
             * retail draw resolution cannot do: it deals past the fifth
             * slot until the deck runs out. */
            S.queued[D_8009B1D5] = 0;
            if (hand_count() >= HAND_SIZE) plan_draws(0);
        }
        lay_out();
        return;
    }
    swords = side->swords_turns_remaining;
    turns = side->rank.turns_taken;
    for (i = 0; i < DUEL_CARD_RECORD_COUNT; i++)
        used[i] = D_801A7AD8[i].flags & DUEL_CARD_FLAG_USED_THIS_TURN;
    side->swords_turns_remaining = 0;
    ((void (*)(void))original_draw)();
    Ehp_SpawnExtras(HIDDEN_Y);
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
    lay_out();
}

/* The draw resolution's own loop (duel_draw_resolution.c) with each card
 * dealt into the first empty slot instead of the last ones, so a hand that
 * draws fewer cards than it has room for stays packed to the left, as the
 * hand state and the CPU's hand snapshot both need. */
static void deal(void)
{
    DuelSideState *side = D_8009B1C8;
    DisplayObject *object;
    int slot, record, max;
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
        max = Ehp_HandMax();
        for (slot = 0; slot < max && Ehp_Get(D_8009B1D5, slot) >= 0; slot++) { }
        if (slot == max) {
            S.to_draw = 0;
            gDuel_wSceneStateFlags |= DRAWN;
            return;
        }
        record = Ehp_Record(D_8009B1D5, slot);
        Ehp_SetupRecord(record, D_8009B1D5 * DECK_SIZE + side->deck_draw_cursor);
        object = (DisplayObject *)func_80018004(&D_801A7AD8[record], Ehp_LayoutX(slot, S.layout) + SLIDE_IN, 0x92);
        object->field_6C = 1;
        object->field_60 = 0xC;
        object->update = (DisplayObjectCallback)Duel_UpdateDrawCardSlide;
        Ehp_Slot(slot)->object = (u8 *)object;
        Ehp_Arrange(slot);
        Ehp_Set(D_8009B1D5, slot, ((DuelDeckCardRecord *)D_801A7AD8[record].data)->deck_index);
        side->deck_draw_cursor++;
        if (!--S.to_draw) gDuel_wSceneStateFlags |= DRAWN;
        SD_SEPlayFull(0xA);
        return;
    }
    if (DisplayObject_FindAllocatedByTag(SLIDING_CARD_TAG)) return;
    S.custom = S.redraw = 0;
    gDuel_wSceneStateFlags = hand_count() ? STATE_HAND : STATE_FIELD;
    if (hand_count() >= HAND_SIZE && Duel_HasAllExodiaPieces()) gDuel_wSceneStateFlags = 0xE;
}

/* State 3. */
static void resolution(void)
{
    if (S.custom) deal();
    else ((void (*)(void))original_resolution)();
}

/* What the hand state does for a card used from it (its step 5), minus the
 * card: the panel slides away with the cursor and the cards on it; skip()
 * then starts the field. No other hand follows this turn. */
static void slide_away(DuelSelectionRecord *side)
{
    S.skipping = 1;
    S.done = 1;
    D_8009B1B4 = (DuelCardPickCursor *)side;
    D_8009B162 = PANEL_SLIDE_OUT;
}

/* START in an extra hand, or a pass: the field once the panel is gone. */
static int skip(DuelSelectionRecord *side)
{
    if (S.skipping) {
        if (D_8009B162 == 0) {
            S.skipping = 0;
            gDuel_wSceneStateFlags = STATE_FIELD;
        }
        return 1;
    }
    if (!skippable() || !human() || (D_8009B174 & 0xF) != HAND_IDLE || !(D_8009B174 & HAND_STEP_DONE) ||
        (gDuel_wSceneStateFlags & STATE_LOOKING) || side->field_15 != 0 || gDuel_bEffectState ||
        gDuel_bQuitDialogState || !(gInput_wPad1Pressed & PAD_BUTTON_START))
        return 0;
    slide_away(side);
    return 1;
}

/* State 4, under the monster plays: hand_state() runs it. */
static void hand_state(void)
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
    if ((gDuel_wSceneStateFlags & STATE_STARTED) && !(gDuel_wSceneStateFlags & STATE_LOOKING) && human() &&
        Ehp_BigHand()) {
        Ehp_HandActions(side);
        return;
    }
    count = hand_count();
    if (count >= HAND_SIZE || !(gDuel_wSceneStateFlags & STATE_STARTED) || !human() ||
        (D_8009B174 & 0xF) != HAND_IDLE) {
        ((void (*)(void))original_hand)();
        if (!human()) Ehp_RefreshExtras();
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

/* Whether the cards the player's confirm would put down hold a monster: the
 * marked ones, or the one under the cursor when none is marked. */
static int confirm_has_monster(DuelSelectionRecord *side)
{
    int slot, slots = Ehp_BigHand() ? S.layout : HAND_SIZE;
    if (side->field_15 == 0) return monster(Ehp_CardId(Ehp_Get(D_8009B1D5, (s8)side->field_0E)));
    for (slot = 0; slot < slots; slot++)
        if (Ehp_Slot(slot)->active_09 && monster(Ehp_CardId(Ehp_Get(D_8009B1D5, slot)))) return 1;
    return 0;
}

/* Whether the cards just sent to placement (D_800E9EF0) put a monster
 * from the hand down: one, a fusion of them, or one onto a field monster.
 * A hand card's record is one of a side's first five, as placement tells. */
static int placement_has_monster(void)
{
    int i;
    for (i = 0; i < 7 && D_800E9EF0[i]; i++) {
        int record = D_800E9EF0[i]->field_6A;
        if (record % DUEL_CARD_SIDE_RECORD_COUNT < HAND_SIZE && monster(D_801A7AD8[record].card_id)) return 1;
    }
    return 0;
}

/* The CPU's decision, made this frame (AiScript_Run, then the swap into the
 * five): its picks are hand slots 11-15. */
static int decision_has_monster(void)
{
    int i;
    for (i = 0; i < HAND_SIZE && D_800EAE88[i]; i++) {
        int v = D_800EAE88[i];
        if (v >= AI_HAND_SLOT_FIRST && v < AI_HAND_SLOT_FIRST + HAND_SIZE &&
            monster(Ehp_CardId(D_8009B1C8->hand[v - AI_HAND_SLOT_FIRST])))
            return 1;
    }
    return 0;
}

/* State 4. Monster plays: a turn may put down as many monsters as the
 * setting says (a fusion is one), and any number of magic, trap and equip
 * cards. Once the monsters are spent the hand still comes back while it
 * holds another kind of card; the player's confirm of a monster is refused
 * and the CPU passes when it picks one. */
static void hand(void)
{
    DuelSelectionRecord *side = hand_record();
    u16 pressed = gInput_wPad1Pressed, substate = *(u16 *)D_8009B20C;
    int before = gDuel_wSceneStateFlags;
    if ((before & (STATE_STARTED | STATE_LOOKING)) == STATE_STARTED && human() && monsters_spent() &&
        (D_8009B174 & 0xF) == HAND_IDLE && (D_8009B174 & HAND_STEP_DONE) && (pressed & PAD_BUTTON_CONFIRM_MASK) &&
        !gDuel_bEffectState && !gDuel_bQuitDialogState && !S.skipping && confirm_has_monster(side)) {
        SD_SEPlayFull(REFUSED_SOUND);
        gInput_wPad1Pressed = pressed & ~PAD_BUTTON_CONFIRM_MASK;
        hand_state();
        gInput_wPad1Pressed = pressed;
        return;
    }
    hand_state();
    if ((gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == STATE_PLACEMENT && placement_has_monster())
        S.monsters++;
    if (!human() && (before & STATE_STARTED) && (D_8009B174 & 0xF) == CPU_HAND_STEP &&
        (substate & 0x800F) == 0x8000 && (*(u16 *)D_8009B20C & 0xF) == 1 && monsters_spent() &&
        decision_has_monster())
        slide_away(side);
}

/* func_8001B8B8 over every slot, with empty ones skipped. */
static void dim(DuelSelectionRecord *side)
{
    int i, count = Ehp_BigHand() ? S.layout : HAND_SIZE;
    for (i = 0; i < count; i++)
        if (Ehp_Slot(i)->active_09 == 0 && Ehp_Slot(i)->object)
            ((DisplayObject *)Ehp_Slot(i)->object)->field_0C = DUEL_DISPLAY_COLOR_DIMMED;
    if (side->field_15 == 0 && Ehp_Slot((s8)side->field_0E)->object)
        ((DisplayObject *)Ehp_Slot((s8)side->field_0E)->object)->field_0C = DUEL_DISPLAY_COLOR_NORMAL;
}

/* The CPU's hand script searches the hand and then upcoming deck cards,
 * the deck from snapshot slot 11 + cards in hand; func_8001BAF0 takes a
 * pick at slot 16 or above as a deck card. With fewer than five cards the
 * two disagree, so a short hand keeps to the cards in hand. */
static s32 window(void)
{
    s32 n = ((s32 (*)(void))original_window)();
    int count = hand_count(), extras = Ehp_Extras(D_8009B1D5);
    if (gAiScript_State.script_base != D_801A8000 || (gDuel_wSceneStateFlags & STATE_MASK) != STATE_HAND)
        return n;
    /* Past five, the snapshot has the cards past the fifth ahead of the
     * deck: the window takes them in on top of what it looked at. */
    if (extras) return (s8)(n + extras);
    if (count >= HAND_SIZE) return n;
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
static void set_optional_play(ExtraHandOptionalPlay fn) { optional_play = fn; }
static void pass_hand(void)
{
    if ((gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) != (STATE_HAND | STATE_STARTED) || S.skipping)
        return;
    slide_away(hand_record());
}
static int hand_slots(void) { return Ehp_BigHand() ? S.layout : HAND_SIZE; }
static int hand_card(int slot)
{
    if (slot < 0 || slot >= EHP_SLOTS) return 0;
    return Ehp_CardId(Ehp_Get(D_8009B1D5, slot));
}
static int hand_mark(int slot) { return slot < 0 || slot >= EHP_SLOTS ? 0 : Ehp_Slot(slot)->active_09; }
static const ExtraHandApi api = {EXTRA_HAND_API_VERSION, set_turn_draws, bonus_hand, queued, set_optional_play,
                                 pass_hand, Ehp_SetLimits, hand_slots, hand_card, hand_mark};

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
/* Bit 0: the skip hint; bit 1: the monsters are spent; bits 2 up: cards
 * waiting to be drawn. The discard: bit 31, with the cards still to mark. */
static unsigned hint_signature(void)
{
    unsigned shown = 0;
    if (S.discard && human()) return 0x80000000u | S.discard_left;
    if (!hand_idle()) return 0;
    if (setting("hint", 1, 0, 1) && skippable() && !S.done) shown |= 1;
    if (setting("hint", 1, 0, 1) && monsters_spent()) shown |= 2;
    return shown | (unsigned)S.queued[D_8009B1D5] << 2;
}
static void draw_hint(void)
{
    unsigned shown = hint_signature();
    char text[64];
    int row = 0;
    if (shown & 0x80000000u) {
        unsigned left = shown & 0xFF;
        snprintf(text, sizeof text, "Hand limit: discard %u card%s", left, left == 1 ? "" : "s");
        line(text, row++);
        line("UP: mark   X: discard the marked cards", row);
        return;
    }
    if (shown & 1) line(extra_hand() ? "Extra hand: START to skip" : "START: play no card this turn", row++);
    if (shown & 2) line("Monster played: magic, trap and equip cards only", row++);
    if (shown >> 2) {
        snprintf(text, sizeof text, "%u card%s waiting to be drawn", shown >> 2, shown >> 2 == 1 ? "" : "s");
        line(text, row);
    }
}

int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    if (from->api < 4) return 0;
    host = ehp_host = from;
    mod->api = 4;
    mod->overlay = draw_hint;
    mod->overlay_signature = hint_signature;
    memset(S.xhand, -1, sizeof S.xhand);
    return host->register_state(host, &S, sizeof S, EHP_STATE_VERSION) &&
        host->provide(host, "api", (void *)&api) &&
        host->hook(host, (void *)DuelScene_UpdateStartup, (void *)startup, &original_startup) &&
        host->hook(host, (void *)DuelScene_UpdateDrawPhase, (void *)draw, &original_draw) &&
        host->hook(host, (void *)DuelScene_UpdateDrawResolution, (void *)resolution, &original_resolution) &&
        host->hook(host, (void *)DuelScene_UpdateHandActions, (void *)hand, &original_hand) &&
        host->hook(host, (void *)DuelScene_UpdateFieldActions, (void *)field, &original_field) &&
        host->hook(host, (void *)func_8001B8B8, (void *)dim, &original_dim) &&
        host->hook(host, (void *)Ai_GetHandSize, (void *)window, &original_window) &&
        Ehp_Install() && Ehp_DiscardInstall();
}
