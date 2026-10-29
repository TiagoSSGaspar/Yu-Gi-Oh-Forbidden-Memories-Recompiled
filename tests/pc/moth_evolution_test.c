/* Compile for i386: Petit Moth's evolution (Duelist Kingdom rules,
 * moth.c), against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/moth.c"

u16 gDuel_wSceneStateFlags;
u8 D_8009B1D5;
DuelSideState D_800E9FF0[DUEL_SIDE_COUNT];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
static DuelDeckCardRecord deck_cards[DUEL_CARD_RECORD_COUNT];
volatile u32 D_8009B0F4_abs;
u32 D_8009B134_abs;
u16 D_800EA128[DUEL_RITUAL_IMAGE_PREFIX_HALFWORDS];
u8 D_8018C2D8[80 * 1408];
static int sounds, requests, loads, loaded_id, stores, remakes;
static DuelEffectRequest effect_request;
void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
DuelEffectRequest *DuelEffect_CreateRequest(s32 id) { (void)id; requests++; return &effect_request; }
FileTransferDescriptor *func_80029164(s32 slot, s32 value) { (void)slot; loads++; loaded_id = value; return 0; }
int StoreImage(RECT *rect, u32 *pixels) { (void)rect; (void)pixels; stores++; return 0; }
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; }
void DuelCard_DeactivateRecord(DuelCardRecord *card) { card->flags &= ~DUEL_CARD_FLAG_OCCUPIED; }
/* The game's remake: the record occupied again with its deck card's id,
 * its modifiers cleared (the mod puts them back). */
void func_80024D34(s32 record, s32 deck_index)
{
    DuelCardRecord *card = &D_801A7AD8[record];
    (void)deck_index;
    remakes++;
    card->flags = DUEL_CARD_FLAG_OCCUPIED;
    card->card_id = (s16)((DuelDeckCardRecord *)card->data)->id;
    card->stat_modifier = card->defense_modifier = 0;
}

/* The host: settings, the two events' callbacks, the state. */
static int moth_setting = 1, applied = 1, registered_size;
static MemoriesModCallback fusion_cb, monster_cb;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "moth_evolution") ? moth_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return applied; }
static int fake_subscribe(const MemoriesModHost *h, unsigned event, int priority, MemoriesModCallback fn)
{
    (void)h; (void)priority;
    if (event == MEMORIES_EVENT_FUSION) fusion_cb = fn;
    if (event == MEMORIES_EVENT_MONSTER) monster_cb = fn;
    return 1;
}
static int fake_register_state(const MemoriesModHost *h, void *data, size_t size, unsigned version)
{ (void)h; (void)data; (void)version; registered_size = (int)size; return 1; }
/* The overlay: a 1280x960 window at the port's scale 2; the words drawn. */
static char drawn[8][96];
static int drawn_count;
static void fake_overlay_size(const MemoriesModHost *h, int *w, int *hh, int *scale)
{ (void)h; *w = 1280; *hh = 960; *scale = 2; }
static int fake_text_width(const MemoriesModHost *h, const char *t, int scale) { (void)h; return (int)strlen(t) * 6 * scale; }
static void fake_fill(const MemoriesModHost *h, int x, int y, int w, int hh, uint32_t rgb, unsigned a)
{ (void)h; (void)x; (void)y; (void)w; (void)hh; (void)rgb; (void)a; }
static void fake_draw_text(const MemoriesModHost *h, int x, int middle, const char *t, uint32_t rgb, int scale)
{
    (void)h; (void)x; (void)middle; (void)rgb; (void)scale;
    assert(drawn_count < 8);
    snprintf(drawn[drawn_count++], sizeof drawn[0], "%s", t);
}
static void fake_picture_point(const MemoriesModHost *h, int x, int y, int *cx, int *cy)
{ (void)h; *cx = x * 4; *cy = y * 4; }
static const char *fake_text(const MemoriesModHost *h, const char *english)
{
    (void)h;
    if (!strcmp(english, "Cocoon: Perfectly Ultimate Great Moth in %d turns"))
        return "Casulo: Perfectly Ultimate Great Moth em %d turnos";
    return english;
}
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static int fuse(int a, int b)
{
    MemoriesModEvent e = {MEMORIES_EVENT_FUSION, MEMORIES_BEFORE, a, b, 0, 0, 0};
    fusion_cb(&e);
    return e.handled ? e.result : 0;
}
static void monster_event(int what, int card, int record)
{
    MemoriesModEvent e = {MEMORIES_EVENT_MONSTER, MEMORIES_AFTER, card, record, what, 0, 0};
    monster_cb(&e);
}
/* A monster in record r (5-9 the player's zones, 20-24 the CPU's). */
static void put(int r, int id, unsigned flags)
{
    D_801A7AD8[r].card_id = (s16)id;
    D_801A7AD8[r].flags = (u16)(DUEL_CARD_FLAG_OCCUPIED | flags);
    D_801A7AD8[r].data = &deck_cards[r];
    deck_cards[r].id = (u16)id;
}

int main(void)
{
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.subscribe = fake_subscribe;
    fake_host.register_state = fake_register_state;
    fake_host.overlay_size = fake_overlay_size;
    fake_host.text_width = fake_text_width;
    fake_host.fill = fake_fill;
    fake_host.draw_text = fake_draw_text;
    fake_host.picture_point = fake_picture_point;
    fake_host.text = fake_text;
    assert(Moth_Install(&fake_host, &fake_mod) && fusion_cb && monster_cb && registered_size == (int)sizeof S);

    /* The larva (Petit Moth, or Larvae Moth as the anime calls it) and
     * Cocoon of Evolution fuse into the cocoon, either way round; nothing
     * else changes; off, they do not. */
    assert(fuse(MOTH_PETIT, MOTH_COCOON) == MOTH_COCOON && fuse(MOTH_COCOON, MOTH_PETIT) == MOTH_COCOON);
    assert(fuse(MOTH_LARVAE, MOTH_COCOON) == MOTH_COCOON && fuse(MOTH_COCOON, MOTH_LARVAE) == MOTH_COCOON);
    assert(fuse(MOTH_GREAT, MOTH_COCOON) == 0);
    assert(fuse(MOTH_PETIT, 1) == 0);
    moth_setting = 0;
    assert(fuse(MOTH_PETIT, MOTH_COCOON) == 0);
    moth_setting = 1;

    /* Only the placement's fusion makes a cocoon: the CPU's script, the
     * fusion guard and the helper ask in the hand. */
    gDuel_wSceneStateFlags = 4;
    fuse(MOTH_PETIT, MOTH_COCOON);
    put(6, MOTH_COCOON, 0);
    monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 6);
    assert(!S.turns[6]);
    /* The placement's: the side's next summoned Cocoon is the cocoon, in
     * defense. */
    gDuel_wSceneStateFlags = 7;
    fuse(MOTH_COCOON, MOTH_PETIT);
    monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 6);
    assert(S.turns[6] == 1 && S.card[6] == MOTH_COCOON);
    assert(D_801A7AD8[6].flags & DUEL_CARD_FLAG_DEFENSE_POSITION);
    /* A Cocoon played alone later is none. */
    put(7, MOTH_COCOON, 0);
    monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 7);
    assert(!S.turns[7]);
    /* A fusion whose chain went on into another card marks nothing. */
    fuse(MOTH_PETIT, MOTH_COCOON);
    put(8, 3, 0);
    monster_event(MEMORIES_MONSTER_SUMMON, 3, 8);
    put(9, MOTH_COCOON, 0);
    monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 9);
    assert(!S.turns[8] && !S.turns[9]);
    /* The CPU's placement marks the CPU's side only. */
    D_8009B1D5 = 1;
    fuse(MOTH_PETIT, MOTH_COCOON);
    put(21, MOTH_COCOON, 0);
    monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 21);
    assert(S.turns[21] == 1);
    D_8009B1D5 = 0;

    /* Each of its owner's turns counts; at the 5th it opens: Perfectly
     * Ultimate Great Moth. The hand waits while it evolves: the picture
     * loaded, stored, the record remade face up, in attack, what equips gave
     * the cocoon kept. */
    {
        int turn, holds;
        D_801A7AD8[6].stat_modifier = 300;
        for (turn = 1; turn <= 4; turn++) monster_event(MEMORIES_MONSTER_DRAW, MOTH_COCOON, 6);
        assert(S.turns[6] == 5 && !S.due[6] && !Moth_Hold());
        monster_event(MEMORIES_MONSTER_DRAW, MOTH_COCOON, 6);                 /* the 5th */
        assert(S.due[6] == MOTH_PERFECT);
        D_8009B0F4_abs = 0x10;                                                 /* the picture still coming */
        assert(Moth_Hold() && loads == 1 && loaded_id == MOTH_PERFECT && requests == 1 && sounds == 1);
        assert(Moth_Hold() && !stores);
        D_8009B0F4_abs = 0;
        assert(Moth_Hold() && stores == 1);
        assert(!Moth_Hold() && remakes == 1);
        assert(D_801A7AD8[6].card_id == MOTH_PERFECT && S.evolving == -1 && !S.turns[6]);
        assert(!(D_801A7AD8[6].flags & (DUEL_CARD_FLAG_DEFENSE_POSITION | DUEL_CARD_FLAG_FACE_DOWN)));
        assert(D_801A7AD8[6].stat_modifier == 300);
        monster_event(MEMORIES_MONSTER_DRAW, MOTH_PERFECT, 6);                /* nothing after it */
        assert(!S.turns[6] && !Moth_Hold() && remakes == 1);
        (void)holds;
    }
    /* Destroyed before 4 turns, the cocoon dies with the larva. */
    assert(S.turns[21] == 1);
    D_801A7AD8[21].flags = 0;
    assert(!Moth_Hold() && !S.turns[21] && remakes == 1);
    /* Destroyed after 4 turns, Great Moth comes out in its place, in attack;
     * two such at once, one after the other. A cocoon fused away (its record
     * holding another card) is only forgotten. */
    {
        int holds;
        gDuel_wSceneStateFlags = 7;
        fuse(MOTH_PETIT, MOTH_COCOON);
        put(5, MOTH_COCOON, DUEL_CARD_FLAG_DEFENSE_POSITION);
        monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 5);
        fuse(MOTH_LARVAE, MOTH_COCOON);
        put(7, MOTH_COCOON, 0);
        monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 7);
        fuse(MOTH_PETIT, MOTH_COCOON);
        put(8, MOTH_COCOON, 0);
        monster_event(MEMORIES_MONSTER_SUMMON, MOTH_COCOON, 8);
        S.turns[5] = S.turns[7] = S.turns[8] = 5;                              /* 4 turns each */
        D_801A7AD8[5].flags = 0;                                               /* destroyed */
        D_801A7AD8[7].flags &= ~DUEL_CARD_FLAG_OCCUPIED;                       /* a 2D battle's loser */
        D_801A7AD8[8].card_id = 3;                                             /* fused into 3 */
        for (holds = 0; Moth_Hold(); holds++) assert(holds < 16);
        assert(D_801A7AD8[5].card_id == MOTH_GREAT && D_801A7AD8[7].card_id == MOTH_GREAT);
        assert(D_801A7AD8[5].flags & DUEL_CARD_FLAG_OCCUPIED);
        assert(!(D_801A7AD8[5].flags & DUEL_CARD_FLAG_DEFENSE_POSITION));
        assert(D_801A7AD8[8].card_id == 3 && !S.turns[5] && !S.turns[7] && !S.turns[8]);
        assert(remakes == 3);
    }
    /* Off, nothing waits. */
    moth_setting = 0;
    S.due[5] = MOTH_PERFECT;
    assert(!Moth_Hold());
    moth_setting = 1;
    S.due[5] = 0;

    /* The player's cocoons say how many turns are left, in the player's
     * language; its last turn, that Great Moth is ready. The CPU's do not
     * show. The signature follows the count. */
    {
        unsigned before;
        memset(S.turns, 0, sizeof S.turns);
        put(8, MOTH_COCOON, 0);
        S.turns[8] = 2; S.card[8] = MOTH_COCOON;                  /* 1 turn gone: 4 left */
        put(22, MOTH_COCOON, 0);
        S.turns[22] = 1; S.card[22] = MOTH_COCOON;
        gDuel_wSceneStateFlags = 4;
        drawn_count = 0;
        fake_mod.overlay();
        assert(drawn_count == 1 && !strcmp(drawn[0], "Casulo: Perfectly Ultimate Great Moth em 4 turnos"));
        before = fake_mod.overlay_signature();
        S.turns[8] = 5;                                           /* 4 gone: the last */
        assert(fake_mod.overlay_signature() != before);
        drawn_count = 0;
        fake_mod.overlay();
        assert(drawn_count == 1 &&
               !strcmp(drawn[0], "Cocoon: Perfectly Ultimate Great Moth next turn; destroyed now, Great Moth comes out"));
        /* Lifted off its record (a battle starting): no line, not forgotten. */
        D_801A7AD8[8].flags &= ~DUEL_CARD_FLAG_OCCUPIED;
        drawn_count = 0;
        fake_mod.overlay();
        assert(!drawn_count && S.turns[8] == 5);
        D_801A7AD8[8].flags |= DUEL_CARD_FLAG_OCCUPIED;
        /* Not while the duel is busy, nor off. */
        gDuel_wSceneStateFlags = 9;
        drawn_count = 0;
        fake_mod.overlay();
        assert(!drawn_count && !fake_mod.overlay_signature());
        gDuel_wSceneStateFlags = 4;
        moth_setting = 0;
        drawn_count = 0;
        fake_mod.overlay();
        assert(!drawn_count);
        moth_setting = 1;
    }

    printf("The moth's evolution: the fusion, the cocoon, the turns, Perfectly Ultimate, Great Moth from a destroyed cocoon, the turns left on screen passed\n");
    return 0;
}
