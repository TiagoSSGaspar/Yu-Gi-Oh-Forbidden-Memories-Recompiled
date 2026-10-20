/* The hand past five cards, when another mod raises the hand limit
 * (set_hand_limits in extra_hand_plays.h).
 *
 * The disc keeps five hand slots per side: DuelSideState.hand[5] (deck
 * indices), D_800EA030[5] (the slots on screen) and five card records per
 * side (D_800907CC). Cards six to ten live beside them, here:
 * - their deck indices in S.xhand, per side, packed;
 * - their slots on screen in S.xslot, for the side whose turn it is;
 * - their card records borrowed from the other side's five hand records.
 *   During a side's turn nothing reads the other side's hand records -- its
 *   draw rebuilds them from its hand when its turn comes -- so they are
 *   cleared when the turn ends and when it begins.
 * The five slots are always filled first: a draw packs the cards past the
 * fifth into the five's empty slots before it spreads the hand out.
 *
 * On screen the hand is laid out overlapped across the width of five
 * (Ehp_LayoutX), later cards over earlier ones and the card under the
 * cursor over all.
 *
 * The player walks and picks from every card with a copy of the hand
 * state's player steps (Ehp_HandActions, from duel_scene_hand_actions.c),
 * the five-slot helpers it calls taught the rest. The CPU's hand script
 * gets the cards past the fifth in its snapshot ahead of the upcoming deck,
 * as the disc numbers deck cards it can swap into the hand: one it picks is
 * swapped with a card of the five by the disc's own swap (func_8001BAF0), so
 * the CPU always plays from the five. */
#include "hand_internal.h"
#define D_8009B_DISPLAY_OBJECTS_VISIBLE
#define AI_GET_HAND_SIZE_RETURNS_S32 /* Native override serves word-sized callers. */
#include "game/ai.h"
#include "game/card_constants.h"
#include "game/display_object.h"
#include "game/display_object_core.h"
#include "game/display_object_config.h"
#include "game/display_object_helpers.h"
#include "game/display_object_interpolation.h"
#include "game/display_object_motion.h"
#include "game/display_object_work_slots.h"
#include "game/display_parent_links.h"
#include "game/duel_action_lock.h"
#include "game/duel_card.h"
#include "game/duel_card_display_state.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_pick_cursor.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_card_staging.h"
#include "game/duel_card_state_helpers.h"
#include "game/duel_card_viewer.h"
#include "game/duel_check_quit_input.h"
#include "game/duel_cursor_status.h"
#include "game/duel_deck_card.h"
#include "game/duel_display.h"
#include "game/duel_effect.h"
#include "game/duel_field_display_objects.h"
#include "game/duel_get_card_viewer_request_id.h"
#include "game/duel_grid.h"
#include "game/duel_scene_hand_actions.h"
#include "game/duel_scene_state.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/func_80018004.h"
#include "game/func_8001B780.h"
#include "game/func_8001B7AC.h"
#include "game/func_8001B8B8.h"
#include "game/func_8001B938.h"
#include "game/input.h"
#include "game/script_state.h"
#include "game/sound.h"
#include "pc/cards/cards.h"
#include "unmatched.h"
#include <string.h>

s32 func_800235C0(void);
void func_80027DF8(AiActiveCard *out, s32 who);
s32 Duel_HasAllExodiaPieces(void);

#define CARD_RECORD(i) (((DuelCardStagingDeckView *)D_8015C424)->field_cards[i])
#define CARD_ID(i) (CARD_RECORD(i).card_id)
#define CARD_KIND(id) ((gDuel_adwCardStats[(id) - 1] >> CARD_STAT_TYPE_SHIFT) & CARD_STAT_TYPE_MASK)
#define SIDE_CURSOR(base) ((DuelCardPickCursor *)((base) + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE))
#define FIELD_CURSOR (((DuelSelectionSideCursors *)D_800E9F10)[D_8009B1D5].field)
#define HAND_OBJECT(object) ((HandCardObject *)(object))
#define SPRITE_FLAGS(sprite) (*(u16 *)((u8 *)(sprite) + 8))
#define CARD_PLACE_X(card) (*(s16 *)((u8 *)(card) + 8))
#define CARD_PLACE_Y(card) (*(s16 *)((u8 *)(card) + 0xA))

#define SLOT_X 0xE            /* the first card's x on the panel, as the disc lays out five */
#define SLOT_STEP 0x3C
#define SPREAD (4 * SLOT_STEP) /* the first card to the last: the width of five */
#define CURSOR_Y 194          /* the hand cursor's y (func_8001B780) */
#define PANEL_START 0x8000    /* D_8009B162: the move began */
#define PANEL_IN 2
#define PANEL_OUT 8
#define DEPTH_STEP 1          /* later cards over earlier ones */
#define FOCUS_DEPTH (EHP_SLOTS + 1) /* the card under the cursor over them all */
#define CURSOR_DEPTH 0xA      /* the hand cursor's own (the hand state's first frame) */

static ExtraHandLimits limits;
static void *original_panel, *original_cursor, *original_mark, *original_zone, *original_snapshot,
    *original_exodia, *original_setup;

static void get_limits(int side, int *max, int *keep)
{
    *max = HAND_SIZE;
    *keep = 0;
    if (limits) limits(side, max, keep);
    if (*max < HAND_SIZE) *max = HAND_SIZE;
    if (*max > EHP_SLOTS) *max = EHP_SLOTS;
    if (*keep < 0) *keep = 0;
    if (*keep > *max) *keep = *max;
}
void Ehp_SetLimits(ExtraHandLimits fn) { limits = fn; }
int Ehp_HandMax(void)
{
    int max, keep;
    get_limits(D_8009B1D5, &max, &keep);
    return max;
}
int Ehp_HandKeep(void)
{
    int max, keep;
    get_limits(D_8009B1D5, &max, &keep);
    return keep;
}

s8 Ehp_Get(int side, int slot)
{
    return slot < HAND_SIZE ? D_800E9FF0[side].hand[slot] : S.xhand[side][slot - HAND_SIZE];
}
void Ehp_Set(int side, int slot, s8 deck_index)
{
    if (slot < HAND_SIZE) D_800E9FF0[side].hand[slot] = deck_index;
    else S.xhand[side][slot - HAND_SIZE] = deck_index;
}
int Ehp_Count(int side)
{
    int slot, n = 0;
    for (slot = 0; slot < EHP_SLOTS; slot++) n += Ehp_Get(side, slot) >= 0;
    return n;
}
int Ehp_Extras(int side)
{
    int k, n = 0;
    for (k = 0; k < EHP_EXTRA; k++) n += S.xhand[side][k] >= 0;
    return n;
}
DuelHandSlot *Ehp_Slot(int slot) { return slot < HAND_SIZE ? &D_800EA030[slot] : &S.xslot[slot - HAND_SIZE]; }
int Ehp_Record(int side, int slot)
{
    int order;
    if (slot >= HAND_SIZE) return (side ^ 1) * DUEL_CARD_SIDE_RECORD_COUNT + slot - HAND_SIZE;
    order = D_800907CC[slot + side * HAND_SIZE];
    return order & 0x80 ? (order & 0x7F) + DUEL_CARD_SIDE_RECORD_COUNT : order;
}
int Ehp_CardId(int deck_index) { return deck_index >= 0 ? gDuel_aDeckCardRecords[deck_index].id : 0; }

void Ehp_SetupRecord(int record, int deck_index) { Duel_SetupCardRecord(record, deck_index); }

/* Duel_SetupCardRecord takes a record of the second side's for a card of
 * that side's deck, and adds 40 to a position below 40. The first side's
 * cards past the fifth borrow second-side records, and the game sets them
 * up again from the card's own deck index -- a fusion's result, for one --
 * so during the first side's turn a position below 40 in one of them is
 * the first side's card, given 40 short for the addition to cancel. Nothing
 * else uses those records then. */
static u8 *setup(s32 index, s32 position)
{
    int record = index & 0x80 ? (index & 0x7F) + DUEL_CARD_SIDE_RECORD_COUNT : index;
    if (D_8009B1D5 == 0 && record >= DUEL_CARD_SIDE_RECORD_COUNT &&
        record < DUEL_CARD_SIDE_RECORD_COUNT + EHP_EXTRA && position >= 0 && position < DECK_SIZE)
        position -= DECK_SIZE;
    return ((u8 *(*)(s32, s32))original_setup)(index, position);
}

int Ehp_LayoutX(int slot, int count)
{
    if (count <= HAND_SIZE) return slot * SLOT_STEP + SLOT_X;
    return SLOT_X + slot * SPREAD / (count - 1);
}
int Ehp_BigHand(void) { return S.layout > HAND_SIZE; }

void Ehp_Pack(int side)
{
    s8 cards[EHP_SLOTS];
    int slot, n = 0;
    for (slot = 0; slot < EHP_SLOTS; slot++)
        if (Ehp_Get(side, slot) >= 0) cards[n++] = Ehp_Get(side, slot);
    for (slot = 0; slot < EHP_SLOTS; slot++) Ehp_Set(side, slot, slot < n ? cards[slot] : -1);
}

void Ehp_ClearBorrowed(int side)
{
    int k;
    for (k = 0; k < EHP_EXTRA; k++) {
        DuelCardRecord *record = &D_801A7AD8[Ehp_Record(side, HAND_SIZE + k)];
        record->flags = 0;
        record->object = 0;
    }
}

/* After the draw has spawned the five below the screen: the rest, where
 * the panel picks them up. */
void Ehp_SpawnExtras(int y)
{
    int side = D_8009B1D5, k, record;
    memset(S.xslot, 0, sizeof S.xslot);
    for (k = 0; k < EHP_EXTRA; k++) {
        if (S.xhand[side][k] < 0) continue;
        record = Ehp_Record(side, HAND_SIZE + k);
        Ehp_SetupRecord(record, S.xhand[side][k]);
        S.xslot[k].object = (u8 *)func_80018004(&D_801A7AD8[record], Ehp_LayoutX(HAND_SIZE + k, HAND_SIZE),
                                                y);
    }
}

static int is_hand_record(void *record)
{
    return (u8 *)record == D_800E9F10 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE;
}

static void set_depth(int slot, int depth)
{
    DuelHandSlot *at = Ehp_Slot(slot);
    if (!at->object) return;
    DisplayObject_SetDepthOffset((DisplayObject *)at->object, (s8)depth);
    if (at->child) DisplayObject_SetDepthOffset((DisplayObject *)at->child, (s8)(depth + 1));
}
/* The depth the hand's cards are made at, read when the panel lays them out. */
static int depth0;
static int base_depth(void) { return depth0; }
/* The card under the cursor over the rest. */
static void focus(int slot)
{
    if (!Ehp_BigHand()) return;
    if (S.focus) set_depth(S.focus - 1, base_depth() + (S.focus - 1) * DEPTH_STEP);
    set_depth(slot, base_depth() + FOCUS_DEPTH);
    S.focus = (u8)(slot + 1);
}

void Ehp_Arrange(int slot)
{
    if (!Ehp_BigHand() || !Ehp_Slot(slot)->object) return;
    if (slot == 0) depth0 = ((DisplayObject *)Ehp_Slot(0)->object)->field_16;
    set_depth(slot, base_depth() + slot * DEPTH_STEP);
}

/* The CPU's swap (func_8001BAF0, or AI Hard Mode's) exchanges deck records:
 * a card past the fifth it took into the five leaves the five's card in
 * its place. Its record and object follow. */
void Ehp_RefreshExtras(void)
{
    int side = D_8009B1D5, k, n, record;
    DuelCardDisplayObject *old;
    for (k = 0; k < EHP_EXTRA; k++) {
        if ((n = S.xhand[side][k]) < 0 || !(old = (DuelCardDisplayObject *)S.xslot[k].object)) continue;
        record = Ehp_Record(side, HAND_SIZE + k);
        if (D_801A7AD8[record].card_id == Ehp_CardId(n)) continue;
        Ehp_SetupRecord(record, n);
        S.xslot[k].object = (u8 *)func_80018004(&D_801A7AD8[record], old->out_x, old->out_y);
        DisplayObject_ReleaseIfPresent(old);
        Ehp_Arrange(HAND_SIZE + k);
    }
}

/* The panel has just started sliding in: its cards spread over the width of
 * five, linked to it at their new places. */
void Ehp_LayOut(DuelSelectionRecord *side)
{
    DisplayParent *parent = (DisplayParent *)side;
    DisplayObject *panel = parent->position_base, *object;
    int slot, count = S.layout;
    if (count <= HAND_SIZE || !panel) return;
    if (D_800EA030[0].object) depth0 = ((DisplayObject *)D_800EA030[0].object)->field_16;
    for (slot = 0; slot < count; slot++) {
        if (!(object = (DisplayObject *)Ehp_Slot(slot)->object)) continue;
        object->field_30.h.field_30 = panel->field_30.h.field_30 + Ehp_LayoutX(slot, count);
        object->field_30.h.field_32 = panel->field_30.h.field_32 - 0x1C;
        DuelSelection_LinkDisplayObject(parent, object);
        set_depth(slot, base_depth() + slot * DEPTH_STEP);
    }
    S.focus = 0;
}

/* The panel leaving: the slots past the fifth go with it, as the five do --
 * except a card being played, which placement has taken (it clears the
 * five's own), and whose pick number goes now. In the end-of-turn discard
 * the marked cards are thrown away: they go with the panel too. */
static void release_extras(DisplayParent *parent)
{
    int k;
    for (k = 0; k < EHP_EXTRA; k++) {
        DuelHandSlot *slot = &S.xslot[k];
        if (slot->active_09 && !S.discard) {
            DisplayObject_ReleaseIfPresent(slot->child);
            slot->child = 0;
            slot->object = 0;
        }
        DuelSelection_LinkDisplayObject(parent, (DisplayObject *)slot->object);
        DuelSelection_LinkDisplayObject(parent, (DisplayObject *)slot->child);
        slot->object = slot->child = 0;
        slot->active_09 = 0;
    }
    S.focus = 0;
}

/* func_800235C0, the panel and camera moves of D_8009B162. */
static s32 panel(void)
{
    u16 flags = D_8009B162;
    DisplayParent *source = (DisplayParent *)D_8009B1B4;
    int hand = flags && !(flags & PANEL_START) && source && is_hand_record(source);
    s32 result = ((s32 (*)(void))original_panel)();
    if (hand && (flags & PANEL_IN)) Ehp_LayOut((DuelSelectionRecord *)source);
    if (hand && (flags & PANEL_OUT)) release_extras(source);
    return result;
}

/* func_8001B780, the hand cursor onto its slot. */
static void cursor(DuelHandStackState *state)
{
    if (!Ehp_BigHand() || !is_hand_record(state)) {
        ((void (*)(DuelHandStackState *))original_cursor)(state);
        return;
    }
    state->position_object->field_30.h.field_30 = (s16)Ehp_LayoutX(state->slot_index, S.layout);
    state->position_object->field_30.h.field_32 = CURSOR_Y;
    /* Over the cards, which a spread hand stacks up. */
    if (base_depth() + FOCUS_DEPTH + 2 > CURSOR_DEPTH)
        DisplayObject_SetDepthOffset(state->position_object, (s8)(base_depth() + FOCUS_DEPTH + 2));
    focus(state->slot_index);
}

/* func_8001B7AC, the next pick number onto the cursor's card. */
static void mark(DuelHandStackState *state)
{
    DuelHandSlot *slot;
    DuelCardDisplayObject *card;
    DisplayObject *child;
    if (state->slot_index < HAND_SIZE) {
        ((void (*)(DuelHandStackState *))original_mark)(state);
        return;
    }
    slot = Ehp_Slot(state->slot_index);
    card = (DuelCardDisplayObject *)slot->object;
    card->out_y -= 4;
    child = (DisplayObject *)DisplayObject_AcquireSlot(DisplayObject_FindFreeGeneralSlot(), 1);
    DisplayObject_ConfigureScreenSprite(child, card->out_x, card->out_y, 0x10, 0x10, state->count << 4, 0xB8,
                                        0xB, 0x250, 0xFC);
    DisplayObject_SelectOrderingTable1(child);
    DisplayObject_SetDepthOffset(child, (s8)(((DisplayObject *)slot->object)->field_16 + 1));
    slot->child = (u8 *)child;
    state->count++;
    slot->active_09 = state->count;
    SD_SEPlayFull(0x2F);
}

/* func_8001B938, the zone cursor set up for the card under the hand cursor. */
static void zone(DuelSelectionRecord *selection)
{
    DisplayObject *r;
    DuelCardRecord *e;
    int i, kind;
    if (selection->field_15 != 0 || (s8)selection->field_0E < HAND_SIZE) {
        ((void (*)(DuelSelectionRecord *))original_zone)(selection);
        return;
    }
    D_8009B1B4 = (DuelCardPickCursor *)(D_800E9F48 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE);
    D_8009B1B4->status = 0;
    D_8009B1B4->field_13 = 1;
    D_8009B1B4->field_0C = 0x74;
    D_8009B1B4->field_18 = 0;
    D_8009B1B4->field_11 = 2;
    D_8009B1B4->field_12 = 3;
    r = (DisplayObject *)Ehp_Slot((s8)selection->field_0E)->object;
    kind = CARD_KIND(((DuelDeckCardRecord *)CARD_RECORD(r->field_6A).data)->id);
    if (kind >= CARD_TYPE_MAGIC && (r->field_20.b.field_21 != 0 || kind == CARD_TYPE_TRAP)) {
        D_8009B1B4->field_11 = 3;
        D_8009B1B4->field_12 = 4;
    }
    D_8009B1B4->row = D_8009B1B4->field_11;
    e = &D_801A7AD8[D_800907D8[D_8009B1B4->row * DUEL_FIELD_ROW_SIZE + D_8009B1D5 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT]];
    for (i = 0; i < DUEL_FIELD_ROW_SIZE; i++, e++) {
        if (!(e->flags & DUEL_CARD_FLAG_OCCUPIED)) {
            D_8009B1B4->col = i;
            break;
        }
    }
    D_8009B162 = 3;
}

/* func_80027DF8, a side's snapshot for the CPU's scripts: its field, its
 * hand packed, then its upcoming deck. The cards past the fifth go between
 * hand and deck, numbered like deck cards, so that the hand script can pick
 * them and the swap (func_8001BAF0) brings them into the five. */
static void snapshot(AiActiveCard *out, s32 who)
{
    AiActiveCard *at;
    int held = 0, deck = 0, extras = Ehp_Extras(who), k, n, slot;
    ((void (*)(AiActiveCard *, s32))original_snapshot)(out, who);
    if (!extras) return;
    for (slot = 0; slot < HAND_SIZE; slot++) held += D_800E9FF0[who].hand[slot] >= 0;
    at = out + 2 * DUEL_FIELD_ROW_SIZE + held;
    while (at[deck].card_id) deck++;
    memmove(at + extras, at, (deck + 1) * sizeof *at);
    for (k = 0; k < EHP_EXTRA; k++) {
        s32 stats;
        if ((n = S.xhand[who][k]) < 0) continue;
        at->card_id = (s16)Ehp_CardId(n);
        stats = gDuel_adwCardStats[at->card_id - 1];
        at->attack = (s16)((stats & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE);
        at->defense = (s16)(((stats >> CARD_STAT_DEFENSE_SHIFT) & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE);
        at->flags = 0;
        at->card_type = (s8)((stats >> CARD_STAT_TYPE_SHIFT) & CARD_STAT_TYPE_MASK);
        at->guardian_star = (s8)((stats >> CARD_STAT_GUARDIAN_STAR_1_SHIFT) & CARD_STAT_GUARDIAN_STAR_MASK);
        at->guardian_star_2 = (s8)((stats >> CARD_STAT_GUARDIAN_STAR_2_SHIFT) & CARD_STAT_GUARDIAN_STAR_MASK);
        at->deck_index = (u8)n;
        at++;
    }
}

/* Duel_HasAllExodiaPieces over every slot. */
static s32 exodia(void)
{
    int side = D_8009B1D5, used[EHP_SLOTS] = {0}, id, slot;
    if (!Ehp_Extras(side)) return ((s32 (*)(void))original_exodia)();
    for (id = EXODIA_FIRST_CARD_ID; id < EXODIA_CARD_ID_END; id++) {
        if (!Cards_ExodiaPiece(id)) return 0;
        for (slot = 0; slot < EHP_SLOTS; slot++)
            if (!used[slot] && Ehp_CardId(Ehp_Get(side, slot)) == id && Ehp_Get(side, slot) >= 0) break;
        if (slot == EHP_SLOTS) return 0;
        used[slot] = 1;
    }
    return 1;
}

/* The hand state's zone confirm (its step 4): the marked cards, in the order
 * they were marked, or the card under the cursor, after any field card they
 * go onto, into D_800E9EF0 for placement. */
static void confirm(DuelSelectionRecord *side, DuelHandSlot *hand)
{
    DuelCardRecord *card;
    HandCardObject *obj;
    int n, value, i = 0;
    SD_SEPlayFull(7);
    for (n = 6; n >= 0; n--) D_800E9EF0[n] = 0;
    n = D_8009B1B4->row * DUEL_FIELD_ROW_SIZE + D_8009B1B4->col;
    value = D_800907D8[n + D_8009B1D5 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT];
    card = &D_801A7AD8[value];
    D_8009B19C = (u8)value;
    if (card->flags & DUEL_CARD_FLAG_OCCUPIED) {
        obj = HAND_OBJECT(card->object);
        i = 1;
        D_800E9EF0[0] = (DisplayObject *)func_80017F04(card, CARD_PLACE_X(card), CARD_PLACE_Y(card));
        DuelCard_DeactivateRecord(&D_801A7AD8[obj->card_index]);
    } else if (side->field_15 == 0 && HAND_OBJECT(hand->object)->kind == CARD_TYPE_EQUIP &&
               D_8009B1B4->row == 2) {
        return;
    }
    if (side->field_15 == 0) {
        hand->active_09 = 1;
        if (HAND_OBJECT(hand->object)->face != 0) D_8009B1C8->rank.face_down_plays++;
    }
    for (value = 1; value <= HAND_SIZE; value++) {
        for (n = 0; n < S.layout && Ehp_Slot(n)->active_09 != value; n++) { }
        if (n == S.layout) break;
        Ehp_Set(D_8009B1D5, n, -1);
        D_800E9EF0[i++] = (DisplayObject *)Ehp_Slot(n)->object;
    }
    D_8009B174 = 6;
}

/* The hand state's player steps (duel_scene_hand_actions.c, cases 1, 3, 4,
 * 5 and 6) for a hand of more than five: the same steps over S.layout slots,
 * with at most five cards marked, as a fusion chain takes. Its first frame
 * and the L2/R2 look at the field are the disc's own: they do not touch the
 * slots. */
void Ehp_HandActions(DuelSelectionRecord *side)
{
    int count = S.layout, at = (s8)side->field_0E, n, v, value, card_id;
    DuelHandSlot *hand = Ehp_Slot(at), *slot;
    HandCardObject *obj;
    u8 *sprite;
    switch (D_8009B174 & 0xF) {
    case 1:
        if (!(D_8009B174 & 0x80)) {
            D_8009B174 |= 0x80;
            for (n = 0; n < count; n++)
                if (Ehp_Slot(n)->object) HAND_OBJECT(Ehp_Slot(n)->object)->color = DUEL_DISPLAY_COLOR_NORMAL;
        }
        if (Duel_CheckQuitInput() != 0) break;
        if (gInput_wPad1Pressed & PAD_BUTTON_TRIANGLE) {
            card_id = CARD_ID(Ehp_Record(D_8009B1D5, at));
            gDuel_bCardViewerYOffset = 0x14;
            gDuel_bEffectState = 2;
            gDuel_wViewerCardID = (u16)card_id;
            return;
        }
        if (gInput_wPad1Repeat & 0xA000) {
            value = at;
            if (gInput_wPad1Repeat & 0x2000) {
                if (++value >= count) return;
            } else if (--value < 0) {
                return;
            }
            side->field_0E = (u8)value;
            func_8001B780((DuelHandStackState *)side);
            func_80023144((DuelFieldDisplaySource *)side, Ehp_Record(D_8009B1D5, value));
            SD_SEPlayFull(6);
            return;
        }
        if ((gInput_wPad1Pressed & 0xFFFF) == 0x1000 && gInput_wPad1Held == 0x1000) {
            if (hand->active_09 == 0 && side->field_15 < HAND_SIZE) {
                func_8001B7AC((DuelHandStackState *)side);
                return;
            }
        } else if ((gInput_wPad1Pressed & PAD_BUTTON_CIRCLE) ||
                   ((gInput_wPad1Pressed & 0xFFFF) == 0x4000 && gInput_wPad1Held == 0x4000)) {
            value = hand->active_09;
            if (value != 0) {
                SD_SEPlayFull(0x2F);
                hand->active_09 = 0;
                HAND_OBJECT(hand->object)->pos.xy.y += 4;
                DisplayObject_ReleaseIfPresent(hand->child);
                hand->child = 0;
                side->field_15--;
                for (n = 0; n < count; n++) {
                    slot = Ehp_Slot(n);
                    v = slot->active_09;
                    if (v >= value) {
                        slot->active_09 = (u8)(v - 1);
                        HAND_OBJECT(slot->child)->icon_state = (u8)((v - 2) * 0x10);
                    }
                }
                return;
            }
        } else {
            if (gInput_wPad1Held & 3) {
                D_8009B1D4 = 0;
                D_8009B1B4 = SIDE_CURSOR(D_800E9F2C);
                gDuel_wSceneStateFlags |= 0x4000;
                return;
            }
            if ((gInput_wPad1Pressed & PAD_BUTTON_CONFIRM_MASK) && side->field_15 != 1) {
                SD_SEPlayFull(7);
                D_8009B174 = 4;
                func_8001B8B8(side);
                if (side->field_15 == 0) {
                    D_8009B174 = 3;
                    return;
                }
            }
        }
        break;
    case 3:
        obj = HAND_OBJECT(hand->object);
        if (!(D_8009B174 & 0x80)) {
            D_8009B174 |= 0xC0;
            obj->target.xy.x = 0x86;
            obj->target.xy.y = 0x5A;
            obj->saved.xy.x = obj->pos.xy.x;
            obj->saved.xy.y = obj->pos.xy.y;
            DisplayObject_SavePosition((void *)obj);
            obj->step = 0;
            DisplayObject_SetDepthOffset((DisplayObject *)obj, (s8)(obj->depth + 4));
            side->cursor_object->flags &= 0xFFBF;
        }
        if (D_8009B174 & 0x40) {
            DisplayObject_InterpolatePositionCosine((void *)obj, obj->target.xy.x, obj->target.xy.y, obj->step);
            obj->step = obj->step + 0xAA;
            if (obj->step >= 0x800) {
                D_8009B174 &= 0xBF;
                obj->pos.word = obj->target.word;
                if (D_8009B174 & 0x10) {
                    side->cursor_object->flags |= 0x40;
                    DisplayObject_SetDepthOffset((DisplayObject *)obj, (s8)(obj->depth - 4));
                    if (D_8009B174 & 0x20) {
                        D_8009B174 = 4;
                        return;
                    }
                    D_8009B174 = 1;
                    return;
                }
                card_id = ((DuelDeckCardRecord *)CARD_RECORD(obj->card_index).data)->id;
                value = CARD_KIND(card_id);
                if ((value < CARD_TYPE_MAGIC || value == CARD_TYPE_TRAP) && obj->face == 0) {
                    obj->step = 0x10;
                    D_8009B174 |= 0x20;
                    SD_SEPlayFull(0xB);
                }
                sprite = DisplayObject_AcquireSlot(DisplayObject_FindFreeGeneralSlot(), 2);
                DisplayObject_ConfigureSpriteAtPosition(sprite, (s16)obj->pos.xy.x - 8, (s16)obj->pos.xy.y + 0x1E,
                                                        3, 1, 2, 0xB, 0x20C);
                DisplayObject_SelectOrderingTable1((DisplayObject *)sprite);
                DisplayObject_SetDepthOffset((DisplayObject *)sprite, 0xA);
                SPRITE_FLAGS(sprite) |= 0x28;
                D_8009B188 = (DisplayObject *)sprite;
                sprite = DisplayObject_AcquireSlot(DisplayObject_FindFreeGeneralSlot(), 2);
                DisplayObject_ConfigureSpriteAtPosition(sprite, (s16)obj->pos.xy.x + 0x3C,
                                                        (s16)obj->pos.xy.y + 0x1E, 3, 1, 0, 0xB, 0x20C);
                DisplayObject_SelectOrderingTable1((DisplayObject *)sprite);
                DisplayObject_SetDepthOffset((DisplayObject *)sprite, 0xA);
                SPRITE_FLAGS(sprite) |= 0x28;
                D_8009B18C = (DisplayObject *)sprite;
                return;
            }
        } else if (D_8009B174 & 0x20) {
            obj->flags |= 4;
            obj->face += obj->step;
            if (!(obj->face & 0x7F)) {
                if (obj->face == 0) obj->flags &= 0xFFFB;
                D_8009B174 &= 0xDF;
                if (D_8009B174 & 0x10) goto restore;
            }
        } else {
            if (gInput_wPad1Repeat & 0xA000) {
                obj->step = 0x10;
                D_8009B174 |= 0x20;
                if (gInput_wPad1Repeat & 0x2000) obj->step = -0x10;
                SD_SEPlayFull(0xB);
                return;
            }
            if (gInput_wPad1Pressed & PAD_BUTTON_CIRCLE) {
                SD_SEPlayFull(8);
                if (obj->face != 0) {
                    obj->step = 0x10;
                    D_8009B174 |= 0x30;
                    return;
                }
                goto restore;
            }
            if (gInput_wPad1Pressed & PAD_BUTTON_CONFIRM_MASK) {
                SD_SEPlayFull(7);
                D_801A7AD8[obj->card_index].flags &= 0xEFFF;
                if (obj->face != 0) D_801A7AD8[obj->card_index].flags |= 0x1000;
                card_id = CARD_ID(obj->card_index);
                value = CARD_KIND(card_id);
                if (value >= CARD_TYPE_MAGIC && obj->face == 0 && value != CARD_TYPE_EQUIP &&
                    value != CARD_TYPE_TRAP) {
                    DisplayObject_ReleaseIfPresent(D_8009B188);
                    DisplayObject_ReleaseIfPresent(D_8009B18C);
                    D_800E9EF0[0] = (DisplayObject *)obj;
                    D_8009B18C = 0;
                    D_8009B188 = 0;
                    Ehp_Set(D_8009B1D5, at, -1);
                    D_8009B174 = 5;
                    return;
                }
                D_8009B174 |= 0x70;
                goto restore;
            }
        }
        break;
    restore:
        DisplayObject_ReleaseIfPresent(D_8009B188);
        DisplayObject_ReleaseIfPresent(D_8009B18C);
        D_8009B18C = 0;
        D_8009B188 = 0;
        *(s32 *)&obj->target = *(s32 *)&obj->saved;
        DisplayObject_SavePosition((void *)obj);
        obj->step = 0;
        D_8009B174 |= 0x50;
        return;
    case 4:
        if (!(D_8009B174 & 0x80)) {
            D_8009B174 |= 0x80;
            D_8009B210 = 0;
            func_8001B938(side);
            D_8009B174 |= 0x40;
        }
        if (D_8009B174 & 0x40) {
            if (D_8009B162 == 0) {
                D_8009B174 &= 0xBF;
                if (D_8009B174 & 0x10) {
                    D_8009B174 = 1;
                    D_8009B1B4 = SIDE_CURSOR(D_800E9F10);
                    if (side->field_15 == 0) {
                        D_8009B174 = 3;
                        return;
                    }
                }
            }
        } else if (func_80024060((void *)D_8009B1B4) == 0) {
            value = Duel_GetCardViewerRequestId(
                &D_801A7AD8[D_800907D8[FIELD_CURSOR.row * DUEL_FIELD_ROW_SIZE + FIELD_CURSOR.col +
                                       D_8009B1D5 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT]]);
            if (value != 0) {
                gDuel_bCardViewerYOffset = 0x14;
                gDuel_wViewerCardID = (u16)value;
                gDuel_bEffectState = 2;
                return;
            }
            if (gInput_wPad1Pressed & PAD_BUTTON_CIRCLE) {
                D_8009B162 = 0xC;
                D_8009B174 |= 0x50;
                SD_SEPlayFull(8);
                return;
            }
            if (gInput_wPad1Pressed & PAD_BUTTON_CONFIRM_MASK) {
                confirm(side, hand);
                return;
            }
        }
        break;
    case 6:
        if (!(D_8009B174 & 0x80)) {
            D_8009B162 = 0xC;
            D_8009B174 |= 0x80;
            D_8009B1B4 = SIDE_CURSOR(D_800E9F48);
        }
        if (D_8009B162 == 0) gDuel_wSceneStateFlags = 7;
        break;
    case 5:
        if (!(D_8009B174 & 0x80)) {
            D_8009B174 |= 0x80;
            hand->object = 0;
            D_8009B162 = 8;
            D_8009B1B4 = SIDE_CURSOR(D_800E9F10);
        }
        if (D_8009B162 == 0) gDuel_wSceneStateFlags = 6;
        break;
    }
}

int Ehp_Install(void)
{
    const MemoriesModHost *host = ehp_host;
    return host->hook(host, (void *)func_800235C0, (void *)panel, &original_panel) &&
        host->hook(host, (void *)func_8001B780, (void *)cursor, &original_cursor) &&
        host->hook(host, (void *)func_8001B7AC, (void *)mark, &original_mark) &&
        host->hook(host, (void *)func_8001B938, (void *)zone, &original_zone) &&
        host->hook(host, (void *)func_80027DF8, (void *)snapshot, &original_snapshot) &&
        host->hook(host, (void *)Duel_HasAllExodiaPieces, (void *)exodia, &original_exodia) &&
        host->hook(host, (void *)Duel_SetupCardRecord, (void *)setup, &original_setup);
}
