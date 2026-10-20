/* Compile for i386: the Modern rules mod's turn draws, fusion-chain guard,
 * Pot of Greed effect and the CPU's use of it, against stand-ins for the
 * game and for Extra hand plays' table. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/modern_rules.c"

u16 gDuel_wSceneStateFlags;
u8 D_8009B174;
u8 D_8009B1D5;
s8 D_8009B360[2];
u8 D_800E9F10[DUEL_SELECTION_SIDE_SIZE * 2];
DuelSideState D_800E9FF0[2];
DuelHandSlot D_800EA030[HAND_SIZE];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
u8 D_800907CC[2 * HAND_SIZE + 2] = {0, 1, 2, 3, 4, 0x80, 0x81, 0x82, 0x83, 0x84};
u8 D_801A8000[16], D_801A9800[16];
AiScriptState gAiScript_State;
u8 gDuel_bEffectState;
u8 gDuel_bQuitDialogState;
u16 gInput_wPad1Pressed;
unsigned char D_800EAE88[12];
DuelCardPickCursor *D_8009B1B4;
/* Side 0's grid: rows of five, the monsters (row 2) on records 5-9. */
u8 D_800907D8[2 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT] = {
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
static DuelCardPickCursor field_cursor;

#define POT 800
static void (*mod_frame)(void);
static void frames(int n) { while (n--) mod_frame(); }
u16 gDuel_awPlayerDeck[DECK_SIZE + 40];
static unsigned char pot_trunk;
static int save_loaded = 1, chest_on_screen;
int Cheats_SaveLoaded(void) { return save_loaded; }
int Cheats_ChestOnScreen(void) { return chest_on_screen; }
unsigned char *Cards_ChestSlot(void *state, int id)
{
    assert(state == gDuel_awPlayerDeck && id == POT);
    return &pot_trunk;
}
static int sounds, seen_pressed, bonus_draws, bonus_calls, applied_flag = 1;
static ExtraHandTurnDraws registered;
static MemoriesModCallback subscribed;

/* Cards 1 + 2 fuse into 3; 3 + 4 into 5; 6 equips onto any monster. */
s32 Duel_CheckFusion(s32 a, s32 b)
{
    if ((a == 1 && b == 2) || (a == 2 && b == 1)) return 3;
    if ((a == 3 && b == 4) || (a == 4 && b == 3)) return 5;
    return 0;
}
s32 Duel_CheckEquip(s32 equip, s32 monster)
{
    if (equip == 8) return monster == 1 ? monster : 0;      /* 8 equips 1 only */
    return equip == 6 && monster != 6 ? monster : 0;
}
void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
void DuelScene_UpdateHandActions(void) { seen_pressed = gInput_wPad1Pressed; }
void DuelScene_UpdateFieldActions(void) { seen_pressed = gInput_wPad1Pressed; }
/* The CPU's hand decision the stand-in script makes: picks, then [6] the
 * zone (1-5 the monster zones), [8] face down. */
static unsigned char ai_choice[12] = {13, 0, 0, 0, 0, 0, 2};
s32 AiScript_Run(void) { memcpy(D_800EAE88, ai_choice, 12); return 1; }
AiActiveCard gDuel_aActiveCards[80];
s32 gDuel_adwCardStats[900];
static int passes, guard_setting = 1, optional_setting = 1;

static void set_turn_draws(ExtraHandTurnDraws fn) { registered = fn; }
static void bonus_hand(int draws) { bonus_calls++; bonus_draws += draws; }
static int queued(int side) { (void)side; return 0; }
static ExtraHandOptionalPlay registered_optional;
static void set_optional_play(ExtraHandOptionalPlay fn) { registered_optional = fn; }
static void pass_hand(void) { passes++; }
/* The hand past the fifth card, as Extra hand plays keeps it: ids and marks
 * of slots 6 to 10. */
static int extra_ids[5], extra_marks[5], slot_count = HAND_SIZE;
static ExtraHandLimits registered_limits;
static void set_hand_limits(ExtraHandLimits fn) { registered_limits = fn; }
static int hand_slots_api(void) { return slot_count; }
static int hand_card_api(int slot)
{
    if (slot >= HAND_SIZE) return extra_ids[slot - HAND_SIZE];
    return D_800E9FF0[D_8009B1D5].hand[slot] >= 0 ? D_801A7AD8[slot].card_id : 0;
}
static int hand_mark_api(int slot) { return slot >= HAND_SIZE ? extra_marks[slot - HAND_SIZE] : D_800EA030[slot].active_09; }
static const ExtraHandApi fake_api = {EXTRA_HAND_API_VERSION, set_turn_draws, bonus_hand, queued, set_optional_play,
                                      pass_hand, set_hand_limits, hand_slots_api, hand_card_api, hand_mark_api};
static int limit_setting = 6, draw_one_setting = 1;

static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{
    (void)h;
    if (!strcmp(key, "fusion_guard")) return guard_setting;
    if (!strcmp(key, "optional_play")) return optional_setting;
    if (!strcmp(key, "hand_limit")) return limit_setting;
    if (!strcmp(key, "draw_one")) return draw_one_setting;
    return fallback;
}
static int fake_applied(const MemoriesModHost *h) { (void)h; return applied_flag; }
static int fake_card_id(const MemoriesModHost *h, const char *identity)
{
    (void)h; return !strcmp(identity, POT_IDENTITY) ? POT : 0;
}
static void *fake_find(const MemoriesModHost *h, const char *name)
{
    (void)h; return !strcmp(name, "extra-hand-plays:api") ? (void *)&fake_api : 0;
}
static int fake_subscribe(const MemoriesModHost *h, unsigned event, int priority, MemoriesModCallback fn)
{
    (void)h; (void)priority; assert(event == MEMORIES_EVENT_EFFECT); subscribed = fn; return 1;
}
static int fake_hook(const MemoriesModHost *h, void *fn, void *replacement, void **previous)
{
    (void)h; assert(replacement); *previous = fn; return 1;
}
static void fake_log(const MemoriesModHost *h, const char *format, ...) { (void)h; (void)format; }

/* The player's hand: card ids by slot, marks in pick order (0 unmarked). */
static void deal(const int *ids, const int *marks)
{
    DuelSelectionRecord *side = (DuelSelectionRecord *)D_800E9F10;
    int i, marked = 0;
    for (i = 0; i < HAND_SIZE; i++) {
        D_800E9FF0[0].hand[i] = ids[i] ? (s8)i : -1;
        D_801A7AD8[i].card_id = (s16)ids[i];
        D_800EA030[i].active_09 = (u8)marks[i];
        marked += marks[i] != 0;
    }
    side->field_15 = (u8)marked;
}
static int confirm(void)
{
    gInput_wPad1Pressed = PAD_BUTTON_CROSS;
    seen_pressed = -1;
    sounds = 0;
    hand();
    assert(gInput_wPad1Pressed == PAD_BUTTON_CROSS);
    return seen_pressed == PAD_BUTTON_CROSS;
}

int main(void)
{
    MemoriesModHost h = {0};
    MemoriesMod mod = {0};
    MemoriesModEvent event;
    h.api = 4; h.setting = fake_setting; h.applied = fake_applied; h.card_id = fake_card_id;
    h.find = fake_find; h.subscribe = fake_subscribe; h.hook = fake_hook; h.log = fake_log;
    assert(MemoriesModInit(&h, &mod) && registered && subscribed);
    D_8009B1B4 = &field_cursor;              /* on the opponent's row: nothing to check */

    /* Playing a card is optional while the mod is applied. */
    assert(registered_optional && registered_optional());
    applied_flag = 0;
    assert(!registered_optional());
    applied_flag = 1;

    /* One card a turn after the opening; the disc's when removed. The
     * opening is a side's draw with nothing drawn yet: the side that goes
     * first has its five dealt before the duel's first turn, which has no
     * draw and is not counted, so its next turn is still counted as 1; the
     * other side's opening is its five and its turn's card. */
    assert(registered(1, 1, 10) == 6);
    D_800E9FF0[0].deck_draw_cursor = 5;
    assert(registered(0, 1, 3) == 1);
    D_800E9FF0[0].deck_draw_cursor = 6;
    D_800E9FF0[1].deck_draw_cursor = 5;
    assert(registered(0, 2, 1) == 1 && registered(1, 9, 3) == 1);
    applied_flag = 0;
    assert(registered(0, 4, 2) == -1);
    applied_flag = 1;

    /* Up to ten in hand, six kept at the end of a turn; the disc's five and
     * no limit without one card a turn. */
    {
        int max = HAND_SIZE, keep = 0;
        assert(registered_limits);
        registered_limits(0, &max, &keep);
        assert(max == 10 && keep == 6);
        limit_setting = 0;
        registered_limits(1, &max, &keep);
        assert(max == 10 && keep == 0);
        limit_setting = 6;
        draw_one_setting = 0;
        max = HAND_SIZE; keep = 0;
        registered_limits(0, &max, &keep);
        assert(max == HAND_SIZE && keep == 0 && registered(0, 2, 1) == -1);
        draw_one_setting = 1;
    }

    /* The player's confirm of marked cards. */
    D_8009B1D5 = 0;
    D_8009B360[0] = -1;
    gDuel_wSceneStateFlags = STATE_HAND | STATE_STARTED;
    D_8009B174 = HAND_IDLE | HAND_STEP_DONE;
    {
        static const int fuse_twice[5] = {1, 2, 4, 9, 0}, fuse_marks[5] = {1, 2, 3, 0, 0};
        static const int equip_after[5] = {1, 2, 6, 0, 0}, equip_marks[5] = {1, 2, 3, 0, 0};
        static const int equip_first[5] = {6, 7, 0, 0, 0}, equip_first_marks[5] = {1, 2, 0, 0, 0};
        static const int broken[5] = {1, 2, 9, 4, 0}, broken_marks[5] = {1, 2, 3, 4, 0};
        static const int order[5] = {2, 4, 1, 0, 0}, order_marks[5] = {1, 2, 3, 0, 0};
        static const int single[5] = {9, 1, 2, 0, 0}, single_marks[5] = {0, 1, 0, 0, 0};
        deal(fuse_twice, fuse_marks);                   /* 1+2 -> 3, 3+4 -> 5 */
        assert(confirm() && sounds == 0);
        deal(equip_after, equip_marks);                 /* the fusion, then an equip on it */
        assert(confirm());
        deal(equip_first, equip_first_marks);           /* an equip onto the monster after it */
        assert(confirm());
        deal(broken, broken_marks);                     /* 3 + 9 would throw one away */
        assert(!confirm() && sounds == 1);
        deal(order, order_marks);                       /* 2 + 4 do not fuse, whatever comes after */
        assert(!confirm());
        deal(single, single_marks);                     /* one marked card is not a chain */
        assert(confirm());
        /* Past the fifth card: marks on slots 6 to 10 count in their order. */
        {
            static const int five[5] = {9, 9, 9, 9, 9}, unmarked[5] = {0, 0, 0, 0, 0};
            deal(five, unmarked);
            slot_count = 8;
            extra_ids[0] = 2; extra_ids[1] = 1; extra_ids[2] = 4;
            extra_marks[1] = 1; extra_marks[0] = 2; extra_marks[2] = 3;  /* 1 + 2 -> 3, 3 + 4 -> 5 */
            ((DuelSelectionRecord *)D_800E9F10)->field_15 = 3;
            assert(confirm() && sounds == 0);
            extra_marks[2] = 0;
            D_800EA030[4].active_09 = 3;                                 /* 3 + 9 throws one away */
            assert(!confirm() && sounds == 1);
            memset(extra_ids, 0, sizeof extra_ids);
            memset(extra_marks, 0, sizeof extra_marks);
            slot_count = HAND_SIZE;
        }
        /* Only the player's idle hand is guarded. */
        deal(broken, broken_marks);
        D_8009B360[0] = 1;
        assert(confirm());
        D_8009B360[0] = -1;
        D_8009B174 = 4 | HAND_STEP_DONE;
        assert(confirm());
        D_8009B174 = HAND_IDLE | HAND_STEP_DONE;
    }

    /* Putting cards onto one of the player's monsters (a full monster row,
     * or by choice): they must fuse with it or equip it. */
    {
        static const int hand_ids[5] = {2, 4, 9, 6, 0}, none[5] = {0, 0, 0, 0, 0};
        static const int two[5] = {0, 1, 0, 0, 0};
        DuelSelectionRecord *side = (DuelSelectionRecord *)D_800E9F10;
        D_8009B1B4 = &field_cursor;
        D_8009B174 = HAND_PLACING | HAND_STEP_DONE;
        D_801A7AD8[7].flags = DUEL_CARD_FLAG_OCCUPIED;
        D_801A7AD8[7].card_id = 1;               /* monster 1 at row 2, column 2 */
        field_cursor.row = MONSTER_ROW;
        field_cursor.col = 2;
        deal(hand_ids, none);
        side->field_0E = 0;                      /* 2 onto 1: they fuse */
        assert(confirm());
        side->field_0E = 3;                      /* 6 equips onto 1 */
        assert(confirm());
        side->field_0E = 2;                      /* 9 would take 1's place */
        assert(!confirm() && sounds == 1);
        deal(hand_ids, two);                     /* 1 + 4 do not fuse */
        side->field_15 = 2;
        D_800EA030[1].active_09 = 1;
        D_800EA030[0].active_09 = 2;             /* marked 4, then 2 */
        assert(!confirm());
        D_800EA030[0].active_09 = 1;
        D_800EA030[1].active_09 = 2;             /* 1 + 2 -> 3, 3 + 4 -> 5 */
        assert(confirm());
        side->field_15 = 0;
        side->field_0E = 2;
        field_cursor.col = 3;                    /* an empty zone */
        assert(confirm());
        field_cursor.col = 2;
        field_cursor.row = 3;                    /* the spell row is not checked */
        assert(confirm());
        field_cursor.row = MONSTER_ROW;
        D_8009B174 = HAND_PLACING | HAND_STEP_DONE | HAND_PLACING_MOVING;
        assert(confirm());                       /* the cursor is still moving */
        D_8009B174 = HAND_IDLE | HAND_STEP_DONE;
    }

    /* An equip used from the field (the field state's step 6, the monster it
     * goes onto): it must equip that monster, or fuse with it -- the disc
     * throws it away otherwise. The field cursor is on the equip, in the
     * spell row; the target cursor on one of the player's monsters. */
    {
        DuelSelectionRecord *spell = (DuelSelectionRecord *)D_800E9F10 + 2;
        DuelSelectionRecord *target = (DuelSelectionRecord *)D_800E9F10 + 3;
        int i;
        for (i = 5; i < 15; i++) D_801A7AD8[i].flags = 0;
        gDuel_adwCardStats[8 - 1] = CARD_TYPE_EQUIP << CARD_STAT_TYPE_SHIFT;
        D_801A7AD8[11].flags = DUEL_CARD_FLAG_OCCUPIED;   /* equip 8 at row 3, column 1 */
        D_801A7AD8[11].card_id = 8;
        D_801A7AD8[7].flags = DUEL_CARD_FLAG_OCCUPIED;    /* monster 1 at row 2, column 2 */
        D_801A7AD8[7].card_id = 1;
        D_801A7AD8[8].flags = DUEL_CARD_FLAG_OCCUPIED;    /* monster 9 at column 3 */
        D_801A7AD8[8].card_id = 9;
        spell->row = 3; spell->col = 1;
        target->row = MONSTER_ROW; target->col = 2;
        D_8009B1D5 = 0;
        D_8009B360[0] = -1;
        gDuel_wSceneStateFlags = STATE_FIELD | STATE_STARTED;
        D_8009B174 = FIELD_TARGETING;
        gInput_wPad1Pressed = PAD_BUTTON_CROSS;
        sounds = 0;
        field();
        assert(seen_pressed == PAD_BUTTON_CROSS && sounds == 0);            /* 8 equips 1 */
        target->col = 3;
        field();
        assert(seen_pressed == 0 && sounds == 1 && gInput_wPad1Pressed == PAD_BUTTON_CROSS);  /* not 9 */
        guard_setting = 0;
        field();
        assert(seen_pressed == PAD_BUTTON_CROSS);
        guard_setting = 1;
        D_8009B360[0] = 1;                                                  /* the CPU's own path */
        field();
        assert(seen_pressed == PAD_BUTTON_CROSS);
        D_8009B360[0] = -1;
        D_801A7AD8[11].card_id = 2;                                         /* not an equip */
        field();
        assert(seen_pressed == PAD_BUTTON_CROSS);
        D_801A7AD8[11].card_id = 8;
        D_8009B174 = FIELD_TARGETING | FIELD_TARGET_LEAVING;               /* going back */
        field();
        assert(seen_pressed == PAD_BUTTON_CROSS);
        D_8009B174 = HAND_IDLE | HAND_STEP_DONE;
        gDuel_wSceneStateFlags = STATE_HAND | STATE_STARTED;
        for (i = 5; i < 15; i++) D_801A7AD8[i].flags = 0;
    }

    /* Pot of Greed: the use's first handler asks for a bonus hand with two
     * draws; both handlers are taken from the retail dispatcher; other cards
     * and the per-frame update are left alone. */
    memset(&event, 0, sizeof event);
    event.type = MEMORIES_EVENT_EFFECT; event.phase = MEMORIES_BEFORE; event.a = POT;
    subscribed(&event);
    assert(event.handled && bonus_calls == 1 && bonus_draws == 2);
    memset(&event, 0, sizeof event);
    event.phase = MEMORIES_BEFORE; event.a = POT; event.b = 1;
    subscribed(&event);
    assert(event.handled && bonus_calls == 1);
    memset(&event, 0, sizeof event);
    event.phase = MEMORIES_BEFORE; event.a = POT; event.c = 1;
    subscribed(&event);
    assert(!event.handled);
    memset(&event, 0, sizeof event);
    event.phase = MEMORIES_BEFORE; event.a = 343;
    subscribed(&event);
    assert(!event.handled && bonus_calls == 1);

    /* The CPU uses a Pot of Greed in hand first: its packed hand slot. */
    {
        static const int ids[5] = {1, 0, 7, POT, 2}, none[5] = {0, 0, 0, 0, 0};
        static const int without[5] = {1, 7, 2, 0, 0};
        D_8009B1D5 = 0;
        gAiScript_State.script_base = D_801A8000;
        gDuel_wSceneStateFlags = STATE_HAND | STATE_STARTED;
        deal(ids, none);
        assert(run() == 1 && D_800EAE88[0] == AI_HAND_SLOT_FIRST + 2 && D_800EAE88[6] == 0);
        deal(without, none);
        assert(run() == 1 && D_800EAE88[0] == 13 && D_800EAE88[6] == 2);
        deal(ids, none);
        gAiScript_State.script_base = D_801A9800;     /* the field script keeps its own */
        assert(run() == 1 && D_800EAE88[0] == 13);
        /* Past the fifth card: numbered after the five in the snapshot. */
        {
            static const int full[5] = {1, 7, 2, 7, 1};
            deal(full, none);
            gAiScript_State.script_base = D_801A8000;
            extra_ids[0] = 7; extra_ids[1] = POT;
            assert(run() == 1 && D_800EAE88[0] == AI_HAND_SLOT_FIRST + 6);
            memset(extra_ids, 0, sizeof extra_ids);
        }
    }

    /* The CPU is held to the same rule as the player: cards it puts onto
     * one of its monsters must fuse with it or equip it, and a chain from
     * its hand must not throw a card away. Its zone moves to one that
     * keeps every card; with none, it plays no card (playing is optional). */
    {
        static const int field_ids[5] = {7, 7, 1, 7, 7};
        static const int no_pot[5] = {1, 7, 2, 0, 0}, none[5] = {0, 0, 0, 0, 0};
        int i;
        D_8009B1D5 = 0;
        deal(no_pot, none);                                /* no Pot of Greed to use first */
        gAiScript_State.script_base = D_801A8000;
        gDuel_wSceneStateFlags = STATE_HAND | STATE_STARTED;
        gDuel_adwCardStats[6 - 1] = CARD_TYPE_EQUIP << CARD_STAT_TYPE_SHIFT;
        gDuel_adwCardStats[POT - 1] = CARD_TYPE_MAGIC << CARD_STAT_TYPE_SHIFT;
        for (i = 5; i < 10; i++) D_801A7AD8[i].flags = 0;
        D_801A7AD8[7].flags = DUEL_CARD_FLAG_OCCUPIED;     /* monster 1, column 2 */
        D_801A7AD8[7].card_id = 1;
        gDuel_aActiveCards[11].card_id = 9;
        gDuel_aActiveCards[12].card_id = 2;
        gDuel_aActiveCards[13].card_id = 6;
        gDuel_aActiveCards[16].card_id = 4;              /* a deck card the script may pick */
        memset(ai_choice, 0, sizeof ai_choice);
        ai_choice[0] = 11; ai_choice[6] = 3; ai_choice[8] = 1;
        assert(run() == 1 && D_800EAE88[0] == 11 && D_800EAE88[6] == 1);   /* 9 onto 1: to column 0 */
        ai_choice[0] = 12;
        assert(run() == 1 && D_800EAE88[6] == 3);                          /* 2 onto 1 fuses: kept */
        ai_choice[0] = 12; ai_choice[1] = 16;
        assert(run() == 1 && D_800EAE88[1] == 16 && D_800EAE88[6] == 3);   /* 1+2 -> 3, 3+4 -> 5 */
        ai_choice[0] = 12; ai_choice[1] = 11; ai_choice[6] = 1;
        assert(run() == 1 && D_800EAE88[0] == 12 && D_800EAE88[1] == 0);   /* 2 + 9 break: 2 alone */
        ai_choice[0] = 13; ai_choice[1] = 0; ai_choice[6] = 1;
        assert(run() == 1 && D_800EAE88[6] == 3);                          /* an equip needs a monster */
        for (i = 0; i < 5; i++) {                                          /* a full monster row */
            D_801A7AD8[5 + i].flags = DUEL_CARD_FLAG_OCCUPIED;
            D_801A7AD8[5 + i].card_id = (s16)field_ids[i];
        }
        ai_choice[0] = 12; ai_choice[6] = 1;
        assert(run() == 1 && D_800EAE88[6] == 3);                          /* onto the 1 it fuses with */
        ai_choice[0] = 11;
        passes = 0;
        assert(run() == 1 && passes == 1 && D_800EAE88[0] == 0);           /* 9 fits nowhere: no card */
        optional_setting = 0;                                              /* a play is required */
        assert(run() == 1 && passes == 1 && D_800EAE88[0] == 11 && D_800EAE88[6] == 1);
        optional_setting = 1;
        guard_setting = 0;
        assert(run() == 1 && passes == 1 && D_800EAE88[0] == 11 && D_800EAE88[6] == 1);
        guard_setting = 1;
        ai_choice[0] = AI_ENEMY_SLOT_FIRST + 4;                            /* not its card: left alone */
        assert(run() == 1 && D_800EAE88[0] == AI_ENEMY_SLOT_FIRST + 4 && D_800EAE88[6] == 1);
        for (i = 5; i < 10; i++) D_801A7AD8[i].flags = 0;
        memset(ai_choice, 0, sizeof ai_choice);
        ai_choice[0] = 13; ai_choice[6] = 2;
    }

    /* For testing: trunk and deck together keep three Pot of Greed. */
    mod_frame = mod.frame;
    assert(mod_frame);
    frames(TEST_CHECK_FRAMES);
    assert(pot_trunk == 3);
    gDuel_awPlayerDeck[5] = POT;                  /* one went into the deck */
    pot_trunk = 1;
    frames(TEST_CHECK_FRAMES);
    assert(pot_trunk == 2);
    pot_trunk = 0;
    chest_on_screen = 1;                          /* Build Deck has the trunk */
    frames(TEST_CHECK_FRAMES);
    assert(pot_trunk == 0);
    chest_on_screen = 0;
    save_loaded = 0;
    frames(TEST_CHECK_FRAMES);
    assert(pot_trunk == 0);

    printf("Modern rules: optional play, turn draws, fusion guard on the hand, the field and the CPU, Pot of Greed and the CPU's use of it, test copies passed\n");
    return 0;
}
