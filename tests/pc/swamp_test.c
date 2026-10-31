/* Compile for i386: the swamp brothers (Duelist Kingdom rules, swamp.c),
 * against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/swamp.c"

ModernRulesState gModernRules_State;
u16 gDuel_wSceneStateFlags;
u16 D_8009B170[2], D_8009B178[2];
DisplayObject *D_800E9EF0[7];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
static DisplayObject attacker_object, attacked_object;
static int applied_flags;

/* A card's ATK in the low half and DEF in the high, as Duel_CalcCardStats
 * gives them: base 1800/1500 or 1550/1800 for the battleguards, plus the
 * record's stat_modifier. */
static int base_attack(int id) { return id == SWAMP_BATTLEGUARD ? 1800 : id == LAVA_BATTLEGUARD ? 1550 : 2350; }
s32 Duel_CalcCardStats(DuelCardRecord *card)
{
    int attack = base_attack(card->card_id) + card->stat_modifier;
    return (attack & 0xFFFF) | (1500 + card->stat_modifier) << 16;
}
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; applied_flags++; }
/* The wall of Kuribohs is tested on its own (multiply_test.c). */
static int multiply_began, multiply_over;
void Multiply_BattleBegan(void) { multiply_began++; }
void Multiply_BattleOver(void) { multiply_over++; }

/* The battle as the game plays it: its first frame lifts the two monsters
 * off their records keeping the attacked one's modifier and flags; its last
 * puts them back from those and leaves for the field. */
static int battle_frames, battle_ends, battle_destroys;
void DuelScene_UpdateBattle(void)
{
    battle_frames++;
    if (!(gDuel_wSceneStateFlags & SCENE_STARTED)) {
        DuelCardRecord *attacked = &D_801A7AD8[attacked_object.field_6A];
        D_8009B170[1] = (u16)attacked->stat_modifier;
        D_8009B178[1] = attacked->flags;
        attacked->flags &= (u16)~DUEL_CARD_FLAG_OCCUPIED;
        gDuel_wSceneStateFlags |= SCENE_STARTED;
        return;
    }
    if (battle_ends) {
        DuelCardRecord *attacked = &D_801A7AD8[attacked_object.field_6A];
        attacked->flags = battle_destroys ? 0 : (u16)(DUEL_CARD_FLAG_OCCUPIED | (D_8009B178[1] & 0xA00));
        attacked->stat_modifier = (s16)D_8009B170[1];
        gDuel_wSceneStateFlags = 5;
    }
}

static int swamp_setting = 1;
static void *hooked;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "swamp_brothers") ? swamp_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_hook(const MemoriesModHost *h, void *function, void *replacement, void **original)
{ (void)h; *original = function; hooked = replacement; return 1; }
static char drawn[96];
static int drawn_count, earlier;
static void fake_overlay_size(const MemoriesModHost *h, int *w, int *hh, int *scale)
{ (void)h; *w = 1280; *hh = 960; *scale = 2; }
static int fake_text_width(const MemoriesModHost *h, const char *t, int scale) { (void)h; return (int)strlen(t) * 6 * scale; }
static void fake_fill(const MemoriesModHost *h, int x, int y, int w, int hh, uint32_t rgb, unsigned a)
{ (void)h; (void)x; (void)y; (void)w; (void)hh; (void)rgb; (void)a; }
static void fake_draw_text(const MemoriesModHost *h, int x, int middle, const char *t, uint32_t rgb, int scale)
{ (void)h; (void)x; (void)middle; (void)rgb; (void)scale; drawn_count++; snprintf(drawn, sizeof drawn, "%s", t); }
static void fake_picture_point(const MemoriesModHost *h, int x, int y, int *cx, int *cy)
{ (void)h; *cx = x * 4; *cy = y * 4; }
static const char *fake_text(const MemoriesModHost *h, const char *english)
{ (void)h; return !strcmp(english, "The swamp brothers join: %d") ? "Os irmãos do pântano se unem: %d" : english; }
static void earlier_overlay(void) { earlier++; }
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static void put(int r, int id, unsigned flags)
{
    memset(&D_801A7AD8[r], 0, sizeof D_801A7AD8[r]);
    D_801A7AD8[r].card_id = (s16)id;
    D_801A7AD8[r].flags = (u16)(DUEL_CARD_FLAG_OCCUPIED | flags);
    D_801A7AD8[r].object = &attacked_object;
}
/* A battle against record `attacked`: its first frame, then the last. */
static void battle_start(int attacked)
{
    attacker_object.field_6A = 5;
    attacked_object.field_6A = (u8)attacked;
    D_800E9EF0[0] = &attacker_object;
    D_800E9EF0[1] = &attacked_object;
    gDuel_wSceneStateFlags = SCENE_BATTLE;
    battle_ends = 0;
    battle();
}
static void battle_end(void)
{
    battle();   /* a frame in the middle */
    battle_ends = 1;
    battle();
}

int main(void)
{
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.hook = fake_hook;
    fake_host.overlay_size = fake_overlay_size;
    fake_host.text_width = fake_text_width;
    fake_host.fill = fake_fill;
    fake_host.draw_text = fake_draw_text;
    fake_host.picture_point = fake_picture_point;
    fake_host.text = fake_text;
    fake_mod.overlay = earlier_overlay;
    assert(Swamp_Install(&fake_host, &fake_mod) && hooked == (void *)battle && fake_mod.overlay == overlay);

    /* The CPU's Swamp Battleguard (record 20) in defense, Lava Battleguard
     * beside it (22): attacked, it fights in attack with 3350. */
    put(5, 99, 0);
    put(20, SWAMP_BATTLEGUARD, DUEL_CARD_FLAG_DEFENSE_POSITION);
    put(22, LAVA_BATTLEGUARD, 0);
    battle_start(20);
    assert(W.record == 20 && W.boost == 1550 && W.total == 3350);
    assert((Duel_CalcCardStats(&D_801A7AD8[20]) & 0xFFFF) == 3350 && (s16)D_8009B170[1] == 1550);
    assert(!(D_801A7AD8[20].flags & DUEL_CARD_FLAG_DEFENSE_POSITION) && !(D_8009B178[1] & DUEL_CARD_FLAG_DEFENSE_POSITION));
    /* The line during the battle. */
    drawn_count = 0;
    overlay();
    assert(earlier == 1 && drawn_count == 1 && !strcmp(drawn, "Os irmãos do pântano se unem: 3350"));
    /* The battle over, the battleguard still there: its own ATK, in defense. */
    battle_end();
    assert(W.record == -1 && D_801A7AD8[20].stat_modifier == 0 &&
           (D_801A7AD8[20].flags & DUEL_CARD_FLAG_DEFENSE_POSITION) && applied_flags == 1);
    drawn_count = 0;
    overlay();
    assert(drawn_count == 0);

    /* Lava attacked, Swamp beside it: 1550 + 1800, from attack. */
    put(22, LAVA_BATTLEGUARD, 0);
    battle_start(22);
    assert(W.total == 3350 && !W.was_defense);
    /* Destroyed in the battle: nothing to give back. */
    battle_ends = battle_destroys = 1;
    battle();
    battle_destroys = 0;
    assert(W.record == -1 && applied_flags == 1);

    /* One brother alone, another monster attacked, the setting off: nothing. */
    put(22, 7, 0);
    battle_start(20);
    assert(W.record == -1 && D_801A7AD8[20].stat_modifier == 0);
    battle_end();
    put(22, LAVA_BATTLEGUARD, 0);
    battle_start(22);
    battle_end();
    put(21, 99, 0);
    battle_start(21);
    assert(W.record == -1);
    battle_end();
    swamp_setting = 0;
    battle_start(20);
    assert(W.record == -1 && (D_801A7AD8[20].stat_modifier == 0));
    battle_end();
    swamp_setting = 1;
    /* The brother on the other side does not count. */
    put(22, 0, 0);
    D_801A7AD8[22].flags = 0;
    put(7, LAVA_BATTLEGUARD, 0);
    battle_start(20);
    assert(W.record == -1);
    battle_end();
    /* A direct attack: nothing. */
    D_800E9EF0[1] = 0;
    gDuel_wSceneStateFlags = SCENE_BATTLE;
    battle();
    assert(W.record == -1);

    printf("The swamp brothers: together from defense and attack, given back, destroyed, alone, off, sides passed\n");
    return 0;
}
