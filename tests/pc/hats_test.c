/* Compile for i386: Magical Hats (Duelist Kingdom rules, hats.c), against
 * stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/hats.c"

#define HATS 729
#define HAT 730
ModernRulesState gModernRules_State;
u8 D_8009B1D5, D_8009B1B8;
s16 D_8009B22A;
DisplayObject *D_800E9EF0[7];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
DuelDeckCardRecord gDuel_aDeckCardRecords[80];
static DisplayObject attacked;
static int trap_result = 1, moved_from = -1, moved_to = -1, removed;

s32 Duel_SelectAttackTrap(u8 *record) { (void)record; return trap_result; }
int DuelCard_CanActThisTurn(DuelCardRecord *card) { return (card->flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }
void func_80024D34(s32 slot, s32 deck)
{
    DuelCardRecord *card = &D_801A7AD8[slot];
    memset(card, 0, sizeof *card);
    card->data = &gDuel_aDeckCardRecords[deck];
    card->card_id = (s16)gDuel_aDeckCardRecords[deck].id;
    card->flags = DUEL_CARD_FLAG_OCCUPIED;
}
void DuelCard_RemoveFromField(DuelCardRecord *card) { card->flags = 0; removed++; }
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; }
void SD_SEPlayFull(u32 id) { (void)id; }
void MonsterEffects_Placed(int record, int equip) { (void)record; (void)equip; }
void Spells_Move(int from, int to)
{
    D_801A7AD8[to] = D_801A7AD8[from];
    D_801A7AD8[from].flags = 0;
    moved_from = from; moved_to = to;
}

static int hats_setting = 1;
static void *select_hooked, *act_hooked;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "magical_hats") ? hats_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_hook(const MemoriesModHost *h, void *function, void *replacement, void **original)
{
    (void)h;
    *original = function;
    if (function == (void *)Duel_SelectAttackTrap) select_hooked = replacement;
    else if (function == (void *)DuelCard_CanActThisTurn) act_hooked = replacement;
    else assert(0);
    return 1;
}
static int fake_card_id(const MemoriesModHost *h, const char *identity)
{
    (void)h;
    return !strcmp(identity, "modern-rules:magical_hats:1") ? HATS : !strcmp(identity, "modern-rules:magical_hat:1") ? HAT : -1;
}
static MemoriesModHost fake_host;

static void put(int r, int id, int deck)
{
    gDuel_aDeckCardRecords[deck].id = (u16)id;
    gDuel_aDeckCardRecords[deck].deck_index = (u8)deck;
    func_80024D34(r, deck);
}
static int count(int side, int id)
{
    int col, n = 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if ((D_801A7AD8[MONSTER_RECORD(side, col)].flags & DUEL_CARD_FLAG_OCCUPIED) &&
            D_801A7AD8[MONSTER_RECORD(side, col)].card_id == id)
            n++;
    return n;
}
static int find(int side, int id)
{
    int col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if ((D_801A7AD8[MONSTER_RECORD(side, col)].flags & DUEL_CARD_FLAG_OCCUPIED) &&
            D_801A7AD8[MONSTER_RECORD(side, col)].card_id == id)
            return MONSTER_RECORD(side, col);
    return -1;
}

int main(void)
{
    int round, r;
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.hook = fake_hook;
    fake_host.card_id = fake_card_id;
    assert(Hats_Install(&fake_host) && select_hooked == (void *)select_trap && act_hooked == (void *)can_act);
    assert(Hats_IsDecoy(HAT) && !Hats_IsDecoy(HATS) && !Hats_IsDecoy(2));

    for (round = 0; round < 8; round++) {
        /* The CPU (side 1) attacks the player's Mystical Elf (record 7);
         * the player's Magical Hats (record 10, deck card 3) springs as
         * Fake Trap. Three hats: the Elf in one of the four zones. */
        memset(D_801A7AD8, 0, sizeof D_801A7AD8);
        memset(&T, 0, sizeof T);
        put(7, 2, 1);
        put(9, 26, 2);
        put(10, HATS, 3);
        D_8009B1D5 = 1;
        D_8009B22A = FAKE_TRAP;
        D_8009B1B8 = 10;
        attacked.field_6A = 7;
        D_800E9EF0[1] = &attacked;
        select_trap(0);
        D_801A7AD8[10].flags = 0;   /* the trap leaves the field */
        assert(T.pending && !Hats_Hold() && T.active);
        assert(count(0, HAT) == 3 && count(0, 2) == 1 && count(0, 26) == 1);
        r = find(0, 2);
        assert((D_801A7AD8[r].flags & (DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_DEFENSE_POSITION)) ==
               (DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_DEFENSE_POSITION));
        /* The decoys are made from the trap's deck card, face down in
         * defense, and cannot act. */
        {
            int h = find(0, HAT);
            assert(D_801A7AD8[h].data == &gDuel_aDeckCardRecords[3] && gDuel_aDeckCardRecords[3].id == HAT);
            assert(D_801A7AD8[h].flags & DUEL_CARD_FLAG_FACE_DOWN && !can_act(&D_801A7AD8[h]) && can_act(&D_801A7AD8[r]));
            /* A decoy destroyed: the others stay. */
            D_801A7AD8[h].flags = 0;
            assert(!Hats_Hold() && count(0, HAT) == 2);
        }
        /* The Elf leaves: the hats go. */
        removed = 0;
        D_801A7AD8[r].flags = 0;
        assert(!Hats_Hold() && count(0, HAT) == 0 && removed == 2 && !T.active);
    }

    /* Only one free zone: one hat. */
    memset(D_801A7AD8, 0, sizeof D_801A7AD8);
    memset(&T, 0, sizeof T);
    put(5, 26, 4); put(6, 26, 5); put(7, 2, 1); put(8, 26, 6); put(10, HATS, 3);
    select_trap(0);
    Hats_Hold();
    assert(count(0, HAT) == 1 && count(0, 2) == 1);
    /* No free zone: the trap only stopped the attack. */
    memset(D_801A7AD8, 0, sizeof D_801A7AD8);
    memset(&T, 0, sizeof T);
    put(5, 26, 4); put(6, 26, 5); put(7, 2, 1); put(8, 26, 6); put(9, 26, 7); put(10, HATS, 3);
    select_trap(0);
    Hats_Hold();
    assert(count(0, HAT) == 0 && !T.active);
    /* A direct attack: nothing more. */
    memset(D_801A7AD8, 0, sizeof D_801A7AD8);
    memset(&T, 0, sizeof T);
    put(10, HATS, 3);
    D_800E9EF0[1] = 0;
    select_trap(0);
    Hats_Hold();
    assert(!T.active);
    /* Another trap, or the setting off: nothing. */
    put(7, 2, 1);
    D_800E9EF0[1] = &attacked;
    D_8009B22A = 686;
    select_trap(0);
    assert(!T.pending);
    D_8009B22A = FAKE_TRAP;
    hats_setting = 0;
    select_trap(0);
    assert(!T.pending);
    puts("Magical Hats: the hats up from the trap's deck card, the monster hidden face down among them, decoys idle and going with it, fewer zones, none, a direct attack, other traps, off");
    return 0;
}
