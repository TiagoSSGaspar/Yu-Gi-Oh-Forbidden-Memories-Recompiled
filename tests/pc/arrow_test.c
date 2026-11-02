/* Compile for i386: Living Arrow (Duelist Kingdom rules, arrow.c), against
 * stand-ins for the game, the host and Extra hand plays. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/arrow.c"

/* The CPU's level (cpu.c, tested in multiply_test.c). */
static int cpu_level = CPU_BOSS;
int Cpu_Level(void) { return cpu_level; }

#define POLY 800
#define ARROW 801
#define DRAGON 1     /* Blue-Eyes Ultimate, as it were: 4500 */
#define MAMMOTH 2    /* Mammoth Graveyard, a Zombie: 1200 */
#define ROBOT 3      /* a Machine */
#define GOLEM 4      /* a Rock */
#define WARRIOR 5    /* the CPU's own: 1000 */
#define BIG 6        /* the CPU's own: 5000 */
#define GRAVEYARD 59 /* the real Mammoth Graveyard: a Dinosaur on the disc */
ModernRulesState gModernRules_State;
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
s32 gDuel_adwCardStats[900];
u16 gDuel_wSceneStateFlags, gInput_wPad1Pressed;
u8 D_8009B174, D_8009B1D5, gDuel_bEffectState, gDuel_bQuitDialogState;
s8 D_8009B360[2] = {-1, 0};
static DuelDeckCardRecord deck_cards[2][40];
static DisplayObject objects[DUEL_CARD_RECORD_COUNT];
static int sounds, refused, drawn_color;
void SD_SEPlayFull(u32 id) { sounds++; refused += id == REFUSED_SOUND; }
s32 Duel_CalcCardStats(DuelCardRecord *card)
{
    s32 stats = gDuel_adwCardStats[card->card_id - 1];
    int attack = (stats & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE + card->stat_modifier;
    return attack < 0 ? 0 : attack;
}
int Cards_NameUtf8(int id, char *out, size_t size) { snprintf(out, size, "card %d", id); return 1; }
void func_80015EF4(void *record, POLY_GT4 *prim, POLY_FT4 *sprite, s32 *color)
{
    (void)prim; (void)sprite; (void)color;
    drawn_color = (int)(((DisplayObject *)((DuelCardRecord *)record)->object)->field_0C & 0xFFFFFF);
}

static void stats(int id, int attack, int type)
{
    gDuel_adwCardStats[id - 1] = (attack / 10) | (type << CARD_STAT_TYPE_SHIFT);
}

/* The hand: cards and the player's marks, by hand card. */
static int hand[8], marks[8], taken[3], take_count;
static int hand_slots(void) { return 8; }
static int hand_card(int slot) { return hand[slot]; }
static int hand_mark(int slot) { return marks[slot]; }
static int menu_open(void) { return 0; }
static int cursor;
static int hand_cursor(void) { return cursor; }
static void take_hand(const int *cards, int count)
{
    int i;
    take_count = count;
    for (i = 0; i < count; i++) {
        taken[i] = cards[i];
        hand[cards[i]] = 0;
        marks[cards[i]] = 0;
    }
}
static ExtraHandApi fake_ehp;

static int arrow_setting = 1;
static void *hooked;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "living_arrow") ? arrow_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_card_id(const MemoriesModHost *h, const char *identity)
{
    (void)h;
    return !strcmp(identity, "modern-rules:polymerization:1") ? POLY :
           !strcmp(identity, "modern-rules:living_arrow:1") ? ARROW : 0;
}
static int fake_hook(const MemoriesModHost *h, void *function, void *replacement, void **original)
{ (void)h; *original = function; hooked = replacement; return 1; }
static const char *fake_text(const MemoriesModHost *h, const char *english) { (void)h; return english; }
static uint64_t clock_us;
static uint64_t fake_now(const MemoriesModHost *h) { (void)h; return clock_us; }
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static void put(int r, int id, int deck, int face_down)
{
    int side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    deck_cards[side][deck].id = (s16)id;
    deck_cards[side][deck].deck_index = (s8)deck;
    memset(&D_801A7AD8[r], 0, sizeof D_801A7AD8[r]);
    D_801A7AD8[r].card_id = (s16)id;
    D_801A7AD8[r].flags = DUEL_CARD_FLAG_OCCUPIED | (face_down ? DUEL_CARD_FLAG_FACE_DOWN : 0);
    D_801A7AD8[r].data = &deck_cards[side][deck];
    D_801A7AD8[r].object = &objects[r];
    objects[r].field_0C = 0x808080;
}
static void empty(int r) { D_801A7AD8[r].flags = 0; }
static void player_hand(void)
{
    memset(hand, 0, sizeof hand);
    memset(marks, 0, sizeof marks);
    hand[0] = MAMMOTH;
    hand[1] = WARRIOR;
    hand[2] = ARROW;
    hand[3] = POLY;
    marks[3] = 1;
    marks[2] = 2;
    marks[0] = 3;
}
static int press(u16 buttons)
{
    gInput_wPad1Pressed = buttons;
    return Arrow_Hand();
}

int main(void)
{
    int r;
    stats(DRAGON, 4500, CARD_TYPE_DRAGON);
    stats(MAMMOTH, 1200, CARD_TYPE_ZOMBIE);
    stats(ROBOT, 2000, CARD_TYPE_MACHINE);
    stats(GOLEM, 2500, CARD_TYPE_ROCK);
    stats(WARRIOR, 1000, CARD_TYPE_WARRIOR);
    stats(BIG, 5000, CARD_TYPE_WARRIOR);
    stats(GRAVEYARD, 1200, CARD_TYPE_DINOSAUR);
    stats(POLY, 0, CARD_TYPE_MAGIC);
    stats(ARROW, 0, CARD_TYPE_MAGIC);
    fake_ehp.version = 8;
    fake_ehp.hand_slots = hand_slots;
    fake_ehp.hand_card = hand_card;
    fake_ehp.hand_mark = hand_mark;
    fake_ehp.menu_open = menu_open;
    fake_ehp.hand_cursor = hand_cursor;
    fake_ehp.take_hand = take_hand;
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.card_id = fake_card_id;
    fake_host.hook = fake_hook;
    fake_host.text = fake_text;
    fake_host.now_us = fake_now;
    assert(Arrow_Install(&fake_host, &fake_mod, &fake_ehp) && hooked == (void *)draw_card);
    assert(living(DRAGON) && living(WARRIOR) && !living(MAMMOTH) && !living(ROBOT) && !living(GOLEM) &&
           !living(POLY) && zombie(GRAVEYARD) && !living(GRAVEYARD));

    /* The player's hand, idle; the CPU's field: the dragon face up, a
     * Machine, a Rock, and a face-down dragon. */
    gDuel_wSceneStateFlags = STATE_HAND | STATE_STARTED;
    D_8009B174 = HAND_IDLE | HAND_STEP_DONE;
    D_8009B1D5 = 0;
    put(MONSTER_RECORD(1, 0), ROBOT, 1, 0);
    put(MONSTER_RECORD(1, 1), DRAGON, 2, 0);
    put(MONSTER_RECORD(1, 2), GOLEM, 3, 0);
    put(MONSTER_RECORD(1, 3), DRAGON, 4, 1);
    player_hand();

    /* Two of the three marked: not the combo, and Polymerization and Living
     * Arrow are not played without it. Polymerization alone under the
     * cursor: the same. Another card alone: played. */
    marks[0] = 0;
    refused = 0;
    assert(press(PAD_BUTTON_CROSS) && !A.aiming && refused == 1);
    memset(marks, 0, sizeof marks);
    cursor = 3;
    assert(press(PAD_BUTTON_CROSS) && refused == 2);
    cursor = 1;
    assert(!press(PAD_BUTTON_CROSS));
    player_hand();
    /* The three: the aim, on the only living face-up one. */
    assert(press(PAD_BUTTON_CROSS) && A.aiming && A.target == MONSTER_RECORD(1, 1));
    /* Left and right stay on it; no button reaches the hand meanwhile. */
    assert(press(PAD_DIRECTION_RIGHT) && A.target == MONSTER_RECORD(1, 1));
    assert(press(PAD_DIRECTION_LEFT) && A.target == MONSTER_RECORD(1, 1));
    assert(press(0) && A.aiming);
    /* The aimed card is drawn lit. */
    draw_card(&D_801A7AD8[MONSTER_RECORD(1, 1)], 0, 0, 0);
    assert(drawn_color == 0x30C0C0 && objects[MONSTER_RECORD(1, 1)].field_0C == 0x808080);
    /* Circle: no aim, nothing taken. */
    refused = 0;
    assert(press(PAD_BUTTON_CIRCLE) && !A.aiming && refused == 1 && take_count == 0);
    /* Again, and cross: the dragon infected for the Mammoth's 1200; the
     * three cards out of the hand. */
    assert(press(PAD_BUTTON_CROSS) && A.aiming);
    assert(press(PAD_BUTTON_CROSS) && !A.aiming);
    assert(take_count == 3 && hand[0] == 0 && hand[2] == 0 && hand[3] == 0 && hand[1] == WARRIOR);
    assert(A.card[ZONE_OF(MONSTER_RECORD(1, 1))] == DRAGON && A.loss[ZONE_OF(MONSTER_RECORD(1, 1))] == 1200 &&
           A.owner[ZONE_OF(MONSTER_RECORD(1, 1))] == 0);
    {
        char text[160];
        assert(line(text, sizeof text) && strstr(text, "infected") && strstr(text, "1200"));
    }
    draw_card(&D_801A7AD8[MONSTER_RECORD(1, 1)], 0, 0, 0);
    assert(drawn_color == INFECTED_COLOR);
    draw_card(&D_801A7AD8[MONSTER_RECORD(1, 0)], 0, 0, 0);
    assert(drawn_color == 0x808080);

    /* With 3D Monsters: the Mammoth flies from behind the player's
     * monsters, shrinking, into the dragon's chest, and stays there; the
     * dragon is drawn rotting, more as its ATK goes. */
    {
        Monsters3dExtra extra;
        unsigned char rgb[3], before[3];
        int zone = ZONE_OF(MONSTER_RECORD(1, 1)), first_z, k;
        assert(A.flying == zone && !model_tint(MONSTER_RECORD(1, 1), rgb));
        assert(model_extras(MONSTER_RECORD(1, 1), 0, 95, &extra, 1) == 1 && extra.card == MAMMOTH);
        first_z = extra.z;
        assert(first_z < 0 && extra.share == ONE);
        for (k = 1; k < FLY_FRAMES; k++) model_extras(MONSTER_RECORD(1, 1), 0, 95, &extra, 1);
        assert(extra.share < ONE && extra.z > first_z);
        assert(model_extras(MONSTER_RECORD(1, 1), 0, 95, &extra, 1) == 1 && extra.share == INSIDE &&
               extra.y == CHEST && extra.z == 95 - FRONT);
        assert(model_extras(MONSTER_RECORD(1, 0), 0, 95, &extra, 1) == 0);
        A.pulse = -1;
        assert(model_tint(MONSTER_RECORD(1, 1), before) && before[0] < 128 && !model_tint(MONSTER_RECORD(1, 0), rgb));
        D_801A7AD8[MONSTER_RECORD(1, 1)].stat_modifier = -3000;
        assert(model_tint(MONSTER_RECORD(1, 1), rgb) && rgb[0] < before[0]);
        D_801A7AD8[MONSTER_RECORD(1, 1)].stat_modifier = 0;
    }

    /* The player's turns end (the CPU's begin): 4500, 3300, 2100, 900, 0.
     * The player's own turn beginning takes nothing. */
    Arrow_TurnStarts(0);
    assert(attack_of(MONSTER_RECORD(1, 1)) == 4500);
    Arrow_TurnStarts(1);
    assert(attack_of(MONSTER_RECORD(1, 1)) == 3300);
    Arrow_TurnStarts(1);
    Arrow_TurnStarts(1);
    assert(attack_of(MONSTER_RECORD(1, 1)) == 900);
    Arrow_TurnStarts(1);
    assert(attack_of(MONSTER_RECORD(1, 1)) == 0 && D_801A7AD8[MONSTER_RECORD(1, 1)].stat_modifier == -4500);
    {
        char text[160];
        assert(line(text, sizeof text) && strstr(text, "rots") && strstr(text, "-900"));
    }
    /* The line goes after a while. */
    clock_us += SAID_US;
    {
        char text[160];
        assert(!line(text, sizeof text));
    }
    /* Fused into another monster: no longer the one infected, no line about
     * it, no Mammoth in its chest, no rot. */
    Arrow_TurnStarts(1);
    put(MONSTER_RECORD(1, 1), WARRIOR, 9, 0);
    {
        char text[160];
        Monsters3dExtra extra;
        unsigned char rgb[3];
        assert(!line(text, sizeof text) && !model_extras(MONSTER_RECORD(1, 1), 0, 95, &extra, 1) &&
               !model_tint(MONSTER_RECORD(1, 1), rgb));
        Arrow_TurnStarts(1);
        assert(!A.card[ZONE_OF(MONSTER_RECORD(1, 1))] && attack_of(MONSTER_RECORD(1, 1)) == 1000);
    }
    /* It leaves the field: the infection goes with it. */
    empty(MONSTER_RECORD(1, 1));
    Arrow_TurnStarts(1);
    assert(!A.card[ZONE_OF(MONSTER_RECORD(1, 1))]);

    /* No living monster face up: refused, no aim. */
    player_hand();
    take_count = 0;
    refused = 0;
    assert(press(PAD_BUTTON_CROSS) && !A.aiming && refused == 1 && take_count == 0);

    /* The CPU, with the three in hand: on the player's strongest living
     * monster, stronger than all of its own; not when one of its own is
     * stronger. */
    D_8009B1D5 = 1;
    empty(MONSTER_RECORD(1, 3));
    memset(hand, 0, sizeof hand);
    memset(marks, 0, sizeof marks);
    hand[1] = POLY;
    hand[2] = MAMMOTH;
    hand[4] = ARROW;
    put(MONSTER_RECORD(0, 0), WARRIOR, 1, 0);
    put(MONSTER_RECORD(0, 2), DRAGON, 2, 0);
    put(MONSTER_RECORD(0, 3), ROBOT, 3, 0);
    put(MONSTER_RECORD(1, 4), BIG, 5, 0);
    cpu_level = CPU_MIDDLE;   /* a middle one: only when behind */
    assert(!Arrow_Cpu());
    cpu_level = CPU_BEGINNER;
    empty(MONSTER_RECORD(1, 4));
    put(MONSTER_RECORD(1, 4), WARRIOR, 5, 0);
    assert(!Arrow_Cpu());   /* a beginner: never */
    cpu_level = CPU_MIDDLE;
    empty(MONSTER_RECORD(1, 4));
    put(MONSTER_RECORD(1, 4), WARRIOR, 5, 0);
    assert(Arrow_Cpu() && take_count == 3 && taken[0] == 1 && taken[1] == 4 && taken[2] == 2);
    r = MONSTER_RECORD(0, 2);
    assert(A.card[ZONE_OF(r)] == DRAGON && A.owner[ZONE_OF(r)] == 1);
    Arrow_TurnStarts(1);
    assert(attack_of(r) == 4500);
    Arrow_TurnStarts(0);
    assert(attack_of(r) == 3300);

    /* Off: nothing. */
    arrow_setting = 0;
    D_8009B1D5 = 0;
    player_hand();
    put(MONSTER_RECORD(1, 1), DRAGON, 2, 0);
    assert(!press(PAD_BUTTON_CROSS) && !Arrow_Cpu());

    printf("Living Arrow: living creatures only, not played alone, the combo, the aim, cancel, the infection, rotting to 0, "
           "the line over, fused into another, gone with its monster, no target, the CPU, off passed\n");
    return 0;
}
