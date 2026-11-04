/* Compile for i386: Mirror Force (Duelist Kingdom rules, mirror.c),
 * against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/mirror.c"

#define MIRROR 726
ModernRulesState gModernRules_State;
u8 D_8009B1D5, D_8009B1B8;
s16 D_8009B22A;
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
DuelSideState D_800E9FF0[DUEL_SIDE_COUNT];
static DisplayObject objects[DUEL_CARD_RECORD_COUNT];
static DuelEffectRequest request;
static int removed, sounds, requests, trap_result;
static int sprung_effect, sprung_record;

s32 Duel_SelectAttackTrap(u8 *record) { (void)record; D_8009B22A = (s16)sprung_effect; D_8009B1B8 = (u8)sprung_record; return trap_result; }
DuelEffectRequest *DuelEffect_CreateRequest(s32 kind) { (void)kind; requests++; return &request; }
s32 Duel_GetCardEffectVariant(CardObject *object) { (void)object; return 0; }
void DuelCard_RemoveFromField(DuelCardRecord *card) { card->flags = 0; removed++; }
void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
/* A card's ATK is its id times ten. */
s32 Duel_CalcCardStats(DuelCardRecord *card) { return (card->card_id * 10) & 0xFFFF; }
static int damage_total, damage_side = -1;
int Mods_DamageLife(int side, int life, int damage, int kind)
{ (void)kind; damage_total += damage; damage_side = side; return life - damage; }

static int mirror_setting = 1;
static void *hooked;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "mirror_force") ? mirror_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_hook(const MemoriesModHost *h, void *function, void *replacement, void **original)
{ (void)h; assert(function == (void *)Duel_SelectAttackTrap); *original = function; hooked = replacement; return 1; }
static int fake_card_id(const MemoriesModHost *h, const char *identity)
{ (void)h; return !strcmp(identity, "modern-rules:mirror_force:1") ? MIRROR : -1; }
static MemoriesModHost fake_host;

static void put(int r, int id, int defense)
{
    D_801A7AD8[r].card_id = (s16)id;
    D_801A7AD8[r].flags = (u16)(DUEL_CARD_FLAG_OCCUPIED | (defense ? DUEL_CARD_FLAG_DEFENSE_POSITION : 0));
    D_801A7AD8[r].object = &objects[r];
}
static int occupied(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }
static int hold_all(void)
{
    int n = 0;
    while (Mirror_Hold()) assert(++n < 1000);
    return n;
}

int main(void)
{
    fake_host.api = 23;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.hook = fake_hook;
    fake_host.card_id = fake_card_id;
    assert(Mirror_Install(&fake_host) && hooked == (void *)select_trap && !Mirror_Hold());

    /* The CPU (side 1) attacks with its 2500 (record 20); the player's
     * Mirror Force (record 10) springs as Widespread Ruin. The disc's trap
     * takes the attacker; the 2500 turned back strikes the 1400 (1100 of
     * damage), the 2200 (300) and the 3000 (none): all three in attack go,
     * the one in defense stays, the player's own too. */
    D_8009B1D5 = 1;
    D_800E9FF0[1].life_points.unsigned_value = 4000;
    put(10, MIRROR, 0);
    put(MONSTER_RECORD(1, 0), 250, 0);
    put(MONSTER_RECORD(1, 1), 140, 0);
    put(MONSTER_RECORD(1, 2), 220, 0);
    put(MONSTER_RECORD(1, 3), 300, 0);
    put(MONSTER_RECORD(1, 4), 100, 1);
    put(MONSTER_RECORD(0, 2), 26, 0);
    objects[MONSTER_RECORD(1, 0)].field_6A = MONSTER_RECORD(1, 0);
    sprung_effect = WIDESPREAD_RUIN; sprung_record = 10; trap_result = 1;
    assert(select_trap((u8 *)&objects[MONSTER_RECORD(1, 0)]) == 1 && M.reflect == 2500);
    D_801A7AD8[MONSTER_RECORD(1, 0)].flags = 0;   /* the disc's trap took the attacker */
    assert(hold_all() > 0);
    assert(removed == 3 && sounds == 3 && requests == 3);
    assert(damage_total == 1100 + 300 && damage_side == 1 && D_800E9FF0[1].life_points.unsigned_value == 4000 - 1400);
    assert(!occupied(MONSTER_RECORD(1, 1)) && !occupied(MONSTER_RECORD(1, 2)) && !occupied(MONSTER_RECORD(1, 3)));
    assert(occupied(MONSTER_RECORD(1, 4)) && occupied(MONSTER_RECORD(0, 2)));
    assert(M.burned[1] == 1 && M.burned[0] == 0);   /* the CPU fell into it once */
    damage_total = 0;

    /* A Widespread Ruin of the disc's: nothing more. */
    removed = 0;
    put(MONSTER_RECORD(1, 1), 26, 0);
    put(10, WIDESPREAD_RUIN, 0);
    select_trap((u8 *)&objects[MONSTER_RECORD(1, 0)]);
    assert(hold_all() == 0 && removed == 0);
    /* No trap sprang, or another one: nothing. */
    put(10, MIRROR, 0);
    trap_result = 0;
    select_trap((u8 *)&objects[MONSTER_RECORD(1, 0)]);
    assert(hold_all() == 0 && removed == 0);
    trap_result = 1; sprung_effect = 690;
    select_trap((u8 *)&objects[MONSTER_RECORD(1, 0)]);
    assert(hold_all() == 0 && removed == 0);
    /* The setting off: a Widespread Ruin. */
    sprung_effect = WIDESPREAD_RUIN;
    mirror_setting = 0;
    select_trap((u8 *)&objects[MONSTER_RECORD(1, 0)]);
    assert(hold_all() == 0 && removed == 0);
    /* The player attacks the CPU's mirror: the player's side goes. */
    mirror_setting = 1;
    D_8009B1D5 = 0;
    put(25, MIRROR, 0);
    sprung_record = 25;
    objects[MONSTER_RECORD(0, 2)].field_6A = MONSTER_RECORD(0, 2);
    select_trap((u8 *)&objects[MONSTER_RECORD(0, 2)]);
    assert(hold_all() > 0 && removed == 1 && !occupied(MONSTER_RECORD(0, 2)) && occupied(MONSTER_RECORD(1, 1)));
    /* A new duel: forgotten. */
    assert(M.burned[0] == 1);
    Mirror_TurnStarts();
    assert(!M.burned[0] && !M.burned[1]);
    puts("Mirror Force: the attack turned back as battles (damage over each weaker one), the attacker's monsters in attack position destroyed one at a time, defense spared, other traps and off untouched, both sides");
    return 0;
}
