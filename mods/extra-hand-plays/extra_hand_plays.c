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
#include "game/duel_check_quit_input.h"
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
#define STATE_TURN_SWITCH 0xA  /* the end of the turn: the discard, then the other side's */
#define FIELD_IDLE 3           /* the player's field state, walking the field */
#define FIELD_STEP_DONE 0x80
#define FIELD_END 0xB          /* its step for START: the field closes, then the turn switch */
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

/* A text the mod draws, in the player's language (host API 22: its
 * lang/<language>.json's "texts"); the English on an older host. */
static const char *T(const char *english) { return host->api >= 22 ? host->text(host, english) : english; }

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
    for (slot = 0; slot < EHP_HAND; slot++)
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
    /* In phases a Main Phase lasts until the player leaves it: the hand
     * comes back while it holds a card, playable or not. */
    return Ehp_Phases() ? hand_count() > 0 : playable_left();
}
/* A bonus hand needs cards: in hand, or coming. */
static int bonus_play(void)
{
    if (!S.bonus || gDuel_wSceneStateFlags != STATE_FIELD || gDuel_bEffectState || !lp_left()) return 0;
    return hand_count() > 0 || S.bonus_draws + S.queued[D_8009B1D5] > 0;
}

/* The field's START, in phases: the menu instead of the end of the turn,
 * holding the field state while it is open. Either phase taken closes the
 * field as START does; Main Phase 2 then comes back at the turn switch
 * (Ehp_MainTwo). */
static int field_menu(void)
{
    int item;
    if ((gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) != (STATE_FIELD | STATE_STARTED) ||
        (D_8009B174 & 0xF) != FIELD_IDLE || !(D_8009B174 & FIELD_STEP_DONE) || gDuel_bEffectState ||
        gDuel_bQuitDialogState || !Ehp_Phases()) {
        S.menu = 0;
        return 0;
    }
    item = Ehp_PhaseMenu(1);
    if (item <= 0) return item < 0;
    if (item == PHASE_MAIN2) S.main2 = 1;
    D_8009B174 = FIELD_END;
    return 1;
}

/* State 5. Its first step after a play goes back to the draw instead; after
 * the last play of Main Phase 2 the turn ends, with no field. */
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
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) {
        S.pending = 0;
        if (S.main2 == 2) {
            gDuel_wSceneStateFlags = STATE_TURN_SWITCH;
            return;
        }
    }
    if (field_menu()) return;
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
    if (count > max) count = max;
    S.layout = (u8)(count > EHP_VIEW ? EHP_VIEW : count);
    S.focus = 0;
}

/* The player's draw that found the hand full: when the notice of it goes,
 * and how many cards that draw left waiting (draw_hint). Not saved: a
 * notice, not the duel's state. */
#define FULL_NOTICE_US 3000000
static uint64_t full_notice_until;
static unsigned full_notice_drawn;

/* How many of `total` wanted cards fit; the rest wait. */
static void plan_draws(int total)
{
    int side = D_8009B1D5, room = Ehp_HandMax() - hand_count(), before = S.queued[side];
    if (total > 255) total = 255;
    S.to_draw = (u8)(total < room ? total : room);
    S.queued[side] = (u8)(total - S.to_draw);
    S.custom = 1;
    if (S.queued[side] > before && human()) {
        full_notice_until = host->now_us(host) + FULL_NOTICE_US;
        full_notice_drawn = (unsigned)(S.queued[side] - before);
    }
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
    S.view = 0;
    memset(S.mark, 0, sizeof S.mark);
    Ehp_Pack(D_8009B1D5);
    Ehp_ClearBorrowed(D_8009B1D5);
    if (!S.redraw) {
        int want;
        S.plays = S.hands = S.pending = S.skipping = S.done = S.monsters = 0;
        S.bonus = S.in_bonus = S.bonus_draws = S.custom = S.to_draw = 0;
        S.main2 = S.menu = S.to_end = 0;
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
    int slot, record, max, position, screen;
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
        position = D_8009B1D5 * DECK_SIZE + side->deck_draw_cursor;
        /* The player's window moves on to the card coming in. */
        if (slot >= S.view + EHP_VIEW && human()) Ehp_Show(slot - EHP_VIEW + 1);
        if (slot >= S.view + EHP_VIEW) {
            /* Past the window: into the hand, with nothing on screen. */
            Ehp_Set(D_8009B1D5, slot, (s8)position);
        } else {
            screen = slot - S.view;
            record = Ehp_Record(D_8009B1D5, screen);
            Ehp_SetupRecord(record, position);
            object = (DisplayObject *)func_80018004(&D_801A7AD8[record], Ehp_LayoutX(screen, S.layout) + SLIDE_IN,
                                                    0x92);
            object->field_6C = 1;
            object->field_60 = 0xC;
            object->update = (DisplayObjectCallback)Duel_UpdateDrawCardSlide;
            Ehp_Slot(screen)->object = (u8 *)object;
            Ehp_Arrange(screen);
            Ehp_Set(D_8009B1D5, slot, ((DuelDeckCardRecord *)D_801A7AD8[record].data)->deck_index);
        }
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

/* START in an extra hand, or a pass: the field once the panel is gone --
 * or, in phases, the End Phase when the menu took it. With phases the
 * menu decides, and holds the hand state while it is open. */
static int skip(DuelSelectionRecord *side)
{
    int item;
    if (S.skipping) {
        if (D_8009B162 == 0) {
            S.skipping = 0;
            gDuel_wSceneStateFlags = S.to_end ? STATE_TURN_SWITCH : STATE_FIELD;
            S.to_end = 0;
        }
        return 1;
    }
    if (!skippable() || !human() || (D_8009B174 & 0xF) != HAND_IDLE || !(D_8009B174 & HAND_STEP_DONE) ||
        (gDuel_wSceneStateFlags & STATE_LOOKING) || side->field_15 != 0 || gDuel_bEffectState ||
        gDuel_bQuitDialogState) {
        S.menu = 0;
        return 0;
    }
    if (!Ehp_Phases()) {
        if (!(gInput_wPad1Pressed & PAD_BUTTON_START)) return 0;
        slide_away(side);
        return 1;
    }
    item = Ehp_PhaseMenu(0);
    if (item == PHASE_BATTLE || item == PHASE_END) {
        S.to_end = item == PHASE_END;
        slide_away(side);
        return 1;
    }
    return item < 0;
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
    int card;
    if (side->field_15 == 0) return monster(Ehp_CardId(Ehp_Get(D_8009B1D5, Ehp_Cursor())));
    for (card = 0; card < EHP_HAND; card++)
        if (Ehp_Mark(card) && monster(Ehp_CardId(Ehp_Get(D_8009B1D5, card)))) return 1;
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
        !gDuel_bEffectState && !gDuel_bQuitDialogState && !S.skipping && !S.menu && confirm_has_monster(side)) {
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
/* Up to the last card held: five for a hand that never passed them. */
static int hand_slots(void)
{
    int n = Ehp_BigHand() ? EHP_HAND : HAND_SIZE;
    while (n > HAND_SIZE && Ehp_Get(D_8009B1D5, n - 1) < 0) n--;
    return n;
}
static int hand_card(int card)
{
    if (card < 0 || card >= EHP_HAND) return 0;
    return Ehp_CardId(Ehp_Get(D_8009B1D5, card));
}
static const ExtraHandApi api = {EXTRA_HAND_API_VERSION, set_turn_draws, bonus_hand, queued, set_optional_play,
                                 pass_hand, Ehp_SetLimits, hand_slots, hand_card, Ehp_Mark, Ehp_SetPhases,
                                 Ehp_Cursor};

/* A hint row, centred under the strip above the FIELD box (the picture's
 * lines 0-23), which the port's View > Fusion helper takes: between the
 * FIELD box and the life points, above the field. */
#define HINT_RGB 0xFFFFFF
/* The phase menu's panel: dark blue, a gold frame, the entry under the
 * cursor on a blue bar. */
#define MENU_PANEL_RGB 0x101828
#define MENU_FRAME_RGB 0xC8A040
#define MENU_BAR_RGB 0x2F4FA0
#define MENU_TITLE_RGB 0xF0C860
#define MENU_PICKED_RGB 0xFFFFFF
#define MENU_OTHER_RGB 0xA8B0C0
static void line(const char *text, int row, uint32_t rgb)
{
    int width, height, scale, w, top;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    w = host->text_width(host, text, scale);
    top = height * 26 / 240 + row * 24 * scale;
    host->fill(host, (width - w) / 2 - 8 * scale, top, w + 16 * scale, 20 * scale, 0x000000, 160);
    host->draw_text(host, (width - w) / 2, top + 10 * scale, text, rgb, scale);
}
static int hand_idle(void)
{
    return !S.skipping && human() &&
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == (STATE_HAND | STATE_STARTED) &&
        (D_8009B174 & 0xF) == HAND_IDLE;
}
static int field_idle(void)
{
    return human() && (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == (STATE_FIELD | STATE_STARTED) &&
        (D_8009B174 & 0xF) == FIELD_IDLE;
}
static const char *const phase_names[] = {"", "Main Phase 1", "Battle Phase", "Main Phase 2", "End Phase"};
/* The menu's entries say what each next phase does for the player. On the
 * duel's first turn no monster may attack (D_8009B16C's bit, which the disc
 * sets as the duel starts and clears as that turn ends; the field refuses
 * an attack while it is set), so the Battle Phase is only the field. */
static const char *const phase_actions[] = {"", "", "Attack", "Play more cards", "End turn"};
#define DUEL_FIRST_TURN 0x1000
static const char *phase_action(int phase)
{
    return phase == PHASE_BATTLE && (D_8009B16C & DUEL_FIRST_TURN) ? "Go to the field" : phase_actions[phase];
}
/* The phase on screen, in phases: the hand's Main Phase or the field's
 * Battle Phase; 0 for none. */
static int phase_shown(void)
{
    if (!Ehp_Phases()) return 0;
    if (field_idle()) return PHASE_BATTLE;
    if (hand_idle()) return S.main2 == 2 ? PHASE_MAIN2 : PHASE_MAIN1;
    return 0;
}
/* The window's place, for a player's hand past ten: bits 10-15 and 27-30
 * of the hints hold (count - 11) * 31 + view + 1 (1 to 930), 0 for none. */
static unsigned window_bits(void)
{
    int count = Ehp_Count(D_8009B1D5);
    unsigned e;
    if (!human() || count <= EHP_VIEW) return 0;
    e = (unsigned)((count - EHP_VIEW - 1) * 31 + S.view + 1);
    return (e & 0x3F) << 10 | (e >> 6) << 27;
}

/* Bit 0: the skip hint; bit 1: the monsters are spent; bits 2-9: cards
 * waiting to be drawn; bits 10-15 and 27-30: the window (window_bits);
 * bits 16-18: the phase shown; 19-20: the phase menu's cursor, 21-22 its
 * entries, 23-25 its first. The discard: bit 31, with the cards still to
 * mark and the window. The hand-full notice is hint_signature's bit 26,
 * beside all of these. */
static unsigned hint_bits(void)
{
    unsigned shown = 0;
    int phase = phase_shown(), items[2];
    if (S.discard && human()) return 0x80000000u | S.discard_left | window_bits();
    if (phase) {
        shown = (unsigned)phase << 16;
        if (S.menu) {
            int n = Ehp_PhaseItems(phase == PHASE_BATTLE, items);
            shown |= (unsigned)S.menu << 19 | (unsigned)n << 21 | (unsigned)items[0] << 23;
        }
        if (phase == PHASE_BATTLE) return shown;
    } else if (!hand_idle()) {
        return 0;
    } else if (setting("hint", 1, 0, 1) && skippable() && !S.done) {
        shown |= 1;
    }
    if (setting("hint", 1, 0, 1) && monsters_spent()) shown |= 2;
    return shown | (unsigned)S.queued[D_8009B1D5] << 2 | window_bits();
}
static int full_notice_shown(void) { return full_notice_until && host->now_us(host) < full_notice_until; }
static unsigned hint_signature(void) { return hint_bits() | (full_notice_shown() ? 1u << 26 : 0); }
/* The hand's arrows (hand_arrows) blink: dim every other half second. */
#define WINDOW_BITS (0x3Fu << 10 | 0xFu << 27)
#define ARROW_BLINK_US 500000
static int arrows_dim(void) { return (int)(host->now_us(host) / ARROW_BLINK_US & 1); }
/* What the overlay shows, the arrows' blink with it. */
static unsigned overlay_signature(void)
{
    unsigned shown = hint_signature();
    return shown & WINDOW_BITS && arrows_dim() ? ~shown : shown;
}

/* A drawn card that found no room: a panel high in the middle of the
 * screen for a few seconds, so the draw is seen to have happened. */
static void full_notice(void)
{
    char title[96], text[160];
    int width, height, scale, big, small, frame, pad, w, x, y, panel_w, panel_h, waiting = S.queued[D_8009B1D5];
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    big = 2 * scale;
    small = scale;
    frame = 2 * scale;
    pad = 10 * scale;
    snprintf(title, sizeof title, T("Hand full (%d cards)"), Ehp_HandMax());
    if (full_notice_drawn == 1)
        snprintf(text, sizeof text, T("You drew a card: it waits until a play makes room (%d waiting)"), waiting);
    else
        snprintf(text, sizeof text, T("You drew %u cards: they wait until plays make room (%d waiting)"),
                 full_notice_drawn, waiting);
    w = host->text_width(host, title, big);
    if (host->text_width(host, text, small) > w) w = host->text_width(host, text, small);
    panel_w = w + 2 * pad;
    panel_h = pad + 16 * big + 4 * scale + 12 * small + pad;
    x = (width - panel_w) / 2;
    y = height * 70 / 240;
    host->fill(host, x, y, panel_w, panel_h, MENU_PANEL_RGB, 235);
    host->fill(host, x, y, panel_w, frame, MENU_FRAME_RGB, 255);
    host->fill(host, x, y + panel_h - frame, panel_w, frame, MENU_FRAME_RGB, 255);
    host->fill(host, x, y, frame, panel_h, MENU_FRAME_RGB, 255);
    host->fill(host, x + panel_w - frame, y, frame, panel_h, MENU_FRAME_RGB, 255);
    host->draw_text(host, x + (panel_w - host->text_width(host, title, big)) / 2, y + pad + 8 * big, title,
                    MENU_TITLE_RGB, big);
    host->draw_text(host, x + (panel_w - host->text_width(host, text, small)) / 2,
                    y + pad + 16 * big + 4 * scale + 6 * small, text, MENU_PICKED_RGB, small);
}
/* The phase menu, as in the GX games: a panel in the middle of the screen,
 * the phase now on top, what the next phases do under it with the one
 * under the cursor on a bar, the buttons at the foot. Its text is three times the
 * hints' size, the foot's twice. */
static void phase_menu(int phase)
{
    const char *foot = T("X: confirm   O: close");
    int width, height, scale, big, small, frame, items[2], n, i, w, row, pad, gap, x, y, panel_w, panel_h, top;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    big = 3 * scale;
    small = 2 * scale;
    frame = 3 * scale;
    n = Ehp_PhaseItems(phase == PHASE_BATTLE, items);
    w = host->text_width(host, T(phase_names[phase]), big);
    for (i = 0; i < n; i++)
        if (host->text_width(host, T(phase_action(items[i])), big) > w)
            w = host->text_width(host, T(phase_action(items[i])), big);
    if (host->text_width(host, foot, small) > w) w = host->text_width(host, foot, small);
    row = 16 * big;
    pad = 14 * scale;
    gap = 12 * scale;
    panel_w = w + 4 * pad;
    panel_h = pad + row + gap + n * row + gap + 12 * small + pad;
    x = (width - panel_w) / 2;
    y = (height - panel_h) / 2;
    host->fill(host, x, y, panel_w, panel_h, MENU_PANEL_RGB, 240);
    host->fill(host, x, y, panel_w, frame, MENU_FRAME_RGB, 255);
    host->fill(host, x, y + panel_h - frame, panel_w, frame, MENU_FRAME_RGB, 255);
    host->fill(host, x, y, frame, panel_h, MENU_FRAME_RGB, 255);
    host->fill(host, x + panel_w - frame, y, frame, panel_h, MENU_FRAME_RGB, 255);
    top = y + pad;
    host->draw_text(host, x + (panel_w - host->text_width(host, T(phase_names[phase]), big)) / 2, top + row / 2,
                    T(phase_names[phase]), MENU_TITLE_RGB, big);
    host->fill(host, x + pad, top + row + gap / 2, panel_w - 2 * pad, scale, MENU_FRAME_RGB, 160);
    top += row + gap;
    for (i = 0; i < n; i++, top += row) {
        if (i + 1 == S.menu) host->fill(host, x + pad / 2, top, panel_w - pad, row, MENU_BAR_RGB, 230);
        host->draw_text(host, x + (panel_w - host->text_width(host, T(phase_action(items[i])), big)) / 2,
                        top + row / 2, T(phase_action(items[i])), i + 1 == S.menu ? MENU_PICKED_RGB : MENU_OTHER_RGB,
                        big);
    }
    host->draw_text(host, x + (panel_w - host->text_width(host, foot, small)) / 2, top + gap + 6 * small, foot,
                    MENU_OTHER_RGB, small);
}
/* Where a point of the game's picture (320 x 240) is on the canvas: the
 * host's answer from API 22, else a 4:3 picture as tall as the canvas, in
 * its middle. */
static void picture_point(int x, int y, int *cx, int *cy)
{
    int width, height, scale;
    if (host->api >= 22) {
        host->picture_point(host, x, y, cx, cy);
        return;
    }
    host->overlay_size(host, &width, &height, &scale);
    *cx = (width - height * 4 / 3) / 2 + x * height / 240;
    *cy = y * height / 240;
}
/* The cards off the window at either end of a long hand: a red arrow (the
 * cursor's red) pointing that way, with how many there are under it, in
 * the gutters beside the ten cards on screen (Ehp_LayoutX), above the
 * cursor's height. The arrows blink. */
#define ARROW_RGB 0xE81810
#define ARROW_LEFT_X 8
#define ARROW_RIGHT_X 312
#define ARROW_TOP 154
#define ARROW_BOTTOM 166
#define ARROW_HALF_WIDTH 3
#define ARROW_NUMBER_Y 176
static void hidden_cards(int at, int left, int cards)
{
    char number[8];
    int width, height, scale, x0, y0, x1, y1, cx, cy, tw, box_w, row, half, alpha = arrows_dim() ? 110 : 255;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    snprintf(number, sizeof number, "%d", cards);
    tw = host->text_width(host, number, scale);
    picture_point(at - ARROW_HALF_WIDTH, ARROW_TOP, &x0, &y0);
    picture_point(at + ARROW_HALF_WIDTH, ARROW_BOTTOM, &x1, &y1);
    picture_point(at, ARROW_NUMBER_Y, &cx, &cy);
    box_w = (tw > x1 - x0 ? tw : x1 - x0) + 6 * scale;
    host->fill(host, cx - box_w / 2, y0 - 3 * scale, box_w, cy + 10 * scale - (y0 - 3 * scale), 0x000000, 160);
    half = (y1 - y0) / 2;
    for (row = y0; row < y1 && half > 0; row++) {
        int d = row - (y0 + half), w = (x1 - x0) * (half - (d < 0 ? -d : d)) / half;
        if (w > 0) host->fill(host, left ? x1 - w : x0, row, w, 1, ARROW_RGB, alpha);
    }
    host->draw_text(host, cx - tw / 2, cy, number, HINT_RGB, scale);
}
/* The arrows for window_bits' share of the hints; none for none. */
static void hand_arrows(unsigned shown)
{
    unsigned e = (shown >> 10 & 0x3F) | (shown >> 27 & 0xF) << 6;
    int count, view;
    if (!(shown & WINDOW_BITS)) return;
    count = (int)((e - 1) / 31) + EHP_VIEW + 1;
    view = (int)((e - 1) % 31);
    if (view > 0) hidden_cards(ARROW_LEFT_X, 1, view);
    if (count - view - EHP_VIEW > 0) hidden_cards(ARROW_RIGHT_X, 0, count - view - EHP_VIEW);
}
static void draw_hint(void)
{
    unsigned shown = hint_signature(), waiting = shown >> 2 & 0xFF;
    int phase = shown >> 16 & 7, row = 0;
    char text[128];
    if (shown & 1u << 26) full_notice();
    shown &= ~(1u << 26);
    if (shown & 0x80000000u) {
        unsigned left = shown & 0xFF;
        snprintf(text, sizeof text, left == 1 ? T("Hand limit: discard %u card") : T("Hand limit: discard %u cards"),
                 left);
        line(text, row++, HINT_RGB);
        line(T("UP: mark   X: discard the marked cards"), row, HINT_RGB);
        hand_arrows(shown);
        return;
    }
    if (phase) {
        snprintf(text, sizeof text, T("%s   START: phases"), T(phase_names[phase]));
        line(text, row++, HINT_RGB);
        if (S.menu) phase_menu(phase);
    }
    if (shown & 1)
        line(extra_hand() ? T("Extra hand: START to skip") : T("START: play no card this turn"), row++, HINT_RGB);
    if (shown & 2) line(T("Monster played: magic, trap and equip cards only"), row++, HINT_RGB);
    if (waiting) {
        snprintf(text, sizeof text,
                 waiting == 1 ? T("Hand full: %u card waiting to be drawn") : T("Hand full: %u cards waiting to be drawn"),
                 waiting);
        line(text, row++, HINT_RGB);
    }
    hand_arrows(shown);
}

int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    if (from->api < 4) return 0;
    host = ehp_host = from;
    mod->api = 4;
    mod->overlay = draw_hint;
    mod->overlay_signature = overlay_signature;
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
