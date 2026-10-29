/* The end-of-turn hand limit (set_hand_limits' keep): before the turn
 * switch (scene state 10) a side holding more cards discards down to it.
 *
 * The player's hand comes up on its panel as a draw puts it up, with the
 * hand cursor. UP marks a card to throw away (its pick number shows the
 * order), DOWN or circle takes the mark back, and X or square throws the
 * marked cards away once there are as many as the limit asks for; the
 * panel then slides away with them. The CPU throws away its weakest cards
 * at once: monsters by their better stat, any other card counted as 1500.
 * The game has no graveyard: a discarded card leaves the duel. */
#include "hand_internal.h"
#include "game/card_constants.h"
#include "game/display_object.h"
#include "game/display_object_config.h"
#include "game/display_object_core.h"
#include "game/display_object_helpers.h"
#include "game/display_object_work_slots.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_pick_cursor.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_field_display_objects.h"
#include "game/duel_hand.h"
#include "game/duel_scene_hand_actions.h"
#include "game/duel_scene_state.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/func_80018004.h"
#include "game/func_8001B780.h"
#include "game/func_8001B7AC.h"
#include "game/input.h"
#include "game/sound.h"
#include "unmatched.h"
#include <string.h>

void DuelScene_UpdateTurnSwitch(void);

#define STATE_STARTED 0x8000
#define PANEL_IN 2
#define PANEL_OUT 8
#define PANEL_Y 0xAE              /* the hand panel's y once in (the draw's) */
#define HIDDEN_Y 0x292            /* below the screen, where the draw makes the cards */
#define OTHER_CARD_VALUE 1500

enum { DISCARD_NONE, DISCARD_PANEL_IN, DISCARD_PICK, DISCARD_PANEL_OUT };

static void *original_switch;

static int human(void) { return D_8009B360[D_8009B1D5] < 0; }
static DuelSelectionRecord *hand_record(void)
{
    return (DuelSelectionRecord *)(D_800E9F10 + D_8009B1D5 * DUEL_SELECTION_SIDE_SIZE);
}
static int lp_left(void)
{
    return D_800E9FF0[0].life_points.signed_value && D_800E9FF0[1].life_points.signed_value;
}

static int value(int id)
{
    s32 stats = gDuel_adwCardStats[id - 1];
    int attack, defense;
    if (((stats >> CARD_STAT_TYPE_SHIFT) & CARD_STAT_TYPE_MASK) >= CARD_TYPE_MAGIC) return OTHER_CARD_VALUE;
    attack = (stats & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE;
    defense = ((stats >> CARD_STAT_DEFENSE_SHIFT) & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE;
    return attack > defense ? attack : defense;
}

static void cpu_discard(int side, int count)
{
    while (count-- > 0) {
        int slot, worst = -1;
        for (slot = 0; slot < EHP_HAND; slot++)
            if (Ehp_Get(side, slot) >= 0 &&
                (worst < 0 || value(Ehp_CardId(Ehp_Get(side, slot))) < value(Ehp_CardId(Ehp_Get(side, worst)))))
                worst = slot;
        if (worst < 0) break;
        Ehp_Set(side, worst, -1);
    }
    Ehp_Pack(side);
}

/* The hand up on its panel, as the draw puts it up (DuelScene_UpdateDrawPhase). */
static void spread(int side)
{
    DuelSelectionRecord *record = hand_record();
    int slot, count, card;
    Ehp_Pack(side);
    Duel_ClearHandSlots();
    memset(S.xslot, 0, sizeof S.xslot);
    Ehp_ClearBorrowed(side);
    count = Ehp_Count(side);
    for (slot = 0; slot < count; slot++) {
        card = Ehp_Record(side, slot);
        Ehp_SetupRecord(card, Ehp_Get(side, slot));
        Ehp_Slot(slot)->object = (u8 *)func_80018004(&D_801A7AD8[card], Ehp_LayoutX(slot, HAND_SIZE), HIDDEN_Y);
    }
    record->hand = D_800EA030;
    record->field_0E = 0;
    record->field_15 = 0;
    record->field_0C = PANEL_Y;
    S.layout = (u8)count;
    S.focus = 0;
    D_8009B1B4 = (DuelCardPickCursor *)record;
    D_8009B162 = PANEL_IN;
}

/* The hand cursor, as the hand state's first frame makes it. */
static void make_cursor(DuelSelectionRecord *record, int side)
{
    HandCardObject *obj = (HandCardObject *)DisplayObject_AcquireSlot(DisplayObject_FindFreeGeneralSlot(), 2);
    DisplayObject_ConfigureSpriteResource((DisplayObject *)obj, 3, 0, 2, 0xB, 0x20C);
    obj->flags |= 0x28;
    DisplayObject_SelectOrderingTable1((DisplayObject *)obj);
    DisplayObject_SetDepthOffset((DisplayObject *)obj, 0xA);
    record->cursor_object = (struct DisplayObject *)obj;
    func_8001B780((DuelHandStackState *)record);
    func_80023144((DuelFieldDisplaySource *)record, Ehp_Record(side, 0));
}

static void pick(DuelSelectionRecord *record, int side)
{
    int at = (s8)record->field_0E, count = S.layout, value, n, v;
    DuelHandSlot *hand = Ehp_Slot(at), *slot;
    if (gInput_wPad1Repeat & 0xA000) {
        value = at + (gInput_wPad1Repeat & 0x2000 ? 1 : -1);
        if (value < 0 || value >= count) return;
        record->field_0E = (u8)value;
        func_8001B780((DuelHandStackState *)record);
        func_80023144((DuelFieldDisplaySource *)record, Ehp_Record(side, value));
        SD_SEPlayFull(6);
        return;
    }
    if ((gInput_wPad1Pressed & 0xFFFF) == 0x1000) {
        if (!hand->active_09 && record->field_15 < S.discard_left) func_8001B7AC((DuelHandStackState *)record);
        return;
    }
    if ((gInput_wPad1Pressed & PAD_BUTTON_CIRCLE) || (gInput_wPad1Pressed & 0xFFFF) == 0x4000) {
        if (!(value = hand->active_09)) return;
        SD_SEPlayFull(0x2F);
        hand->active_09 = 0;
        ((HandCardObject *)hand->object)->pos.xy.y += 4;
        DisplayObject_ReleaseIfPresent(hand->child);
        hand->child = 0;
        record->field_15--;
        for (n = 0; n < count; n++) {
            slot = Ehp_Slot(n);
            if ((v = slot->active_09) >= value) {
                slot->active_09 = (u8)(v - 1);
                ((HandCardObject *)slot->child)->icon_state = (u8)((v - 2) * 0x10);
            }
        }
        return;
    }
    if (gInput_wPad1Pressed & PAD_BUTTON_CONFIRM_MASK) {
        if (record->field_15 != S.discard_left) {
            SD_SEPlayFull(8);
            return;
        }
        SD_SEPlayFull(7);
        for (n = 0; n < count; n++)
            if (Ehp_Slot(n)->active_09) Ehp_Set(side, n, -1);
        D_8009B1B4 = (DuelCardPickCursor *)record;
        D_8009B162 = PANEL_OUT;
        S.discard = DISCARD_PANEL_OUT;
    }
}

int Ehp_Discard(void)
{
    int side = D_8009B1D5, keep, count;
    DuelSelectionRecord *record = hand_record();
    switch (S.discard) {
    case DISCARD_NONE:
        keep = Ehp_HandKeep();
        count = Ehp_Count(side);
        if (!keep || count <= keep || !lp_left()) return 0;
        if (!human()) {
            cpu_discard(side, count - keep);
            return 0;
        }
        S.discard_left = (u8)(count - keep);
        S.discard = DISCARD_PANEL_IN;
        spread(side);
        return 1;
    case DISCARD_PANEL_IN:
        if (D_8009B162) return 1;
        make_cursor(record, side);
        S.discard = DISCARD_PICK;
        return 1;
    case DISCARD_PICK:
        pick(record, side);
        return 1;
    default:
        if (D_8009B162) return 1;
        Duel_ClearHandSlots();
        memset(S.xslot, 0, sizeof S.xslot);
        Ehp_Pack(side);
        S.discard = DISCARD_NONE;
        S.discard_left = 0;
        S.layout = 0;
        return 0;
    }
}

/* State 10. Before it starts: Main Phase 2 asked for on the field
 * (phases.c), then the discard; then the other side's hand records go back
 * empty, for its draw to rebuild. */
static void turn_switch(void)
{
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) {
        if (Ehp_MainTwo()) return;
        if (Ehp_Discard()) return;
        Ehp_ClearBorrowed(D_8009B1D5);
        S.layout = 0;
    }
    ((void (*)(void))original_switch)();
}

int Ehp_DiscardInstall(void)
{
    return ehp_host->hook(ehp_host, (void *)DuelScene_UpdateTurnSwitch, (void *)turn_switch, &original_switch);
}
