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
static u8 objects[DUEL_CARD_RECORD_COUNT][0x70];
static int sounds, lights, remakes, applied;
static DuelEffectRequest effect;
void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
DuelEffectRequest *DuelEffect_CreateRequest(s32 id) { (void)id; lights++; return &effect; }
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; applied++; }
/* The battle's stats: ATK and DEF straight from the record. */
DisplayObject *D_800E9EF0[7];
s16 gDuel_awSavedDefenseModifier[2];
static s32 base_def(DuelCardRecord *c) { return c->card_id == KURIBOH ? 200 : 1000; }
s32 Duel_CalcBattleAttack(DuelCardRecord *card, DuelCardRecord *opponent)
{ (void)opponent; return (card->card_id == KURIBOH ? 300 : 4500) + card->stat_modifier; }
s32 Duel_CalcBattleDefense(DuelCardRecord *card, DuelCardRecord *opponent)
{ (void)opponent; return base_def(card) + card->defense_modifier; }
s32 Duel_GetCardEffectVariant(CardObject *object) { (void)object; return 0; }
static DisplayObject attacker_object, attacked_object;
/* The game's check before a position change: occupied, not used. */
int DuelCard_CanActThisTurn(DuelCardRecord *card)
{
    return (card->flags & DUEL_CARD_FLAG_OCCUPIED) && !(card->flags & DUEL_CARD_FLAG_USED_THIS_TURN);
}
/* The game's remake of a record from its deck card: occupied, face down. */
void func_80024D34(s32 record, s32 deck_index)
{
    DuelCardRecord *card = &D_801A7AD8[record];
    int side = record / DUEL_CARD_SIDE_RECORD_COUNT;
    remakes++;
    card->data = &deck_cards[side][deck_index];
    card->card_id = deck_cards[side][deck_index].id;
    card->flags = DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_USED_THIS_TURN;
    card->stat_modifier = 300;
    card->object = objects[record];
}
static int multiply_setting = 1, identity_asks;
static void *hooked;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "multiply") ? multiply_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_card_id(const MemoriesModHost *h, const char *identity)
{ (void)h; identity_asks++; return !strcmp(identity, "modern-rules:multiply:1") ? MULTIPLY_ID : 0; }
static int fake_hook(const MemoriesModHost *h, void *function, void *replacement, void **original)
{ (void)h; *original = function; hooked = replacement; return 1; }
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
    D_801A7AD8[r].object = objects[r];
}
static void empty(int r) { D_801A7AD8[r].flags = 0; }
static int defending(int r)
{
    return (D_801A7AD8[r].flags & (DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_DEFENSE_POSITION | DUEL_CARD_FLAG_FACE_DOWN)) ==
        (DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_DEFENSE_POSITION);
}

int main(void)
{
    int i;
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.card_id = fake_card_id;
    fake_host.hook = fake_hook;
    assert(Multiply_Install(&fake_host, &fake_mod) && hooked == (void *)can_act);
    assert(Multiply_Is(MULTIPLY_ID) && !Multiply_Is(301) && !Multiply_Is(0));
    Multiply_Is(MULTIPLY_ID);
    assert(identity_asks == 1);   /* asked once, then kept */

    /* The player's Kuriboh in zone 2 (attack), Celtic Guardian in zone 0;
     * Multiply lands on the Kuriboh: light and sound, the Kuriboh to
     * defense. */
    put(MONSTER_RECORD(0, 0), 36, 4);
    put(MONSTER_RECORD(0, 2), KURIBOH, 7);
    put(SPELL_RECORD(0, 2), MULTIPLY_ID, 9);
    Multiply_Landed(MONSTER_RECORD(0, 2));
    assert(lights == 1 && sounds == 1 && defending(MONSTER_RECORD(0, 2)));
    assert(Multiply_Up(0) && !Multiply_Up(1));
    {
        int chain[2] = {KURIBOH, MULTIPLY_ID};
        assert(Multiply_InChain(chain, 2) && !Multiply_InChain(chain, 1));
    }

    /* Attacked and destroyed: born again in its zone, in defense, face up,
     * not used, no modifier; the light again. */
    empty(MONSTER_RECORD(0, 2));
    assert(!Multiply_Hold() && remakes == 1 && lights == 2);
    assert(D_801A7AD8[MONSTER_RECORD(0, 2)].card_id == KURIBOH && defending(MONSTER_RECORD(0, 2)));
    assert(!(D_801A7AD8[MONSTER_RECORD(0, 2)].flags & DUEL_CARD_FLAG_USED_THIS_TURN) &&
           D_801A7AD8[MONSTER_RECORD(0, 2)].stat_modifier == 0);
    /* Nothing more while it is there. */
    Multiply_Hold();
    assert(remakes == 1);
    /* Its position cannot be changed; another monster's can. */
    assert(!can_act(&D_801A7AD8[MONSTER_RECORD(0, 2)]) && can_act(&D_801A7AD8[MONSTER_RECORD(0, 0)]));
    /* Put in attack some other way: back to defense. */
    D_801A7AD8[MONSTER_RECORD(0, 2)].flags &= (u16)~DUEL_CARD_FLAG_DEFENSE_POSITION;
    Multiply_Hold();
    assert(defending(MONSTER_RECORD(0, 2)) && remakes == 1);
    /* Its zone taken meanwhile: a free one. */
    put(MONSTER_RECORD(0, 2), 26, 5);
    Multiply_Hold();
    assert(remakes == 2 && D_801A7AD8[MONSTER_RECORD(0, 1)].card_id == KURIBOH);
    /* The Celtic Guardian destroyed: nothing is born. */
    empty(MONSTER_RECORD(0, 0));
    Multiply_Hold();
    assert(remakes == 2);

    /* With 3D Monsters: five more of the model, spread in front of the
     * zones, toward the other side; none for another card. */
    {
        Monsters3dCopy more[MONSTERS_3D_COPIES];
        assert(model_copies(MONSTER_RECORD(0, 1), -70, -95, more, MONSTERS_3D_COPIES) == CLONES && CLONES == 5);
        for (i = 0; i < CLONES; i++) assert(more[i].z > -95 && more[i].z < 0 && more[i].x >= -140 && more[i].x <= 140);
        assert(more[0].x < more[4].x);
        assert(model_copies(MONSTER_RECORD(0, 1), -70, -95, more, 3) == 3);
        assert(model_copies(MONSTER_RECORD(0, 2), 0, -95, more, MONSTERS_3D_COPIES) == 0);
    }

    /* Multiply destroyed: no wall, nothing born again. */
    empty(SPELL_RECORD(0, 2));
    assert(model_copies(MONSTER_RECORD(0, 1), -70, -95, (Monsters3dCopy[MONSTERS_3D_COPIES]){{0}}, MONSTERS_3D_COPIES) == 0);
    empty(MONSTER_RECORD(0, 1));
    Multiply_Hold();
    assert(remakes == 2);

    /* Two Kuribohs: the wall is the one Multiply went onto. The other is a
     * monster as any other: its position changes, and destroyed it is not
     * born again; the wall's, destroyed, is born again in its own zone. */
    put(SPELL_RECORD(0, 2), MULTIPLY_ID, 9);
    put(MONSTER_RECORD(0, 0), KURIBOH, 11);
    put(MONSTER_RECORD(0, 3), KURIBOH, 12);
    Multiply_Landed(MONSTER_RECORD(0, 3));
    assert(Multiply_Wall(0) == MONSTER_RECORD(0, 3) && !can_act(&D_801A7AD8[MONSTER_RECORD(0, 3)]) &&
           can_act(&D_801A7AD8[MONSTER_RECORD(0, 0)]));
    {
        Monsters3dCopy more[MONSTERS_3D_COPIES];
        assert(model_copies(MONSTER_RECORD(0, 0), 0, -95, more, MONSTERS_3D_COPIES) == 0);
    }
    remakes = 0;
    empty(MONSTER_RECORD(0, 0));
    Multiply_Hold();
    assert(remakes == 0 && Multiply_Wall(0) == MONSTER_RECORD(0, 3));
    empty(MONSTER_RECORD(0, 3));
    Multiply_Hold();
    assert(remakes == 1 && Multiply_Wall(0) == MONSTER_RECORD(0, 3) &&
           D_801A7AD8[MONSTER_RECORD(0, 3)].data == &deck_cards[0][12]);
    empty(SPELL_RECORD(0, 2));
    empty(MONSTER_RECORD(0, 3));
    remakes = 2;

    /* A battle with the wall's Kuriboh: its DEF meets the attack, so
     * neither is destroyed; over, it has its own back, and a clone dies,
     * to stand again some frames later. Another monster's battle: nothing. */
    put(SPELL_RECORD(0, 2), MULTIPLY_ID, 9);
    put(MONSTER_RECORD(0, 3), KURIBOH, 12);
    Multiply_Landed(MONSTER_RECORD(0, 3));
    put(MONSTER_RECORD(1, 0), 380, 1);
    attacker_object.field_6A = MONSTER_RECORD(1, 0);
    attacked_object.field_6A = MONSTER_RECORD(0, 3);
    D_800E9EF0[0] = &attacker_object;
    D_800E9EF0[1] = &attacked_object;
    gDuel_awSavedDefenseModifier[1] = 0;
    models_asked = 1;
    Multiply_BattleBegan();
    assert(Duel_CalcBattleDefense(&D_801A7AD8[MONSTER_RECORD(0, 3)], 0) == 4500 &&
           gDuel_awSavedDefenseModifier[1] == 4300);
    lights = sounds = 0;
    Multiply_BattleOver();
    assert(D_801A7AD8[MONSTER_RECORD(0, 3)].defense_modifier == 0 && K.dead >= 0 && sounds == 1);
    {
        Monsters3dCopy more[MONSTERS_3D_COPIES];
        int dead = K.dead;
        assert(model_copies(MONSTER_RECORD(0, 3), 70, -95, more, MONSTERS_3D_COPIES) == CLONES - 1);
        for (i = 0; i < CLONES - 1; i++) assert(more[i].x != clone_x[dead]);
        for (i = 0; i < CLONE_GONE; i++) model_copies(MONSTER_RECORD(0, 3), 70, -95, more, MONSTERS_3D_COPIES);
        assert(model_copies(MONSTER_RECORD(0, 3), 70, -95, more, MONSTERS_3D_COPIES) == CLONES);
    }
    attacked_object.field_6A = MONSTER_RECORD(0, 0);
    put(MONSTER_RECORD(0, 0), 36, 4);
    Multiply_BattleBegan();
    assert(K.guard == -1 && D_801A7AD8[MONSTER_RECORD(0, 0)].defense_modifier == 0);
    Multiply_BattleOver();
    models_asked = 0;
    empty(MONSTER_RECORD(0, 0));
    empty(MONSTER_RECORD(1, 0));
    empty(SPELL_RECORD(0, 2));
    empty(MONSTER_RECORD(0, 3));

    /* The CPU's own, apart; off: nothing. */
    put(MONSTER_RECORD(1, 4), KURIBOH, 2);
    put(SPELL_RECORD(1, 4), MULTIPLY_ID, 3);
    Multiply_Landed(MONSTER_RECORD(1, 4));
    empty(MONSTER_RECORD(1, 4));
    Multiply_Hold();
    assert(remakes == 3 && D_801A7AD8[MONSTER_RECORD(1, 4)].card_id == KURIBOH);
    multiply_setting = 0;
    empty(MONSTER_RECORD(1, 4));
    Multiply_Hold();
    assert(remakes == 3 && !Multiply_Is(MULTIPLY_ID));

    printf("Kuriboh and Multiply: the identity, the wall up, in defense, held there, two Kuribohs, the attack held by the wall and a clone dying, born again, a free zone, others not, "
           "the wall as models, Multiply gone, sides, off passed\n");
    return 0;
}
