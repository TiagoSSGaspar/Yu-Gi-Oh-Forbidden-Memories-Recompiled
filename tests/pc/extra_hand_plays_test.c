/* Compile for i386: drives the Extra hand plays mod's hooks through a
 * turn with stand-ins for the game's scene states, checking where each
 * state hands over and what the rebuilt hand leaves alone. The stand-ins
 * only do what the hooks depend on: the draw's turn-level work and hand
 * compaction, the hand state's five-slot colouring, and the scene flags. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define D_800EAE88_VISIBLE
#define D_800EAE88_AS_BYTES
/* One translation unit: big_hand.c first, for the declarations it asks of
 * unmatched.h; discard.c's own helpers renamed apart from the engine's. */
#include "../../mods/extra-hand-plays/big_hand.c"
#include "../../mods/extra-hand-plays/extra_hand_plays.c"
#define human discard_human
#define hand_record discard_hand_record
#define lp_left discard_lp_left
#include "../../mods/extra-hand-plays/discard.c"
#undef human
#undef hand_record
#undef lp_left
#define human phases_human
#include "../../mods/extra-hand-plays/phases.c"
#undef human

u16 gDuel_wSceneStateFlags;
u16 D_8009B162;
u8 D_8009B174;
DuelCardPickCursor *D_8009B1B4;
u8 D_8009B1D5;
s8 D_8009B360[2];
u8 D_800E9F10[DUEL_SELECTION_SIDE_SIZE * 2];
DuelSideState D_800E9FF0[2];
DuelHandSlot D_800EA030[HAND_SIZE];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
u8 D_801A8000[16], D_801A9800[16];
AiScriptState gAiScript_State;
u8 gDuel_bEffectState;
u8 gDuel_bQuitDialogState;
u16 gInput_wPad1Pressed;
u16 gInput_wPad1Repeat;
DuelSideState *D_8009B1C8;
u8 D_8009B1ED;
u8 D_800907CC[2 * HAND_SIZE + 2] = {0, 1, 2, 3, 4, 0x80, 0x81, 0x82, 0x83, 0x84};
u8 gDuel_bWinnerSide;
DuelDeckCardRecord gDuel_aDeckCardRecords[80];
#define deck gDuel_aDeckCardRecords
static DuelCardDisplayObject drawn_objects[64];
static int drawn;
/* What big_hand.c and discard.c reach of the game. */
u8 D_800907D8[2 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT];
DisplayObject *G32 D_8009B188, *G32 D_8009B18C;
u8 D_8009B19C, D_8009B1D4;
u16 D_8009B210;
u8 D_800E9F2C[DUEL_SELECTION_SIDE_SIZE * 2], D_800E9F48[DUEL_SELECTION_SIDE_SIZE * 2];
u8 D_8015C424[sizeof(DuelCardStagingDeckView)];
DisplayObject *G32 D_800E9EF0[7];
s32 gDuel_adwCardStats[900];
u8 gDuel_bCardViewerYOffset;
u16 gDuel_wViewerCardID;
u16 gInput_wPad1Held;
u8 D_800EAE88[12];
s16 D_8009B20C[2];
static DisplayObject sprites[16];
static int sprite_count, switches, setup_seen, info_record = -1;
int Cards_ExodiaPiece(int id) { (void)id; return 1; }
void *DisplayObject_AcquireSlot(s32 index, s32 key)
{
    (void)index; (void)key;
    assert(sprite_count < 16);
    return &sprites[sprite_count++];
}
s32 DisplayObject_FindFreeGeneralSlot(void) { return 0; }
struct DisplayObject *DisplayObject_ConfigureScreenSprite(struct DisplayObject *object, s32 x, s32 y, s32 height,
                                                          s32 width, s32 a, s32 b, s32 c, s32 d, s32 e)
{
    (void)x; (void)y; (void)height; (void)width; (void)a; (void)b; (void)c; (void)d; (void)e;
    return object;
}
void DisplayObject_ConfigureSpriteAtPosition(void *object, s32 x, s32 y, s32 a, s32 b, s32 c, s32 color, s32 texture)
{
    (void)object; (void)x; (void)y; (void)a; (void)b; (void)c; (void)color; (void)texture;
}
void *DisplayObject_ConfigureSpriteResource(struct DisplayObject *object, s32 a, s32 b, s32 c, s32 color, s32 texture)
{
    (void)a; (void)b; (void)c; (void)color; (void)texture;
    return object;
}
void DisplayObject_InterpolatePositionCosine(DisplayObjectPosition *object, s32 x, s32 y, s32 step)
{
    (void)object; (void)x; (void)y; (void)step;
}
void DisplayObject_ReleaseIfPresent(void *object) { (void)object; }
void DisplayObject_SavePosition(DisplayObjectSnapshot *object) { (void)object; }
void DisplayObject_SelectOrderingTable1(DisplayObject *object) { (void)object; }
s32 DisplayObject_SetDepthOffset(DisplayObject *object, s8 value) { object->field_16 = value; return 0; }
void DuelCard_DeactivateRecord(DuelCardRecord *record) { (void)record; }
void DuelScene_UpdateTurnSwitch(void) { switches++; }
void DuelSelection_LinkDisplayObject(DisplayParent *parent, volatile DisplayObject *object)
{
    (void)parent; (void)object;
}
s32 Duel_CheckQuitInput(void) { return 0; }
void Duel_ClearHandSlots(void) { memset(D_800EA030, 0, sizeof D_800EA030); }
s32 Duel_GetCardViewerRequestId(DuelCardRecord *card) { (void)card; return 0; }
u8 *func_80017F04(DuelCardRecord *card, s32 x, s32 y) { (void)card; (void)x; (void)y; return 0; }
void func_8001B780(DuelHandStackState *state) { (void)state; }
/* The pick number onto the cursor's card, past the fifth slot too, as the
 * game does with this mod's hook in place. */
void func_8001B7AC(DuelHandStackState *state)
{
    state->count++;
    Ehp_Slot(state->slot_index)->active_09 = state->count;
}
void func_8001B938(DuelSelectionRecord *selection) { (void)selection; }
void func_80023144(DuelFieldDisplaySource *source, s32 index) { (void)source; info_record = index; }
s32 func_800235C0(void) { return 0; }
s32 func_80024060(DuelCursorStatus *object) { (void)object; return 1; }
/* A side's snapshot as the disc writes it: an empty field, the hand packed,
 * the upcoming deck with its deck indices, then a terminator. */
void func_80027DF8(AiActiveCard *out, s32 who)
{
    int i;
    memset(out, 0, 2 * DUEL_FIELD_ROW_SIZE * sizeof *out);
    out += 2 * DUEL_FIELD_ROW_SIZE;
    for (i = 0; i < HAND_SIZE; i++)
        if (D_800E9FF0[who].hand[i] >= 0) (out++)->card_id = deck[D_800E9FF0[who].hand[i]].id;
    for (i = D_800E9FF0[who].deck_draw_cursor; i < DECK_SIZE; i++, out++) {
        out->card_id = deck[who * DECK_SIZE + i].id;
        out->deck_index = (u8)(who * DECK_SIZE + i);
    }
    out->card_id = 0;
}

static DisplayObject objects[HAND_SIZE];
static int draws, resolutions, hands, fields, dims, applied;
static int monsters_setting = 2, sides_setting;
static u16 seen_repeat, seen_pressed;
/* What the stand-in hand state does on its next started step: send the
 * cards down to placement, or make the CPU's decision (its substate 0 to 1). */
static int to_placement, decide;

void DuelScene_UpdateDrawPhase(void)
{
    DuelSideState *side = &D_800E9FF0[D_8009B1D5];
    int i, n = 0;
    s8 kept[HAND_SIZE];
    draws++;
    if (gDuel_wSceneStateFlags & STATE_STARTED) {
        if (!D_8009B162) gDuel_wSceneStateFlags = 3;
        return;
    }
    gDuel_wSceneStateFlags |= STATE_STARTED;
    if (side->swords_turns_remaining) side->swords_turns_remaining--;
    side->rank.turns_taken++;
    for (i = 0; i < DUEL_CARD_RECORD_COUNT; i++)
        D_801A7AD8[i].flags &= ~DUEL_CARD_FLAG_USED_THIS_TURN;
    for (i = 0; i < HAND_SIZE; i++) {
        kept[i] = side->hand[i];
        side->hand[i] = -1;
        D_800EA030[i].object = 0;
    }
    for (i = 0; i < HAND_SIZE; i++)
        if (kept[i] >= 0) {
            side->hand[n] = kept[i];
            D_800EA030[n].object = (u8 *)&objects[n];
            n++;
        }
    D_8009B162 = 2;
}
void DuelScene_UpdateDrawResolution(void) { resolutions++; gDuel_wSceneStateFlags = STATE_HAND; }
static int startups;
void DuelScene_UpdateStartup(void) { startups++; }
void DuelScene_UpdateHandActions(void)
{
    int i;
    hands++;
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) {
        gDuel_wSceneStateFlags |= STATE_STARTED;
        D_8009B174 = D_8009B360[D_8009B1D5] < 0 ? HAND_IDLE : 2;
        return;
    }
    seen_pressed = gInput_wPad1Pressed;
    if (to_placement) {
        to_placement = 0;
        gDuel_wSceneStateFlags = STATE_PLACEMENT;
        return;
    }
    if (decide) {
        decide = 0;
        *(u16 *)D_8009B20C = 1;
        return;
    }
    /* The retail idle step colours all five slots on its first frame. */
    if ((D_8009B174 & 0xF) == HAND_IDLE && !(D_8009B174 & HAND_STEP_DONE)) {
        D_8009B174 |= HAND_STEP_DONE;
        for (i = 0; i < HAND_SIZE; i++) {
            assert(D_800EA030[i].object);
            ((DisplayObject *)D_800EA030[i].object)->field_0C = DUEL_DISPLAY_COLOR_NORMAL;
        }
    }
    seen_repeat = gInput_wPad1Repeat;
}
void DuelScene_UpdateFieldActions(void)
{
    fields++;
    gDuel_wSceneStateFlags |= STATE_STARTED;
}
void func_8001B8B8(DuelSelectionRecord *side) { (void)side; dims++; }
s32 Ai_GetHandSize(void) { return 20; }
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; applied++; }
/* The disc's record setup, as far as the position goes: a second-side record
 * takes a position below 40 as that side's deck. With this mod installed
 * the game's calls reach it through the mod's hook (setup in big_hand.c). */
static u8 *setup_record(s32 index, s32 position)
{
    int record = index & 0x80 ? (index & 0x7F) + DUEL_CARD_SIDE_RECORD_COUNT : index;
    setup_seen = position;
    if (index >= DUEL_CARD_SIDE_RECORD_COUNT && position < DECK_SIZE) position += DECK_SIZE;
    if (position & 0x80) position = (position & 0x7F) + DECK_SIZE;
    assert(position >= 0 && position < 2 * DECK_SIZE);
    deck[position].deck_index = (s8)position;
    D_801A7AD8[record].data = (u8 *)&deck[position];
    D_801A7AD8[record].card_id = deck[position].id;
    D_801A7AD8[record].flags = DUEL_CARD_FLAG_OCCUPIED;
    return (u8 *)&D_801A7AD8[record];
}
u8 *Duel_SetupCardRecord(s32 index, s32 position) { return setup(index, position); }
DuelCardDisplayObject *func_80018004(DuelCardRecord *record, s32 x, s32 y)
{
    (void)record; (void)x; (void)y;
    assert(drawn < 64);
    return &drawn_objects[drawn++];
}
void Duel_UpdateDrawCardSlide(DisplayObject *object) { (void)object; }
void *DisplayObject_FindAllocatedByTag(s32 tag) { (void)tag; return 0; }
void SD_SEPlayFull(u32 id) { (void)id; }
s32 Duel_HasAllExodiaPieces(void) { return 0; }

static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{
    (void)h;
    if (!strcmp(key, "monsters")) return monsters_setting;
    if (!strcmp(key, "sides")) return sides_setting;
    return fallback;
}
static int fake_hook(const MemoriesModHost *h, void *fn, void *replacement, void **previous)
{
    (void)h; assert(replacement);
    *previous = fn == (void *)Duel_SetupCardRecord ? (void *)setup_record : fn;
    return 1;
}
static int fake_state(const MemoriesModHost *h, void *data, size_t size, unsigned version)
{
    (void)h; (void)data; (void)size; (void)version; return 1;
}
static const ExtraHandApi *provided;
static int fake_provide(const MemoriesModHost *h, const char *name, void *pointer)
{
    (void)h; assert(!strcmp(name, "api")); provided = pointer; return 1;
}
/* The port's View > Fusion helper, found by name (fusion_helper.h). */
static const void *(*helper_hand)(int index);
static void set_helper_hand(const void *(*slot)(int index)) { helper_hand = slot; }
static void *fake_symbol(const MemoriesModHost *h, const char *name)
{
    (void)h;
    return !strcmp(name, "FusionHelper_SetHand") ? (void *)set_helper_hand : NULL;
}
/* The overlay: a 1280x960 window at the port's scale 2; the hints' boxes. */
static struct { int x, y, w, h; } boxes[8];
static int box_count;
static void fake_overlay_size(const MemoriesModHost *h, int *width, int *height, int *scale)
{ (void)h; *width = 1280; *height = 960; *scale = 2; }
static int fake_text_width(const MemoriesModHost *h, const char *text, int scale)
{ (void)h; return (int)strlen(text) * 6 * scale; }
static void fake_fill(const MemoriesModHost *h, int x, int y, int w, int hh, uint32_t rgb, unsigned alpha)
{
    (void)h; (void)rgb; (void)alpha;
    assert(box_count < 8);
    boxes[box_count].x = x; boxes[box_count].y = y; boxes[box_count].w = w; boxes[box_count++].h = hh;
}
static void fake_draw_text(const MemoriesModHost *h, int x, int middle, const char *text, uint32_t rgb, int scale)
{ (void)h; (void)x; (void)middle; (void)text; (void)rgb; (void)scale; }
static int optional;
static int optional_answer(void) { return optional; }
static int phases_on;
static int phases_answer(void) { return phases_on; }
static int draw_one(int side, int turn, int room)
{
    (void)side; (void)room;
    return turn == 1 ? HAND_SIZE : 1;
}
static void limits_ten(int side, int *max, int *keep)
{
    (void)side;
    *max = 10;
    *keep = 6;
}
static MemoriesModHost fake_host;
typedef void (*Step)(void);
static Step step_startup, step_draw, step_resolution, step_hand, step_field;
static void (*step_dim)(DuelSelectionRecord *);
static s32 (*step_window)(void);

/* A new turn for `side` with a full hand; the scene enters the draw. */
static void new_turn(int side, int cpu)
{
    int i;
    memset(D_800EA030, 0, sizeof D_800EA030);
    memset(D_801A7AD8, 0, sizeof D_801A7AD8);
    D_8009B1D5 = side;
    D_8009B1C8 = &D_800E9FF0[side];
    D_8009B360[side] = cpu ? 1 : -1;
    for (i = 0; i < HAND_SIZE; i++) D_800E9FF0[side].hand[i] = i;
    D_800E9FF0[0].life_points.signed_value = D_800E9FF0[1].life_points.signed_value = 8000;
    gDuel_wSceneStateFlags = STATE_DRAW;
    D_8009B162 = 0;
    draws = resolutions = hands = fields = 0;
}
/* Run the scene from the draw to the hand's first idle frame. */
static void to_hand(void)
{
    step_draw();
    assert(gDuel_wSceneStateFlags == (STATE_DRAW | STATE_STARTED));
    D_8009B162 = 0;                      /* the panel has slid in */
    step_draw();
    assert(gDuel_wSceneStateFlags == 3);
    for (int frames = 0; (gDuel_wSceneStateFlags & STATE_MASK) == 3; frames++) {
        assert(frames < 200);
        step_resolution();
    }
    assert(gDuel_wSceneStateFlags == STATE_HAND);
    step_hand();                         /* the hand state's setup */
    step_hand();                         /* its first idle step */
}
/* The play: the hand puts card `slot` down -- a monster, through
 * placement, when the hand state is up -- and the field starts. */
static DisplayObject placed;
static void play(int slot)
{
    placed.field_6A = (u8)Ehp_Record(D_8009B1D5, slot);
    if (!D_801A7AD8[placed.field_6A].card_id) D_801A7AD8[placed.field_6A].card_id = 1;
    D_800E9EF0[0] = &placed;
    D_800E9EF0[1] = 0;
    if ((gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) == (STATE_HAND | STATE_STARTED)) {
        to_placement = 1;
        step_hand();
    }
    memset(D_800EA030, 0, sizeof D_800EA030);
    D_800E9FF0[D_8009B1D5].hand[slot] = -1;
    gDuel_wSceneStateFlags = STATE_FIELD;
    step_field();
}

int main(void)
{
    MemoriesMod mod = {0};
    DuelSelectionRecord *side;
    fake_host.api = 4; fake_host.setting = fake_setting; fake_host.hook = fake_hook;
    fake_host.register_state = fake_state;
    fake_host.provide = fake_provide;
    fake_host.symbol = fake_symbol;
    fake_host.overlay_size = fake_overlay_size; fake_host.text_width = fake_text_width;
    fake_host.fill = fake_fill; fake_host.draw_text = fake_draw_text;
    assert(MemoriesModInit(&fake_host, &mod));
    assert(helper_hand && helper_hand(0) == &D_800EA030[0] && helper_hand(4) == &D_800EA030[4] && !helper_hand(5));
    for (int i = 0; i < 80; i++) deck[i].id = (s16)(100 + i);     /* monsters (their stats say type 0) */
    step_startup = (Step)startup;
    step_draw = (Step)draw; step_resolution = (Step)resolution;
    step_hand = (Step)hand; step_field = (Step)field;
    step_dim = dim; step_window = window;

    /* The player's turn: the first play sends the field back to the draw. */
    new_turn(0, 0);
    D_800E9FF0[0].swords_turns_remaining = 2;
    to_hand();
    assert(S.plays == 1 && D_800E9FF0[0].rank.turns_taken == 1 && D_800E9FF0[0].swords_turns_remaining == 1);
    D_801A7AD8[6].flags = DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_USED_THIS_TURN;
    D_801A7AD8[6].object = (DisplayObject *)&objects[0];
    play(1);
    assert(fields == 0 && gDuel_wSceneStateFlags == STATE_DRAW && S.redraw);

    /* The rebuilt hand: no turn, no Swords tick, no draw, used marks kept. */
    applied = resolutions = 0;
    to_hand();
    assert(resolutions == 0 && !S.redraw && S.plays == 2);
    assert(D_800E9FF0[0].rank.turns_taken == 1 && D_800E9FF0[0].swords_turns_remaining == 1);
    assert(D_801A7AD8[6].flags & DUEL_CARD_FLAG_USED_THIS_TURN);
    assert(applied == 1);
    assert(D_800EA030[3].object && !D_800EA030[4].object);   /* compacted to four */

    /* The cursor cannot step right off the last card; it can elsewhere. */
    side = hand_record();
    side->field_0E = 3;
    gInput_wPad1Repeat = PAD_RIGHT;
    step_hand();
    assert(!(seen_repeat & PAD_RIGHT) && gInput_wPad1Repeat == PAD_RIGHT);
    side->field_0E = 2;
    step_hand();
    assert(seen_repeat & PAD_RIGHT);
    gInput_wPad1Repeat = 0;

    /* Dimming skips the empty slot. */
    dims = 0;
    side->field_15 = 0;
    step_dim(side);
    assert(dims == 0 && objects[2].field_0C == DUEL_DISPLAY_COLOR_NORMAL &&
           objects[0].field_0C == DUEL_DISPLAY_COLOR_DIMMED);

    /* START: the panel slides away, then the field, with no third hand. */
    gInput_wPad1Pressed = PAD_BUTTON_START;
    step_hand();
    gInput_wPad1Pressed = 0;
    assert(S.skipping && S.done && D_8009B162 == PANEL_SLIDE_OUT);
    assert(D_8009B1B4 == (DuelCardPickCursor *)side);
    hands = 0;
    step_hand();
    assert(hands == 0 && gDuel_wSceneStateFlags == (STATE_HAND | STATE_STARTED));
    D_8009B162 = 0;
    step_hand();
    assert(gDuel_wSceneStateFlags == STATE_FIELD);
    step_field();
    assert(fields == 1 && !S.pending);

    /* The limit: two plays, then the field. */
    new_turn(1, 1);
    to_hand();
    play(0);
    to_hand();
    play(0);
    assert(fields == 1 && gDuel_wSceneStateFlags == (STATE_FIELD | STATE_STARTED));
    /* A battle later in the turn returns to a field that stays a field. */
    gDuel_wSceneStateFlags = STATE_FIELD;
    step_field();
    assert(fields == 2);

    /* The CPU's extra hand keeps its window to the cards in hand. */
    new_turn(1, 1);
    gAiScript_State.script_base = D_801A8000;
    to_hand();
    assert(step_window() == 20);
    play(4);
    to_hand();
    assert(step_window() == 4);
    gAiScript_State.script_base = D_801A9800;   /* the field script: untouched */
    assert(step_window() == 20);

    /* START does nothing in the first hand. */
    new_turn(0, 0);
    to_hand();
    gInput_wPad1Pressed = PAD_BUTTON_START;
    step_hand();
    assert(!S.skipping);
    gInput_wPad1Pressed = 0;
    /* Unless another mod makes playing a card optional: then START ends the
     * first hand too, and no other hand follows this turn. */
    provided->set_optional_play(optional_answer);
    optional = 1;
    new_turn(0, 0);
    to_hand();
    gInput_wPad1Pressed = PAD_BUTTON_START;
    step_hand();
    gInput_wPad1Pressed = 0;
    assert(S.skipping && S.done && D_8009B162 == PANEL_SLIDE_OUT);
    D_8009B162 = 0;
    step_hand();
    assert(gDuel_wSceneStateFlags == STATE_FIELD);
    step_field();
    assert(fields == 1);
    optional = 0;
    new_turn(0, 0);
    to_hand();
    gInput_wPad1Pressed = PAD_BUTTON_START;
    step_hand();
    gInput_wPad1Pressed = 0;
    assert(!S.skipping);
    provided->set_optional_play(0);

    /* "Player only" leaves the CPU at one play; an empty hand ends it too;
     * so does a side out of life points; one monster a turn, with nothing
     * but monsters in hand, is the original game. */
    sides_setting = 1;
    new_turn(1, 1);
    to_hand();
    play(0);
    assert(fields == 1);
    sides_setting = 0;
    new_turn(0, 0);
    to_hand();
    memset(D_800E9FF0[0].hand, -1, HAND_SIZE);
    gDuel_wSceneStateFlags = STATE_FIELD;
    step_field();
    assert(fields == 1);
    new_turn(0, 0);
    to_hand();
    D_800E9FF0[1].life_points.signed_value = 0;
    play(0);
    assert(fields == 1);
    monsters_setting = 1;
    new_turn(0, 0);
    to_hand();
    play(0);
    assert(fields == 1);

    /* A settling panel delays the return to the hand by a frame. */
    monsters_setting = 3;
    new_turn(0, 0);
    to_hand();
    D_800E9FF0[0].hand[0] = -1;
    gDuel_wSceneStateFlags = STATE_FIELD;
    D_8009B162 = 3;
    step_field();
    assert(gDuel_wSceneStateFlags == STATE_FIELD && fields == 0);
    D_8009B162 = 0;
    step_field();
    assert(gDuel_wSceneStateFlags == STATE_DRAW);
    to_hand();
    play(1);
    assert(gDuel_wSceneStateFlags == STATE_DRAW && S.plays == 2);
    to_hand();
    assert(S.plays == 3 && D_800EA030[2].object && !D_800EA030[3].object);

    /* A panel move that never ends does not hold the hand back for good. */
    monsters_setting = 2;
    new_turn(0, 0);
    to_hand();
    D_800E9FF0[0].hand[0] = -1;
    gDuel_wSceneStateFlags = STATE_FIELD;
    D_8009B162 = 3;
    for (int frame = 0; frame < SETTLE_FRAMES; frame++) {
        step_field();
        assert(gDuel_wSceneStateFlags == STATE_FIELD);
    }
    step_field();
    assert(gDuel_wSceneStateFlags == STATE_DRAW && fields == 0);
    D_8009B162 = 0;
    to_hand();                                 /* the extra hand comes up */
    assert(S.hands == 2);

    /* Another mod's turn draws: one card, into the first empty slot. */
    assert(provided && provided->version == EXTRA_HAND_API_VERSION);
    provided->set_turn_draws(draw_one);
    monsters_setting = 1;
    new_turn(0, 0);
    D_800E9FF0[0].rank.turns_taken = 4;
    D_800E9FF0[0].deck_draw_cursor = 10;
    D_800E9FF0[0].hand[1] = D_800E9FF0[0].hand[3] = -1;    /* three cards kept */
    to_hand();
    assert(D_800E9FF0[0].deck_draw_cursor == 11 && D_800E9FF0[0].hand[3] == 10);
    assert(D_800E9FF0[0].hand[4] < 0 && D_800EA030[3].object && !D_800EA030[4].object);
    /* A full hand has no room: the turn's card waits instead. */
    new_turn(0, 0);
    to_hand();
    assert(D_800E9FF0[0].deck_draw_cursor == 11 && provided->queued(0) == 1);

    /* A bonus hand (a draw-two card used from the hand): it draws the two and
     * the waiting card as far as there is room, and is not counted as a play. */
    play(0);                                   /* plays = 1: no extra hand */
    assert(fields == 1);
    provided->bonus_hand(2);
    gDuel_wSceneStateFlags = STATE_FIELD;      /* the card's use returns to the field */
    step_field();
    assert(gDuel_wSceneStateFlags == STATE_DRAW && fields == 1);
    D_800E9FF0[0].hand[2] = -1;                /* stand-in: the card was used */
    to_hand();
    assert(S.plays == 1 && S.hands == 2);
    assert(D_800E9FF0[0].deck_draw_cursor == 13 && provided->queued(0) == 1 && hand_count() == 5);
    provided->bonus_hand(2);
    play(0);
    assert(gDuel_wSceneStateFlags == STATE_DRAW);
    to_hand();
    assert(D_800E9FF0[0].deck_draw_cursor == 14 && provided->queued(0) == 2);
    /* The next turn draws its card and the waiting ones as room allows. */
    D_800E9FF0[0].hand[0] = D_800E9FF0[0].hand[1] = -1;
    gDuel_wSceneStateFlags = STATE_DRAW;
    to_hand();
    assert(D_800E9FF0[0].deck_draw_cursor == 16 && provided->queued(0) == 1 && hand_count() == 5);
    /* The disc's refill again once nothing sets the draws. */
    provided->set_turn_draws(0);
    new_turn(0, 0);
    D_800E9FF0[0].hand[0] = -1;
    to_hand();
    assert(resolutions == 1);

    /* Drawing from an empty deck loses, as the turn's draw does. */
    provided->set_turn_draws(draw_one);
    new_turn(1, 1);
    D_800E9FF0[1].rank.turns_taken = 6;
    D_800E9FF0[1].deck_draw_cursor = DECK_SIZE;
    D_800E9FF0[1].hand[0] = -1;
    step_draw();
    D_8009B162 = 0;
    step_draw();
    for (int frames = 0; gDuel_wSceneStateFlags == 3 || gDuel_wSceneStateFlags == (3 | STATE_STARTED); frames++) {
        assert(frames < 50);
        step_resolution();
    }
    assert(gDuel_wSceneStateFlags == 0xC && gDuel_bWinnerSide == 0);

    /* The disc's refill, on a turn that starts with a full hand (the last
     * one was skipped with START, playing nothing): nothing to draw. The
     * retail draw resolution would deal past the fifth slot. */
    provided->set_turn_draws(0);
    new_turn(0, 0);
    D_800E9FF0[0].deck_draw_cursor = 12;
    to_hand();
    assert(resolutions == 0 && D_800E9FF0[0].deck_draw_cursor == 12 && hand_count() == HAND_SIZE);

    /* Another mod passes for the CPU (version 3), from inside its hand
     * script: the hand slides away as START's does, then the field, and no
     * other hand comes this turn. Outside a hand it does nothing. */
    assert(provided->version >= 3);
    monsters_setting = 3;
    new_turn(1, 1);
    to_hand();
    provided->pass_hand();
    assert(S.skipping && S.done && D_8009B162 == PANEL_SLIDE_OUT);
    assert(D_8009B1B4 == (DuelCardPickCursor *)hand_record());
    hands = 0;
    step_hand();
    assert(hands == 0 && gDuel_wSceneStateFlags == (STATE_HAND | STATE_STARTED));
    D_8009B162 = 0;
    step_hand();
    assert(gDuel_wSceneStateFlags == STATE_FIELD);
    step_field();
    assert(fields == 1 && !S.pending);
    S.skipping = S.done = 0;
    provided->pass_hand();
    assert(!S.skipping && !S.done && D_8009B162 == 0);
    monsters_setting = 2;

    /* A new duel starts with nothing left over from the last one: the side
     * that goes first never passes through the draw, which resets a turn. */
    S.queued[0] = 2; S.queued[1] = 1; S.bonus = 1; S.bonus_draws = 2; S.plays = 2; S.hands = 3;
    S.pending = S.done = S.custom = 1; S.to_draw = 3; S.settle = 4;
    gDuel_wSceneStateFlags = 1;
    startups = 0;
    step_startup();
    assert(startups == 1);
    {
        EhpState fresh;                        /* every slot past the fifth empty */
        memset(&fresh, 0, sizeof fresh);
        memset(fresh.xhand, -1, sizeof fresh.xhand);
        assert(!memcmp(&S, &fresh, sizeof S));
    }
    S.queued[0] = 1;                           /* its later frames leave the turn alone */
    gDuel_wSceneStateFlags = 1 | STATE_STARTED;
    step_startup();
    assert(startups == 2 && S.queued[0] == 1);


    /* Past five cards: another mod's limits (ten, six kept at the end of a
     * turn) let the turn's card go past the fifth slot, into a record
     * borrowed from the other side's hand. */
    gDuel_wSceneStateFlags = 1;
    step_startup();
    for (int i = 0; i < 80; i++) deck[i].id = (s16)(100 + i);
    for (int i = 0; i < 900; i++) gDuel_adwCardStats[i] = 50;     /* monsters, 500/0 */
    provided->set_turn_draws(draw_one);
    provided->set_hand_limits(limits_ten);
    monsters_setting = 1;
    drawn = 0;
    new_turn(0, 0);
    D_800E9FF0[0].rank.turns_taken = 4;
    D_800E9FF0[0].deck_draw_cursor = 10;
    to_hand();
    assert(D_800E9FF0[0].deck_draw_cursor == 11 && Ehp_Count(0) == 6 && Ehp_Extras(0) == 1);
    assert(S.xhand[0][0] == 10 && Ehp_Record(0, 5) == 15 && D_801A7AD8[15].data == (u8 *)&deck[10]);
    assert(S.layout == 6 && S.xslot[0].object && provided->hand_slots() == 6 && provided->hand_card(5) == 110);
    assert(helper_hand(4) == &D_800EA030[4] && helper_hand(5) == &S.xslot[0] && !helper_hand(6));
    assert(Ehp_Record(1, 7) == 2);                       /* the second side borrows the first's */
    /* The next turn packs: gaps in the five fill from past the fifth, in order. */
    D_800E9FF0[0].hand[1] = D_800E9FF0[0].hand[3] = -1;
    S.xhand[0][1] = 11;
    D_800E9FF0[0].deck_draw_cursor = 12;
    gDuel_wSceneStateFlags = STATE_DRAW;
    D_8009B162 = 0;
    to_hand();
    {
        static const s8 packed[5] = {0, 2, 4, 10, 11};
        assert(!memcmp(D_800E9FF0[0].hand, packed, 5) && S.xhand[0][0] == 12 && S.xhand[0][1] < 0);
        assert(Ehp_Count(0) == 6 && S.layout == 6);
    }
    /* Without limits the hand stays at five: the card waits. */
    provided->set_hand_limits(0);
    new_turn(0, 0);
    memset(S.xhand, -1, sizeof S.xhand);
    to_hand();
    assert(Ehp_Count(0) == 5 && !Ehp_Extras(0) && provided->queued(0) == 1 && S.layout == 5);
    provided->set_hand_limits(limits_ten);
    S.queued[0] = 0;

    /* Record setup: during the first side's turn, a borrowed record (15-19)
     * with a position below 40 reaches the disc 40 short, for its addition
     * to give the first side's card back; anything else passes as is. */
    D_8009B1D5 = 0;
    setup(15, 7);
    assert(setup_seen == 7 - DECK_SIZE && D_801A7AD8[15].data == (u8 *)&deck[7]);
    setup(0x80 | 4, 9);
    assert(setup_seen == 9 - DECK_SIZE && D_801A7AD8[19].data == (u8 *)&deck[9]);
    setup(15, 45);
    assert(setup_seen == 45);
    setup(5, 7);
    assert(setup_seen == 7);
    setup(20, 7);
    assert(setup_seen == 7);                             /* a field record of the second side */
    D_8009B1D5 = 1;
    setup(15, 7);
    assert(setup_seen == 7 && D_801A7AD8[15].data == (u8 *)&deck[47]);
    setup(2, 45);
    assert(setup_seen == 45 && D_801A7AD8[2].data == (u8 *)&deck[45]);

    /* The CPU's snapshot: the cards past the fifth right after the hand,
     * numbered like deck cards, the deck and its terminator moved down. */
    {
        AiActiveCard out[60];
        static const s8 five[5] = {40, 41, 42, 43, 44};
        memset(out, 0x55, sizeof out);
        memcpy(D_800E9FF0[1].hand, five, 5);
        memset(S.xhand[1], -1, EHP_EXTRA);
        S.xhand[1][0] = 45; S.xhand[1][1] = 46;
        D_800E9FF0[1].deck_draw_cursor = 7;
        snapshot(out, 1);
        assert(out[10].card_id == 140 && out[14].card_id == 144);
        assert(out[15].card_id == 145 && out[15].deck_index == 45 && out[16].card_id == 146 && out[16].deck_index == 46);
        assert(out[15].attack == 500 && out[15].flags == 0);
        assert(out[17].card_id == 147 && out[17].deck_index == 47);
        assert(out[17 + 32].card_id == 179 && out[17 + 33].card_id == 0);
        memset(S.xhand[1], -1, EHP_EXTRA);
        snapshot(out, 1);                                /* none past the fifth: the disc's */
        assert(out[15].card_id == 147 && out[15].deck_index == 47);
    }

    /* The hand script's window takes in the cards past the fifth. */
    D_8009B1D5 = 1;
    S.xhand[1][0] = 45; S.xhand[1][1] = 46;
    gAiScript_State.script_base = D_801A8000;
    gDuel_wSceneStateFlags = STATE_HAND | STATE_STARTED;
    assert(step_window() == 22);
    gAiScript_State.script_base = D_801A9800;
    assert(step_window() == 20);

    /* The layout: five as the disc spaces them, more spread over the same width. */
    assert(Ehp_LayoutX(0, 5) == 14 && Ehp_LayoutX(4, 5) == 254 && Ehp_LayoutX(3, 3) == 194);
    assert(Ehp_LayoutX(0, 8) == 14 && Ehp_LayoutX(7, 8) == 254 && Ehp_LayoutX(4, 8) == 14 + 4 * 240 / 7);

    /* The end of the CPU's turn with eight cards: its two weakest go at
     * once, the rest packed, and the turn switch runs the same frame. */
    {
        static const s8 five[5] = {40, 41, 42, 43, 44}, kept[5] = {40, 41, 43, 44, 45};
        D_8009B1D5 = 1;
        D_8009B360[1] = 1;
        memcpy(D_800E9FF0[1].hand, five, 5);
        memset(S.xhand[1], -1, EHP_EXTRA);
        S.xhand[1][0] = 45; S.xhand[1][1] = 46; S.xhand[1][2] = 47;
        gDuel_adwCardStats[deck[42].id - 1] = 1;          /* 10/0 */
        gDuel_adwCardStats[deck[46].id - 1] = 2 << CARD_STAT_DEFENSE_SHIFT;   /* 0/20 */
        gDuel_wSceneStateFlags = 10;
        switches = 0;
        turn_switch();
        assert(switches == 1 && !S.discard && Ehp_Count(1) == 6);
        assert(!memcmp(D_800E9FF0[1].hand, kept, 5) && S.xhand[1][0] == 47 && S.xhand[1][1] < 0);
    }
    /* The player's: the hand comes up, two cards are marked (one past the
     * fifth), a confirm with too few is refused, then they go with the panel. */
    {
        static const s8 five[5] = {0, 1, 2, 3, 4}, kept[5] = {1, 2, 3, 4, 5};
        DuelSelectionRecord *record = (DuelSelectionRecord *)D_800E9F10;
        D_8009B1D5 = 0;
        D_8009B360[0] = -1;
        drawn = sprite_count = 0;
        memcpy(D_800E9FF0[0].hand, five, 5);
        memset(S.xhand[0], -1, EHP_EXTRA);
        S.xhand[0][0] = 5; S.xhand[0][1] = 6; S.xhand[0][2] = 7;
        gDuel_wSceneStateFlags = 10;
        switches = 0;
        turn_switch();
        assert(switches == 0 && S.discard && S.discard_left == 2 && S.layout == 8);
        assert(D_8009B162 == 2 && D_8009B1B4 == (DuelCardPickCursor *)record && record->hand == D_800EA030);
        assert(D_800EA030[4].object && S.xslot[2].object && D_801A7AD8[17].data == (u8 *)&deck[7]);
        turn_switch();                                   /* the panel still sliding in */
        assert(switches == 0 && !record->cursor_object);
        D_8009B162 = 0;
        turn_switch();
        assert(record->cursor_object && info_record == 0);
        gInput_wPad1Pressed = 0x1000;                    /* UP: the first card */
        turn_switch();
        gInput_wPad1Pressed = PAD_BUTTON_CROSS;          /* one of two: refused */
        turn_switch();
        assert(switches == 0 && D_8009B162 == 0 && record->field_15 == 1);
        gInput_wPad1Pressed = 0;
        gInput_wPad1Repeat = PAD_RIGHT;
        for (int i = 0; i < 6; i++) turn_switch();
        gInput_wPad1Repeat = 0;
        assert(record->field_0E == 6 && info_record == Ehp_Record(0, 6));
        gInput_wPad1Pressed = 0x1000;
        turn_switch();
        gInput_wPad1Pressed = PAD_BUTTON_CROSS;
        turn_switch();
        gInput_wPad1Pressed = 0;
        assert(D_8009B162 == 8 && switches == 0 && Ehp_Count(0) == 6);
        turn_switch();                                   /* the panel sliding away */
        assert(switches == 0);
        D_8009B162 = 0;
        turn_switch();
        assert(switches == 1 && !S.discard && !S.layout);
        assert(!memcmp(D_800E9FF0[0].hand, kept, 5) && S.xhand[0][0] == 7 && S.xhand[0][1] < 0);
        /* Six or fewer: no discard. */
        switches = 0;
        gDuel_wSceneStateFlags = 10;
        turn_switch();
        assert(switches == 1 && !S.discard);
    }

    /* Exodia over every slot: three pieces in the five, two past them. */
    {
        static const s8 five[5] = {0, 1, 2, 3, 4};
        D_8009B1D5 = 0;
        D_8009B1C8 = &D_800E9FF0[0];
        for (int i = 0; i < 7; i++) deck[i].id = (s16)(200 + i);
        deck[1].id = EXODIA_FIRST_CARD_ID; deck[3].id = EXODIA_FIRST_CARD_ID + 1; deck[4].id = EXODIA_FIRST_CARD_ID + 2;
        deck[5].id = EXODIA_FIRST_CARD_ID + 3; deck[6].id = EXODIA_FIRST_CARD_ID + 4;
        memcpy(D_800E9FF0[0].hand, five, 5);
        memset(S.xhand[0], -1, EHP_EXTRA);
        S.xhand[0][0] = 5; S.xhand[0][1] = 6;
        assert(exodia() == 1);
        deck[6].id = 206;
        assert(exodia() == 0);
        deck[6].id = EXODIA_FIRST_CARD_ID + 4;
        S.xhand[0][1] = 3;                               /* the same piece twice is not two */
        assert(exodia() == 0);
    }

    /* Monster plays: one a turn by default; magic, trap and equip cards are
     * free, and the hand comes back while it holds one. Once the monster is
     * down the player's confirm of another is refused, a magic card's is
     * not; the CPU passes when its decision holds a monster. */
    {
        DuelSelectionRecord *me;
        int magic = 600;
        gDuel_adwCardStats[magic - 1] = CARD_TYPE_MAGIC << CARD_STAT_TYPE_SHIFT;
        provided->set_turn_draws(0);
        provided->set_hand_limits(0);
        monsters_setting = 1;
        new_turn(0, 0);
        deck[2].id = (s16)magic;
        to_hand();
        play(0);                                   /* the turn's monster */
        assert(S.monsters == 1 && gDuel_wSceneStateFlags == STATE_DRAW);    /* a magic card is left */
        to_hand();
        me = hand_record();
        me->field_15 = 0;
        me->field_0E = 0;                          /* a monster */
        gInput_wPad1Pressed = PAD_BUTTON_CROSS;
        step_hand();
        assert(!(seen_pressed & PAD_BUTTON_CROSS) && gInput_wPad1Pressed == PAD_BUTTON_CROSS);
        me->field_0E = 1;                          /* the magic card */
        step_hand();
        assert(seen_pressed & PAD_BUTTON_CROSS);
        me->field_0E = 0;                          /* marked: the monster with the magic card */
        me->field_15 = 2;
        D_800EA030[0].active_09 = 1;
        D_800EA030[1].active_09 = 2;
        step_hand();
        assert(!(seen_pressed & PAD_BUTTON_CROSS));
        D_800EA030[0].active_09 = D_800EA030[1].active_09 = 0;
        me->field_15 = 0;
        gInput_wPad1Pressed = 0;
        D_800E9FF0[0].hand[1] = -1;                /* the magic card used: monsters only now */
        gDuel_wSceneStateFlags = STATE_FIELD;
        step_field();
        assert(fields == 1 && S.monsters == 1);
        /* The CPU: a monster picked once its monster is down ends its hands. */
        new_turn(1, 1);
        to_hand();
        play(0);
        assert(gDuel_wSceneStateFlags == STATE_DRAW);
        to_hand();
        *(u16 *)D_8009B20C = 0x8000;
        memset(D_800EAE88, 0, sizeof D_800EAE88);
        D_800EAE88[0] = AI_HAND_SLOT_FIRST;        /* slot 0: a monster */
        decide = 1;
        step_hand();
        assert(S.skipping && S.done && D_8009B162 == PANEL_SLIDE_OUT);
        S.skipping = S.done = 0;
        D_8009B162 = 0;
        *(u16 *)D_8009B20C = 0x8000;
        D_800EAE88[0] = AI_HAND_SLOT_FIRST + 1;    /* slot 1: the magic card */
        decide = 1;
        step_hand();
        assert(!S.skipping && !S.done);
        deck[2].id = 102;
    }

    /* Phases (set_phases): START in a hand opens the phase menu, which
     * holds the hand state while open; UP/DOWN move, circle closes, X takes.
     * Main Phase 1 goes to the Battle Phase (the field) or the End Phase
     * (the turn switch, with no field). Off, START slides the hand away at
     * once; the CPU never gets the menu. */
    {
        int items[2];
        provided->set_hand_limits(0);
        memset(S.xhand, -1, sizeof S.xhand);
        S.queued[0] = S.queued[1] = 0;
        phases_on = 1;
        optional = 1;
        provided->set_optional_play(optional_answer);
        provided->set_phases(phases_answer);
        new_turn(0, 0);
        to_hand();
        side = hand_record();
        side->field_15 = 0;
        hands = 0;
        gInput_wPad1Pressed = PAD_BUTTON_START; step_hand();
        assert(S.menu == 1 && !S.skipping && hands == 0);
        assert(Ehp_PhaseItems(0, items) == 2 && items[0] == PHASE_BATTLE && items[1] == PHASE_END);
        gInput_wPad1Pressed = PAD_DIRECTION_DOWN; step_hand(); assert(S.menu == 2);
        gInput_wPad1Pressed = PAD_DIRECTION_UP; step_hand(); assert(S.menu == 1);
        gInput_wPad1Pressed = PAD_BUTTON_CIRCLE; step_hand(); assert(S.menu == 0 && !S.skipping);
        gInput_wPad1Pressed = PAD_BUTTON_START; step_hand();
        gInput_wPad1Pressed = PAD_BUTTON_CROSS; step_hand();
        gInput_wPad1Pressed = 0;
        assert(!S.menu && S.skipping && !S.to_end && D_8009B162 == PANEL_SLIDE_OUT && hands == 0);
        D_8009B162 = 0; step_hand();
        assert(gDuel_wSceneStateFlags == STATE_FIELD);

        new_turn(0, 0);
        to_hand();
        gInput_wPad1Pressed = PAD_BUTTON_START; step_hand();
        gInput_wPad1Pressed = PAD_DIRECTION_DOWN; step_hand();
        gInput_wPad1Pressed = PAD_BUTTON_CROSS; step_hand();
        gInput_wPad1Pressed = 0;
        assert(S.skipping && S.to_end);
        D_8009B162 = 0; step_hand();
        assert(gDuel_wSceneStateFlags == STATE_TURN_SWITCH && !S.to_end);

        phases_on = 0;
        new_turn(0, 0);
        to_hand();
        gInput_wPad1Pressed = PAD_BUTTON_START; step_hand(); gInput_wPad1Pressed = 0;
        assert(!S.menu && S.skipping);
        phases_on = 1;
        new_turn(1, 1);
        assert(!Ehp_Phases());
        new_turn(0, 0);
        assert(Ehp_Phases());
    }

    /* The hints sit under the strip above the FIELD box (the picture's
     * lines 0-23), where View > Fusion helper draws, centred between the
     * FIELD box (to x 67) and the life points (from x 248). */
    D_8009B1D5 = 0; D_8009B360[0] = -1;
    S.discard = 1; S.discard_left = 1;
    box_count = 0;
    mod.overlay();
    assert(box_count == 2);
    for (int i = 0; i < box_count; i++) {
        assert(boxes[i].y >= 960 * 24 / 240);
        assert(boxes[i].x > 1280 * 67 / 320 && boxes[i].x + boxes[i].w < 1280 * 248 / 320);
        assert(!i || boxes[i].y >= boxes[i - 1].y + boxes[i - 1].h);
    }
    S.discard = 0;

    printf("Extra hand plays: rebuilt hands, limits, monster plays, sides, START, CPU window, optional first play, turn draws, bonus hands, queued draws, hands past five (draws, packing, records, snapshot, window, layout, discard, Exodia, fusion helper) passed\n");
    return 0;
}
