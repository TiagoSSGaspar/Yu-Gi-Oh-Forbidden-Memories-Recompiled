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
u16 gDuel_wSceneStateFlags;
u8 D_8009B1D5;
s16 D_8009B22A;
u8 D_8009B1B8;
DisplayObject *D_800E9EF0[7];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
static DisplayObject attacker_object, attacked_object;
static int sounds, lights;
static DuelEffectRequest light;
void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
DuelEffectRequest *DuelEffect_CreateRequest(s32 id) { (void)id; lights++; return &light; }

/* The battle's first frame clears the trap and looks for one (a trap of
 * the test's, when it sets one); its last leaves for the field. */
static s16 trap_found;
static int battle_ends;
void DuelScene_UpdateBattle(void)
{
    if (!(gDuel_wSceneStateFlags & SCENE_STARTED)) {
        D_8009B22A = trap_found;
        D_8009B1B8 = 0;
        gDuel_wSceneStateFlags |= SCENE_STARTED;
        return;
    }
    if (battle_ends) gDuel_wSceneStateFlags = 5;
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
{ (void)h; return !strcmp(english, "The wall of Kuribohs stops the attack") ? "A muralha de Kuribohs bloqueia o ataque" : english; }
static void earlier_overlay(void) { earlier++; }
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static void put(int r, int id)
{
    memset(&D_801A7AD8[r], 0, sizeof D_801A7AD8[r]);
    D_801A7AD8[r].card_id = (s16)id;
    D_801A7AD8[r].flags = DUEL_CARD_FLAG_OCCUPIED;
}
static void empty(int r) { D_801A7AD8[r].flags = 0; }
/* The player (side 0) attacks record `attacked` (-1 a direct attack). */
static void attack(int attacked)
{
    D_8009B1D5 = attacked >= 0 ? (u8)(1 - attacked / 15) : D_8009B1D5;
    attacker_object.field_6A = (u8)(D_8009B1D5 * 15 + 5);
    D_800E9EF0[0] = &attacker_object;
    if (attacked >= 0) {
        attacked_object.field_6A = (u8)attacked;
        D_800E9EF0[1] = &attacked_object;
    } else {
        D_800E9EF0[1] = 0;
    }
    gDuel_wSceneStateFlags = SCENE_BATTLE;
    battle_ends = 0;
    battle();
}
static void end(void)
{
    battle_ends = 1;
    battle();
}

int main(void)
{
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.card_id = fake_card_id;
    fake_host.hook = fake_hook;
    fake_host.overlay_size = fake_overlay_size;
    fake_host.text_width = fake_text_width;
    fake_host.fill = fake_fill;
    fake_host.draw_text = fake_draw_text;
    fake_host.picture_point = fake_picture_point;
    fake_host.text = fake_text;
    fake_mod.overlay = earlier_overlay;
    assert(Multiply_Install(&fake_host, &fake_mod) && hooked == (void *)battle && fake_mod.overlay == overlay);
    assert(Multiply_Is(MULTIPLY_ID) && !Multiply_Is(301) && !Multiply_Is(0));
    Multiply_Is(MULTIPLY_ID);
    assert(identity_asks == 1);   /* asked once, then kept */

    /* Landed: the wall comes up with the ritual's light and sound. */
    Multiply_Landed(MONSTER_RECORD(1, 2));
    assert(lights == 1 && sounds == 1);

    /* The CPU's Kuriboh (record 22) and Multiply (record 27); its Celtic
     * Guardian (record 20). The player's attack on the Kuriboh: the wall
     * stops it as Fake Trap would, the Kuriboh shown. */
    put(MONSTER_RECORD(1, 0), 36);
    put(MONSTER_RECORD(1, 2), KURIBOH);
    put(SPELL_RECORD(1, 2), MULTIPLY_ID);
    put(MONSTER_RECORD(0, 0), 1);
    attack(MONSTER_RECORD(1, 2));
    assert(D_8009B22A == FAKE_TRAP && D_8009B1B8 == MONSTER_RECORD(1, 2) && K.blocking);
    drawn_count = 0;
    overlay();
    assert(earlier == 1 && drawn_count == 1 && !strcmp(drawn, "A muralha de Kuribohs bloqueia o ataque"));
    end();
    assert(!K.blocking);
    drawn_count = 0;
    overlay();
    assert(drawn_count == 0);
    /* The Celtic Guardian beside it is attacked as any monster is, as at
     * the end of the anime's duel. */
    attack(MONSTER_RECORD(1, 0));
    assert(D_8009B22A == 0 && !K.blocking);
    end();
    /* A direct attack: stopped, Yugi untouched; with no Kuriboh left (a
     * Raigeki), Multiply is the card shown. */
    D_8009B1D5 = 0;
    attack(-1);
    assert(D_8009B22A == FAKE_TRAP && D_8009B1B8 == MONSTER_RECORD(1, 2) && K.blocking);
    end();
    empty(MONSTER_RECORD(1, 2));
    attack(-1);
    assert(D_8009B22A == FAKE_TRAP && D_8009B1B8 == SPELL_RECORD(1, 2));
    end();
    put(MONSTER_RECORD(1, 2), KURIBOH);
    /* A trap of the CPU's own sprang: it stands. */
    trap_found = 683;
    attack(MONSTER_RECORD(1, 2));
    assert(D_8009B22A == 683 && !K.blocking);
    end();
    trap_found = 0;
    /* Multiply gone: the attack on the Kuriboh goes through. */
    empty(SPELL_RECORD(1, 2));
    attack(MONSTER_RECORD(1, 2));
    assert(D_8009B22A == 0 && !K.blocking);
    end();
    /* The other side's Multiply does not count; off: nothing. */
    put(SPELL_RECORD(0, 1), MULTIPLY_ID);
    attack(MONSTER_RECORD(1, 2));
    assert(D_8009B22A == 0);
    end();
    put(SPELL_RECORD(1, 2), MULTIPLY_ID);
    multiply_setting = 0;
    attack(MONSTER_RECORD(1, 2));
    assert(D_8009B22A == 0 && !Multiply_Is(MULTIPLY_ID));
    end();

    printf("Kuriboh and Multiply: the identity, the wall up, attacks on Kuriboh stopped, others not, direct stopped, "
           "a trap of its own, Multiply gone, sides, off passed\n");
    return 0;
}
