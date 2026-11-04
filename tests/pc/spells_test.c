/* Compile for i386: Change of Heart and Monster Reborn (Duelist Kingdom
 * rules, spells.c), against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/spells.c"

#define HEART 727
#define REBORN 728
ModernRulesState gModernRules_State;
u16 gDuel_wSceneStateFlags, gInput_wPad1Pressed;
u8 D_8009B1D5;
s8 D_8009B360[2] = {-1, 0};
DuelSideState D_800E9FF0[DUEL_SIDE_COUNT];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
DuelDeckCardRecord gDuel_aDeckCardRecords[80];
s32 gDuel_adwCardStats[800];
/* Side 0's grid as the player sees it (row 1 the opponent's monsters,
 * records 20-24), and side 1's (row 1 the player's, 5-9). */
u8 D_800907D8[2 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT] = {
    25, 26, 27, 28, 29, 20, 21, 22, 23, 24, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    10, 11, 12, 13, 14, 5, 6, 7, 8, 9, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29};
static DuelDeckCardRecord *deck_card(int deck) { return &gDuel_aDeckCardRecords[deck]; }

/* A card's ATK is its id times ten (base stats too), plus the modifier. */
s32 Duel_CalcCardStats(DuelCardRecord *card) { return (card->card_id * 10 + card->stat_modifier) & 0xFFFF; }
int Cards_NameUtf8(int id, char *out, size_t size) { snprintf(out, size, "card %d", id); return 1; }
static int made, removed, placed_from = -1, harpies_moved, equips_moved, requests, sounds;
/* As the game's setup does (Duel_SetupCardRecord): an index under 40 for
 * a record of the CPU's side is taken as the CPU's own deck card. */
void func_80024D34(s32 slot, s32 deck)
{
    DuelCardRecord *card = &D_801A7AD8[slot];
    if (slot >= DUEL_CARD_SIDE_RECORD_COUNT && deck < DECK_SIZE) deck += DECK_SIZE;
    memset(card, 0, sizeof *card);
    card->data = deck_card(deck);
    card->card_id = (s16)deck_card(deck)->id;
    card->flags = DUEL_CARD_FLAG_OCCUPIED;
    made++;
}
void DuelCard_RemoveFromField(DuelCardRecord *card) { card->flags = 0; removed++; }
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; }
/* The CPU's five hand records, in order (D_800907CC: 0x80 + n is record 15 + n). */
u8 D_800907CC[2 * HAND_SIZE + 2] = {0, 1, 2, 3, 4, 0x80, 0x81, 0x82, 0x83, 0x84};
static DuelEffectRequest request;
DuelEffectRequest *DuelEffect_CreateRequest(s32 id) { (void)id; requests++; return &request; }
s32 Duel_GetCardEffectVariant(CardObject *object) { (void)object; return 0; }
void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
void MonsterEffects_Placed(int record, int equip) { (void)equip; placed_from = record; }
void Harpies_Moved(int from, int to) { (void)from; (void)to; harpies_moved++; }
void Equips_Moved(int from, int to) { (void)from; (void)to; equips_moved++; }
/* Magical Hats' decoys (hats_test.c): card 999 here. */
int Hats_IsDecoy(int id) { return id == 999; }

static int settings_on = 1;
static MemoriesModCallback on_effect, on_monster;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; (void)key; (void)fallback; return settings_on; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_subscribe(const MemoriesModHost *h, unsigned type, int priority, MemoriesModCallback cb)
{
    (void)h; (void)priority;
    if (type == MEMORIES_EVENT_EFFECT) on_effect = cb;
    else if (type == MEMORIES_EVENT_MONSTER) on_monster = cb;
    else assert(0);
    return 1;
}
static int fake_card_id(const MemoriesModHost *h, const char *identity)
{
    (void)h;
    return !strcmp(identity, "modern-rules:change_of_heart:1") ? HEART
         : !strcmp(identity, "modern-rules:monster_reborn:1") ? REBORN : -1;
}
static const char *fake_text(const MemoriesModHost *h, const char *english) { (void)h; return english; }
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static int hand[8], taken[8], taken_count;
static int hand_slots(void) { return 8; }
static int hand_card(int slot) { return hand[slot]; }
static void take_hand(const int *cards, int count) { int i; for (i = 0; i < count; i++) taken[taken_count++] = cards[i]; }
static ExtraHandApi fake_ehp;

static void put(int r, int id, int deck, u16 flags)
{
    deck_card(deck)->id = (u16)id;
    deck_card(deck)->deck_index = (u8)deck;
    func_80024D34(r, deck);
    D_801A7AD8[r].flags |= flags;
}
static int occupied_at(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }
static void destroyed(int r)
{
    MemoriesModEvent e = {MEMORIES_EVENT_MONSTER, MEMORIES_AFTER, D_801A7AD8[r].card_id, r, MEMORIES_MONSTER_DESTROYED, 0, 0};
    on_monster(&e);
    D_801A7AD8[r].flags = 0;
}
/* The player's magic scene: its two calls, then the update each frame. */
static void play(int card)
{
    MemoriesModEvent e = {MEMORIES_EVENT_EFFECT, MEMORIES_BEFORE, card, 0, 0, 0, 0};
    on_effect(&e);
    assert(e.handled);
    e.handled = 0; e.b = 1;
    on_effect(&e);
    assert(e.handled);
}
static int update(u16 buttons)
{
    MemoriesModEvent e = {MEMORIES_EVENT_EFFECT, MEMORIES_BEFORE, 0, 0, 1, 0, 0};
    gInput_wPad1Pressed = buttons;
    on_effect(&e);
    return e.handled ? e.result : -1;
}

int main(void)
{
    {   /* base ATK and DEF: the id times ten, as Duel_CalcCardStats here */
        int i;
        for (i = 0; i < 511; i++) gDuel_adwCardStats[i] = (i + 1) | (i + 1) << CARD_STAT_DEFENSE_SHIFT;
    }
    fake_host.api = 23;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.subscribe = fake_subscribe;
    fake_host.card_id = fake_card_id;
    fake_host.text = fake_text;
    fake_ehp.version = 8;
    fake_ehp.hand_slots = hand_slots;
    fake_ehp.hand_card = hand_card;
    fake_ehp.take_hand = take_hand;
    assert(Spells_Install(&fake_host, &fake_mod, &fake_ehp) && on_effect && on_monster && fake_mod.overlay == overlay);

    /* Nothing to take, nothing in the graveyard: both refused. */
    D_8009B1D5 = 0;
    assert(Spells_Refuses(HEART, 0) && Spells_Refuses(REBORN, 0) && !Spells_Refuses(26, 0));
    /* The CPU's two monsters (records 20, 22), the player's one (5). */
    put(20, 150, 40, 0);
    put(22, 200, 42, DUEL_CARD_FLAG_DEFENSE_POSITION);
    put(5, 100, 0, 0);
    assert(!Spells_Refuses(HEART, 0) && Spells_Refuses(REBORN, 0));

    /* Change of Heart: the pick starts on the strongest (200, column 2),
     * left goes to 150; the cross takes it to the player's first free zone
     * (6), as it was. */
    play(HEART);
    assert(S.picking && S.at == 2 && Spells_Aimed(22) && !Spells_Aimed(20));
    assert(update(0) == 1);
    assert(update(PAD_DIRECTION_LEFT) == 1 && S.at == 0 && Spells_Aimed(20));
    assert(update(PAD_DIRECTION_RIGHT) == 1 && S.at == 2);
    assert(update(PAD_BUTTON_CROSS) == 0 && !S.picking);
    assert(!occupied_at(22) && occupied_at(6) && D_801A7AD8[6].card_id == 200);
    assert(D_801A7AD8[6].flags & DUEL_CARD_FLAG_DEFENSE_POSITION);
    assert(placed_from == 22 && harpies_moved == 1 && equips_moved == 1 && S.lent_count == 1);
    assert(update(PAD_BUTTON_CROSS) == -1);   /* no pick: the update is the game's */
    /* The next turn: it goes back to its zone. */
    D_800E9FF0[1].rank.turns_taken = 1;
    Spells_TurnStarts(1);
    assert(!Spells_Hold() && occupied_at(22) && D_801A7AD8[22].card_id == 200 && !occupied_at(6) && !S.lent_count);
    /* Taken again, its zone filled meanwhile: another free zone. */
    play(HEART);
    update(PAD_BUTTON_CROSS);
    assert(occupied_at(6) && !occupied_at(22));
    put(22, 300, 43, 0);
    Spells_TurnStarts(1);
    Spells_Hold();
    assert(D_801A7AD8[21].card_id == 200 && D_801A7AD8[22].card_id == 300);
    /* Taken, the owner's row full: destroyed. */
    play(HEART);
    update(PAD_BUTTON_CROSS);   /* the strongest: 300 */
    assert(D_801A7AD8[6].card_id == 300);
    put(22, 210, 44, 0); put(23, 211, 45, 0); put(24, 212, 46, 0);
    removed = 0;
    Spells_TurnStarts(1);
    Spells_Hold();
    assert(!occupied_at(6) && removed == 1);

    /* The graveyard: destroyed monsters, their owners; a lent one's owner
     * is the side it was taken from. */
    destroyed(24);                                   /* the CPU's 212 */
    assert(S.grave_count == 1 && S.grave_owner[0] == 1 && S.grave_card[0] == 212);
    play(HEART);
    update(PAD_DIRECTION_LEFT); update(PAD_BUTTON_CROSS);
    destroyed(6);                                    /* taken, then destroyed */
    assert(S.grave_count == 2 && S.grave_owner[1] == 1);
    Spells_TurnStarts(1);
    Spells_Hold();
    assert(!Spells_Refuses(REBORN, 0));
    /* Monster Reborn: the strongest first; it comes back face up in attack
     * on the player's side, with the ritual's light, and leaves the
     * graveyard. */
    play(REBORN);
    assert(S.picking && S.kind == PICK_REBORN && S.grave_card[(int)S.at] == 212);
    requests = 0;
    assert(update(PAD_BUTTON_CROSS) == 0);
    assert(occupied_at(6) && D_801A7AD8[6].card_id == 212 &&
           !(D_801A7AD8[6].flags & (DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_DEFENSE_POSITION)));
    assert(S.grave_count == 1 && requests == 1);

    /* The CPU: Change of Heart on the player's strongest when it is
     * stronger than all of its own. */
    memset(D_801A7AD8, 0, sizeof D_801A7AD8);
    put(5, 300, 1, 0);
    put(20, 100, 41, 0);
    deck_card(DECK_SIZE + 1)->id = 336;   /* the CPU's own deck card at the player's index: Dark Hole */
    D_8009B1D5 = 1;
    hand[3] = HEART;
    put(15 + 3, HEART, 45, 0);   /* its Change of Heart, CPU deck card 45, in hand slot 3 */
    D_801A7AD8[18].flags = 0;    /* a hand record, not on the field */
    assert(Spells_Cpu() && taken_count == 1 && taken[0] == 3 && D_801A7AD8[21].card_id == 300 && !occupied_at(5));
    /* The player's card stands as the spent Change of Heart's deck card (45),
     * not as the CPU's own card at the player's index (41, the Dark Hole). */
    assert(D_801A7AD8[21].data == deck_card(45) && deck_card(45)->id == 300 && S.lent_home[0] == 1);
    /* The next turn it goes back as its own card. */
    D_800E9FF0[0].rank.turns_taken = 9;
    Spells_TurnStarts(0);
    Spells_Hold();
    assert(occupied_at(5) && D_801A7AD8[5].data == deck_card(1) && D_801A7AD8[5].card_id == 300 && !occupied_at(21));
    D_801A7AD8[5].flags = 0;
    put(5, 300, 1, 0);
    /* Weaker than its own: not played. */
    hand[3] = 0; hand[4] = HEART;
    put(5, 50, 2, 0);
    assert(!Spells_Cpu() && taken_count == 1);
    /* Monster Reborn on the graveyard's strongest (210), stronger than its
     * own. */
    D_801A7AD8[21].flags = 0;
    hand[4] = REBORN;
    put(15 + 4, REBORN, 46, 0);
    D_801A7AD8[19].flags = 0;
    assert(Spells_Cpu() && taken_count == 2 && taken[1] == 4 && D_801A7AD8[21].card_id == 210 && occupied_at(21));
    assert(D_801A7AD8[21].data == deck_card(44));   /* its own card (CPU deck 44): nothing lent */
    /* A monster of the player's revived on the CPU's side stands as the
     * spent Reborn's deck card. */
    D_801A7AD8[21].flags = 0;
    S.grave_count = 1;
    S.grave_card[0] = 260; S.grave_deck[0] = 3; S.grave_owner[0] = 0;
    deck_card(3)->id = 260; deck_card(3)->deck_index = 3;
    deck_card(DECK_SIZE + 3)->id = 336;
    put(15 + 4, REBORN, 46, 0);
    D_801A7AD8[19].flags = 0;
    hand[4] = REBORN;
    assert(Spells_Cpu() && D_801A7AD8[21].card_id == 260 && D_801A7AD8[21].data == deck_card(46));
    /* A new duel: the graveyard is empty. */
    memset(D_800E9FF0, 0, sizeof D_800E9FF0);
    Spells_TurnStarts(0);
    assert(!S.grave_count && !S.lent_count);
    /* Off: the cards do nothing and are not refused. */
    settings_on = 0;
    assert(!Spells_Refuses(HEART, 0) && !Spells_Refuses(REBORN, 0) && !Spells_Cpu());
    puts("Change of Heart and Monster Reborn: the pick, taken and given back (its zone, another, destroyed), the graveyard and its owners, revived, the CPU, a new duel, off");
    return 0;
}
