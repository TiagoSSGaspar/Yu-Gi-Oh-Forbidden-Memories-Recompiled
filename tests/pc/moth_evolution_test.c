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
DuelEffectResourceRecord D_800EA0E8[4];
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
    assert(Moth_Install(&fake_host, &fake_mod) && fusion_cb && monster_cb && registered_size == (int)sizeof S);

    /* Petit Moth and Cocoon of Evolution fuse into the cocoon, either way
     * round; nothing else changes; off, they do not. */
    assert(fuse(MOTH_PETIT, MOTH_COCOON) == MOTH_COCOON && fuse(MOTH_COCOON, MOTH_PETIT) == MOTH_COCOON);
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

    printf("Petit Moth's evolution: the fusion, the cocoon passed\n");
    return 0;
}
