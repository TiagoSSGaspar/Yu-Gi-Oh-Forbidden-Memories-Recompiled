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
#include "../../mods/extra-hand-plays/extra_hand_plays.c"

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
static DuelDeckCardRecord deck[80];
static DuelCardDisplayObject drawn_objects[64];
static int drawn;

static DisplayObject objects[HAND_SIZE];
static int draws, resolutions, hands, fields, dims, applied;
static int plays_setting = 2, sides_setting;
static u16 seen_repeat;

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
void DuelScene_UpdateHandActions(void)
{
    int i;
    hands++;
    if (!(gDuel_wSceneStateFlags & STATE_STARTED)) {
        gDuel_wSceneStateFlags |= STATE_STARTED;
        D_8009B174 = D_8009B360[D_8009B1D5] < 0 ? HAND_IDLE : 2;
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
u8 *Duel_SetupCardRecord(s32 index, s32 position)
{
    int at = D_8009B1D5 * DECK_SIZE + position;
    deck[at].deck_index = (s8)at;
    D_801A7AD8[index].data = (u8 *)&deck[at];
    return 0;
}
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
    if (!strcmp(key, "plays")) return plays_setting;
    if (!strcmp(key, "sides")) return sides_setting;
    return fallback;
}
static int fake_hook(const MemoriesModHost *h, void *fn, void *replacement, void **previous)
{
    (void)h; assert(replacement); *previous = fn; return 1;
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
static int draw_one(int side, int turn, int room)
{
    (void)side; (void)room;
    return turn == 1 ? HAND_SIZE : 1;
}
static MemoriesModHost fake_host;
typedef void (*Step)(void);
static Step step_draw, step_resolution, step_hand, step_field;
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
/* The play: the hand gives up card `slot`, the field starts. */
static void play(int slot)
{
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
    assert(MemoriesModInit(&fake_host, &mod));
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

    /* "Player only" leaves the CPU at one play; an empty hand ends it too;
     * so does a side out of life points; 1 is the original game. */
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
    plays_setting = 1;
    new_turn(0, 0);
    to_hand();
    play(0);
    assert(fields == 1);

    /* A settling panel delays the return to the hand by a frame. */
    plays_setting = 3;
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

    /* Another mod's turn draws: one card, into the first empty slot. */
    assert(provided && provided->version == EXTRA_HAND_API_VERSION);
    provided->set_turn_draws(draw_one);
    plays_setting = 1;
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

    printf("Extra hand plays: rebuilt hands, limits, sides, START, CPU window, turn draws, bonus hands and queued draws passed\n");
    return 0;
}
