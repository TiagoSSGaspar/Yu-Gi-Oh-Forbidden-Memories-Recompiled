/* Compile for i386: Mai's harpies (Duelist Kingdom rules, harpies.c),
 * against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/harpies.c"

ModernRulesState gModernRules_State;
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
static DuelDeckCardRecord decks[40];

static int harpies_setting = 1;
static MemoriesModCallback subscribed;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "harpies") ? harpies_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_subscribe(const MemoriesModHost *h, unsigned type, int priority, MemoriesModCallback cb)
{ (void)h; (void)priority; assert(type == MEMORIES_EVENT_FUSION); subscribed = cb; return 1; }
static MemoriesModHost fake_host;

static void put(int side, int col, int id, int deck)
{
    DuelCardRecord *card = &D_801A7AD8[MONSTER_RECORD(side, col)];
    memset(card, 0, sizeof *card);
    card->card_id = (s16)id;
    card->flags = DUEL_CARD_FLAG_OCCUPIED;
    decks[deck].deck_index = (u8)deck;
    card->data = &decks[deck];
}
static void clear(int side, int col) { memset(&D_801A7AD8[MONSTER_RECORD(side, col)], 0, sizeof(DuelCardRecord)); }
static int bonus(int side, int col) { return D_801A7AD8[MONSTER_RECORD(side, col)].stat_modifier; }

static int fuse(int a, int b)
{
    MemoriesModEvent e = {MEMORIES_EVENT_FUSION, MEMORIES_BEFORE, a, b, 0, 0, 0};
    subscribed(&e);
    return e.handled ? e.result : -1;
}

int main(void)
{
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.subscribe = fake_subscribe;
    assert(Harpies_Install(&fake_host) && subscribed);

    /* Elegant Egotist onto Harpie Lady, either way round: the sisters. */
    assert(fuse(HARPIE_LADY, ELEGANT_EGOTIST) == HARPIE_SISTERS);
    assert(fuse(ELEGANT_EGOTIST, HARPIE_LADY) == HARPIE_SISTERS);
    assert(fuse(ELEGANT_EGOTIST, 1) == -1 && fuse(HARPIE_LADY, 1) == -1);
    harpies_setting = 0;
    assert(fuse(HARPIE_LADY, ELEGANT_EGOTIST) == -1);
    harpies_setting = 1;

    /* The pet dragon alone: no bonus. A Harpie Lady: +300; the sisters
     * too: +1200; the other side's harpies count for the other side. */
    put(0, 0, PET_DRAGON, 1);
    Harpies_Update();
    assert(bonus(0, 0) == 0);
    put(0, 1, HARPIE_LADY, 2);
    put(1, 1, HARPIE_SISTERS, 3);
    Harpies_Update();
    assert(bonus(0, 0) == 300);
    put(0, 2, HARPIE_SISTERS, 4);
    Harpies_Update();
    Harpies_Update();
    assert(bonus(0, 0) == 1200 && D_801A7AD8[MONSTER_RECORD(0, 0)].defense_modifier == -1200);
    /* An equip's bonus on the dragon is kept apart from the harpies'. */
    D_801A7AD8[MONSTER_RECORD(0, 0)].stat_modifier += 500;
    clear(0, 1);
    Harpies_Update();
    assert(bonus(0, 0) == 500 + 900);
    /* The other side's dragon counts the sisters there. */
    put(1, 4, PET_DRAGON, 5);
    Harpies_Update();
    assert(bonus(1, 4) == 900);
    /* The dragon destroyed and another one in its zone: a fresh start. */
    put(0, 0, PET_DRAGON, 6);
    Harpies_Update();
    assert(bonus(0, 0) == 900);
    /* The setting off: the bonus comes off. */
    harpies_setting = 0;
    Harpies_Update();
    assert(bonus(0, 0) == 0 && bonus(1, 4) == 0 && D_801A7AD8[MONSTER_RECORD(0, 0)].defense_modifier == 0);
    puts("Harpies: Elegant Egotist makes the sisters; the pet dragon counts its side's harpies");
    return 0;
}
