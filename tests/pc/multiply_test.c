/* Compile for i386: Kuriboh and Multiply (Duelist Kingdom rules,
 * multiply.c), against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/multiply.c"

#define MULTIPLY_ID 723
ModernRulesState gModernRules_State;
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
static DuelDeckCardRecord deck_cards[2][40];
static int objects[DUEL_CARD_RECORD_COUNT];
static int sounds, lights, remakes;
static DuelEffectRequest light;
void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
DuelEffectRequest *DuelEffect_CreateRequest(s32 id) { (void)id; lights++; return &light; }
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; }
/* The game's remake of a record from its deck card: occupied, face down. */
void func_80024D34(s32 record, s32 deck_index)
{
    DuelCardRecord *card = &D_801A7AD8[record];
    int side = record / DUEL_CARD_SIDE_RECORD_COUNT;
    remakes++;
    card->data = &deck_cards[side][deck_index];
    card->card_id = deck_cards[side][deck_index].id;
    card->flags = DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_FACE_DOWN;
    card->object = &objects[record];
}

static int multiply_setting = 1, identity_asks;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "multiply") ? multiply_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_card_id(const MemoriesModHost *h, const char *identity)
{ (void)h; identity_asks++; return !strcmp(identity, "modern-rules:multiply:1") ? MULTIPLY_ID : 0; }
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static void put(int r, int id, int deck)
{
    int side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    deck_cards[side][deck].id = (s16)id;
    deck_cards[side][deck].deck_index = (s8)deck;
    memset(&D_801A7AD8[r], 0, sizeof D_801A7AD8[r]);
    D_801A7AD8[r].card_id = (s16)id;
    D_801A7AD8[r].flags = DUEL_CARD_FLAG_OCCUPIED;
    D_801A7AD8[r].data = &deck_cards[side][deck];
}
static void empty(int r) { D_801A7AD8[r].flags = 0; }
static int count(int side, int id)
{
    int col, n = 0;
    for (col = 0; col < 5; col++)
        if ((D_801A7AD8[MONSTER_RECORD(side, col)].flags & DUEL_CARD_FLAG_OCCUPIED) &&
            D_801A7AD8[MONSTER_RECORD(side, col)].card_id == id)
            n++;
    return n;
}

int main(void)
{
    int col;
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.card_id = fake_card_id;
    assert(Multiply_Install(&fake_host, &fake_mod));
    assert(Multiply_Is(MULTIPLY_ID) && !Multiply_Is(301) && !Multiply_Is(0));
    Multiply_Is(MULTIPLY_ID);
    assert(identity_asks == 1);   /* asked once, then kept */

    /* The player's Kuriboh in zone 1, another monster in zone 3: Multiply
     * lands; zones 0, 2 and 4 fill with Kuribohs in defense, face up, with
     * the ritual's light and sound. */
    put(MONSTER_RECORD(0, 1), KURIBOH, 7);
    put(MONSTER_RECORD(0, 3), 26, 8);
    put(SPELL_RECORD(0, 1), MULTIPLY_ID, 9);
    Multiply_Landed(MONSTER_RECORD(0, 1));
    assert(count(0, KURIBOH) == 4 && count(0, 26) == 1 && lights == 1 && sounds == 1);
    for (col = 0; col < 5; col += 2) {
        DuelCardRecord *k = &D_801A7AD8[MONSTER_RECORD(0, col)];
        assert(k->card_id == KURIBOH && (k->flags & DUEL_CARD_FLAG_DEFENSE_POSITION) && !(k->flags & DUEL_CARD_FLAG_FACE_DOWN));
    }
    assert(Multiply_Kuriboh(0) == MONSTER_RECORD(0, 0) && Multiply_Kuriboh(1) == -1);

    /* One destroyed: two come back, but one zone is free: one. */
    empty(MONSTER_RECORD(0, 2));
    assert(!Multiply_Hold() && count(0, KURIBOH) == 4 && lights == 2);
    /* Raigeki: all four and the other monster gone: five free zones, up to
     * eight Kuribohs: five. */
    for (col = 0; col < 5; col++) empty(MONSTER_RECORD(0, col));
    Multiply_Hold();
    assert(count(0, KURIBOH) == 5);
    /* Nothing left: nothing changes. */
    Multiply_Hold();
    assert(count(0, KURIBOH) == 5 && lights == 3);

    /* Multiply destroyed: the Kuribohs stay and come back no more. */
    empty(SPELL_RECORD(0, 1));
    empty(MONSTER_RECORD(0, 0));
    Multiply_Hold();
    assert(count(0, KURIBOH) == 4 && lights == 3);

    /* The CPU's Multiply: its side alone. */
    for (col = 0; col < 5; col++) empty(MONSTER_RECORD(0, col));
    put(MONSTER_RECORD(1, 4), KURIBOH, 2);
    put(SPELL_RECORD(1, 4), MULTIPLY_ID, 3);
    Multiply_Landed(MONSTER_RECORD(1, 4));
    assert(count(1, KURIBOH) == 5 && count(0, KURIBOH) == 0);
    empty(MONSTER_RECORD(1, 0));
    empty(MONSTER_RECORD(1, 1));
    Multiply_Hold();
    assert(count(1, KURIBOH) == 5);

    /* Off: no Multiply, nothing comes back. */
    multiply_setting = 0;
    assert(!Multiply_Is(MULTIPLY_ID));
    empty(MONSTER_RECORD(1, 0));
    Multiply_Hold();
    assert(count(1, KURIBOH) == 4);

    printf("Kuriboh and Multiply: the identity, the wall, two for one, the free zones, Multiply gone, sides, off passed\n");
    return 0;
}
