/* Compile for i386: equips that stay on the field (Duelist Kingdom rules,
 * equips.c), against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/equips.c"

#define EQUIP_LEGEND 301   /* Legendary Sword: equips the warriors */
#define EQUIP_OTHER 302
#define WARRIOR 3          /* a monster the equips equip */
#define FUSED 99           /* what WARRIOR and FUSE_PARTNER make */
#define FUSE_PARTNER 7

ModernRulesState gModernRules_State;
u16 gDuel_wSceneStateFlags;
u8 D_8009B174;
u8 D_8009B1D5;
u8 D_8009B19C;
s16 D_8009B206;
s8 D_8009B360[2] = {-1, 0};
u8 D_800E9F10[DUEL_SELECTION_SIDE_SIZE * 2];
u8 D_800907D8[DUEL_SIDE_COUNT * DUEL_FIELD_SIDE_GRID_SLOT_COUNT];
DisplayObject *D_800E9EF0[7];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
AiActiveCard gDuel_aActiveCards[64];
static DuelDeckCardRecord deck_cards[2][40];
static DisplayObject objects[DUEL_CARD_RECORD_COUNT], equip_object, monster_object;
static int sounds, requests, removed, remakes, placements, landings, snapshots;
static DuelEffectRequest effect_request;

void SD_SEPlayFull(u32 id) { (void)id; sounds++; }
DuelEffectRequest *DuelEffect_CreateRequest(s32 id) { (void)id; requests++; return &effect_request; }
s32 Duel_GetCardEffectVariant(CardObject *object) { (void)object; return 0; }
void Duel_ApplyCardObjectFlags(DuelCardDisplayObject *object) { (void)object; }
void DuelCard_RemoveFromField(DuelCardRecord *card) { removed++; card->flags = 0; }
s32 Duel_CheckFusion(s32 a, s32 b)
{ return (a == WARRIOR && b == FUSE_PARTNER) || (a == FUSE_PARTNER && b == WARRIOR) ? FUSED : 0; }
s32 Duel_CheckEquip(s32 equipment, s32 monster)
{ return (equipment == EQUIP_LEGEND || equipment == EQUIP_OTHER) && (monster == WARRIOR || monster == FUSED); }
int Cards_NameUtf8(int id, char *out, size_t size)
{
    snprintf(out, size, "%s", id == WARRIOR ? "Warrior" : id == EQUIP_LEGEND ? "Legendary Sword" :
             id == EQUIP_OTHER ? "Other Sword" : "?");
    return 1;
}
/* The game's remake of a record from its deck card: occupied, face down
 * (the mod turns it up), modifiers cleared. */
void func_80024D34(s32 record, s32 deck_index)
{
    DuelCardRecord *card = &D_801A7AD8[record];
    int side = record / DUEL_CARD_SIDE_RECORD_COUNT;
    remakes++;
    card->data = &deck_cards[side][deck_index];
    card->card_id = deck_cards[side][deck_index].id;
    card->flags = DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_FACE_DOWN;
    card->stat_modifier = card->defense_modifier = 0;
    card->object = &objects[record];
}
/* The game's three functions under the hooks: each frame they are called
 * as the tests set the state they would have reached. */
void DuelScene_UpdateCardPlacement(void)
{
    placements++;
    gDuel_wSceneStateFlags |= SCENE_STARTED;
    if ((D_8009B174 & 0xF) == STEP_EQUIP && !(D_8009B174 & STEP_ENTERED)) D_8009B174 |= 0xC0;
}
static int landing_reverse;
void func_8001B170(void)
{
    landings++;
    if (landing_reverse) {
        D_8009B174 = LANDING_REVERSE;
        landing_reverse = 0;
        return;
    }
    gDuel_wSceneStateFlags = SCENE_FIELD;
}
void func_80028220(void) { snapshots++; }
/* The field drawn: the colour each record's card is drawn with. */
static u32 drawn_color[DUEL_CARD_RECORD_COUNT];
void Duel_DrawFieldCards(void)
{
    int r;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++)
        drawn_color[r] = D_801A7AD8[r].object ? ((DuelCardDisplayObject *)D_801A7AD8[r].object)->color : 0;
}

static int equips_setting = 1, applied = 1;
static void *hooked[4];
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "equips_stay") ? equips_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return applied; }
static int fake_hook(const MemoriesModHost *h, void *function, void *replacement, void **original)
{
    static int n;
    (void)h;
    *original = function;
    hooked[n++ % 4] = replacement;
    return 1;
}
static char drawn[4][160];
static int drawn_count;
static void fake_overlay_size(const MemoriesModHost *h, int *w, int *hh, int *scale)
{ (void)h; *w = 1280; *hh = 960; *scale = 2; }
static int fake_text_width(const MemoriesModHost *h, const char *t, int scale) { (void)h; return (int)strlen(t) * 6 * scale; }
static void fake_fill(const MemoriesModHost *h, int x, int y, int w, int hh, uint32_t rgb, unsigned a)
{ (void)h; (void)x; (void)y; (void)w; (void)hh; (void)rgb; (void)a; }
static void fake_draw_text(const MemoriesModHost *h, int x, int middle, const char *t, uint32_t rgb, int scale)
{
    (void)h; (void)x; (void)middle; (void)rgb; (void)scale;
    assert(drawn_count < 4);
    snprintf(drawn[drawn_count++], sizeof drawn[0], "%s", t);
}
static void fake_picture_point(const MemoriesModHost *h, int x, int y, int *cx, int *cy)
{ (void)h; *cx = x * 4; *cy = y * 4; }
static const char *fake_text(const MemoriesModHost *h, const char *english)
{
    (void)h;
    if (!strcmp(english, "Equipped to: %s")) return "Equipado em: %s";
    return english;
}
static int earlier_overlays;
static void earlier_overlay(void) { earlier_overlays++; }
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static void clear(void)
{
    memset(D_801A7AD8, 0, sizeof D_801A7AD8);
    memset(&E, 0, sizeof E);
    E.measured = -1;
    D_8009B1D5 = 0;
}
/* A card in record r from deck card `deck` of the record's side. */
static void put(int r, int id, int deck)
{
    int side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    deck_cards[side][deck].id = (s16)id;
    deck_cards[side][deck].deck_index = (s8)deck;
    D_801A7AD8[r].card_id = (s16)id;
    D_801A7AD8[r].flags = DUEL_CARD_FLAG_OCCUPIED;
    D_801A7AD8[r].data = &deck_cards[side][deck];
    D_801A7AD8[r].object = &objects[r];
}
static void frame(int scene, int step)
{
    gDuel_wSceneStateFlags = (u16)scene;
    D_8009B174 = (u8)step;
    placement();
}
/* One placement as the game plays it: `equips` equips (deck cards 10 up,
 * from hand records 0 up, or the field zone `from_col`) onto the monster in
 * record `monster`, each giving `bonus`; then the monster lands in
 * `landing_record`. */
static void play(int monster, int equips, int bonus, int from_col, int landing_record, int reverse)
{
    int i, side = D_8009B1D5;
    frame(7, 0);   /* the placement starts */
    for (i = 0; i < equips; i++) {
        int deck = 10 + i;
        deck_cards[side][deck].id = EQUIP_LEGEND;
        deck_cards[side][deck].deck_index = (s8)deck;
        equip_object.field_68 = CARD_TYPE_EQUIP;
        equip_object.field_6A = (u8)(from_col >= 0 ? SPELL_RECORD(side, from_col) : side * DUEL_CARD_SIDE_RECORD_COUNT + i);
        equip_object.field_6B = (u8)deck;
        monster_object.field_68 = CARD_TYPE_WARRIOR;
        monster_object.field_6A = (u8)monster;
        D_800E9EF0[0] = &monster_object;
        D_800E9EF0[1] = &equip_object;
        D_8009B206 = EQUIP_LEGEND;
        frame(SCENE_STARTED | 7, STEP_COMBINE | STEP_ENTERED);
        frame(SCENE_STARTED | 7, STEP_EQUIP);
        D_801A7AD8[monster].stat_modifier += (s16)bonus;   /* step 4 climbs */
        D_801A7AD8[monster].defense_modifier += 0;
        frame(SCENE_STARTED | 7, 4 | STEP_ENTERED);
        frame(SCENE_STARTED | 7, STEP_COMBINE);   /* step 4 over */
    }
    frame(SCENE_STARTED | 7, STEP_DONE);
    /* func_8001B170 copies the record into its zone. */
    if (landing_record != monster) {
        D_801A7AD8[landing_record] = D_801A7AD8[monster];
        memset(&D_801A7AD8[monster], 0, sizeof D_801A7AD8[monster]);
    }
    D_8009B19C = (u8)landing_record;
    gDuel_wSceneStateFlags = SCENE_STARTED | SCENE_LANDING;
    landing_reverse = reverse;
    landing();
    if (reverse) {
        gDuel_wSceneStateFlags = SCENE_STARTED | SCENE_LANDING;
        D_801A7AD8[landing_record].stat_modifier -= (s16)(2 * bonus * equips);
        landing();
    }
}
static int linked(int col) { return Equips_InZone(D_8009B1D5, col); }
static int face_up(int r)
{
    return (D_801A7AD8[r].flags & (DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_FACE_DOWN)) == DUEL_CARD_FLAG_OCCUPIED;
}
static int hold_frames(void)
{
    int n = 0;
    while (Equips_Hold()) assert(++n < 1000);
    return n;
}

int main(void)
{
    int i, ids[4];
    for (i = 0; i < DUEL_SIDE_COUNT * DUEL_FIELD_SIDE_GRID_SLOT_COUNT; i++) {
        int side = i / DUEL_FIELD_SIDE_GRID_SLOT_COUNT, row = i % DUEL_FIELD_SIDE_GRID_SLOT_COUNT / 5, col = i % 5;
        D_800907D8[i] = (u8)(row == 2 ? MONSTER_RECORD(side, col) : row == 3 ? SPELL_RECORD(side, col) :
                             row == 1 ? MONSTER_RECORD(side ^ 1, 4 - col) : SPELL_RECORD(side ^ 1, 4 - col));
    }
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
    assert(Equips_Install(&fake_host, &fake_mod) && hooked[0] == (void *)placement &&
           hooked[1] == (void *)landing && hooked[2] == (void *)snapshot && hooked[3] == (void *)draw_field);
    assert(fake_mod.overlay == overlay);

    /* The equips that would stay: those after the last fusion. */
    ids[0] = WARRIOR; ids[1] = EQUIP_LEGEND; ids[2] = EQUIP_OTHER;
    assert(Equips_Kept(0, ids, 3) == 2);
    ids[0] = EQUIP_LEGEND; ids[1] = WARRIOR;
    assert(Equips_Kept(0, ids, 2) == 1);
    ids[0] = EQUIP_LEGEND; ids[1] = FUSE_PARTNER;
    assert(Equips_Kept(WARRIOR, ids, 2) == 0);   /* equip, then the fusion takes it */
    ids[0] = FUSE_PARTNER; ids[1] = EQUIP_LEGEND;
    assert(Equips_Kept(WARRIOR, ids, 2) == 1);   /* fusion, then the equip on what it made */

    /* From the hand: a warrior (hand record 0) and an equip, played to
     * zone 2. The equip lands face up behind it, tied to it with its 500. */
    clear();
    put(0, WARRIOR, 3);
    play(0, 1, 500, -1, MONSTER_RECORD(0, 2), 0);
    assert(face_up(SPELL_RECORD(0, 2)) && D_801A7AD8[SPELL_RECORD(0, 2)].card_id == EQUIP_LEGEND && linked(2));
    assert(E.link[SPELL_RECORD(0, 2)].attack == 500 && E.link[SPELL_RECORD(0, 2)].monster == MONSTER_RECORD(0, 2));
    assert(D_801A7AD8[MONSTER_RECORD(0, 2)].stat_modifier == 500 && E.pending_count == 0);

    /* Onto a monster on the field, its zone behind taken: the nearest free
     * one, the left first. A second equip: the next. */
    clear();
    put(MONSTER_RECORD(0, 2), WARRIOR, 3);
    put(SPELL_RECORD(0, 2), 50, 20);
    play(MONSTER_RECORD(0, 2), 2, 500, -1, MONSTER_RECORD(0, 2), 0);
    assert(linked(1) && linked(3) && !linked(2) && D_801A7AD8[MONSTER_RECORD(0, 2)].stat_modifier == 1000);
    assert(E.link[SPELL_RECORD(0, 1)].attack == 500 && E.link[SPELL_RECORD(0, 3)].attack == 500);

    /* Used from the field: it keeps its own zone. */
    clear();
    put(MONSTER_RECORD(0, 0), WARRIOR, 3);
    play(MONSTER_RECORD(0, 0), 1, 500, 4, MONSTER_RECORD(0, 0), 0);
    assert(linked(4) && !linked(0));

    /* An equip, then a fusion: the fusion takes it, nothing lands. */
    clear();
    put(0, WARRIOR, 3);
    frame(7, 0);
    E.pending_count = 1;
    frame(SCENE_STARTED | 7, STEP_FUSION);
    assert(E.pending_count == 0);
    frame(SCENE_STARTED | 7, 2);
    E.pending_count = 1;
    frame(SCENE_STARTED | 7, STEP_THROWN);   /* or the card thrown away */
    assert(E.pending_count == 0);

    /* Reverse Trap: the bonus turned round is what it gave. */
    clear();
    put(0, WARRIOR, 3);
    play(0, 1, 500, -1, MONSTER_RECORD(0, 1), 1);
    assert(linked(1) && E.link[SPELL_RECORD(0, 1)].attack == -500 && D_801A7AD8[MONSTER_RECORD(0, 1)].stat_modifier == -500);

    /* Off: nothing lands; the equip is gone as on the disc. */
    clear();
    equips_setting = 0;
    put(0, WARRIOR, 3);
    play(0, 1, 500, -1, MONSTER_RECORD(0, 2), 0);
    assert(!occupied(SPELL_RECORD(0, 2)) && !E.link[SPELL_RECORD(0, 2)].equip_card);
    equips_setting = 1;

    /* The equip destroyed (Harpie's Feather Duster): its bonus only comes off. */
    clear();
    put(MONSTER_RECORD(0, 2), WARRIOR, 3);
    play(MONSTER_RECORD(0, 2), 2, 500, -1, MONSTER_RECORD(0, 2), 0);
    D_801A7AD8[MONSTER_RECORD(0, 2)].stat_modifier += 200;   /* something else's */
    D_801A7AD8[SPELL_RECORD(0, 2)].flags = 0;
    assert(!Equips_Hold() && D_801A7AD8[MONSTER_RECORD(0, 2)].stat_modifier == 700 && !E.link[SPELL_RECORD(0, 2)].equip_card);
    assert(linked(1) && removed == 0);

    /* The monster destroyed (Raigeki): its equips explode one at a time, the
     * hand waiting meanwhile. */
    clear();
    put(MONSTER_RECORD(0, 2), WARRIOR, 3);
    play(MONSTER_RECORD(0, 2), 2, 500, -1, MONSTER_RECORD(0, 2), 0);
    D_801A7AD8[MONSTER_RECORD(0, 2)].flags = 0;
    removed = requests = sounds = 0;
    assert(Equips_Hold() && removed == 1 && requests == 1);
    assert(hold_frames() > DESTROY_GAP && removed == 2 && sounds == 2);
    assert(!linked(1) && !linked(2) && !occupied(SPELL_RECORD(0, 1)) && !occupied(SPELL_RECORD(0, 2)));

    /* A fusion onto it: the same record, another card: the equip explodes. */
    clear();
    put(MONSTER_RECORD(0, 3), WARRIOR, 3);
    play(MONSTER_RECORD(0, 3), 1, 500, -1, MONSTER_RECORD(0, 3), 0);
    D_801A7AD8[MONSTER_RECORD(0, 3)].card_id = FUSED;
    removed = 0;
    hold_frames();
    assert(removed == 1 && !linked(3));
    /* The same card again from another deck card: the equip explodes too. */
    clear();
    put(MONSTER_RECORD(0, 3), WARRIOR, 3);
    play(MONSTER_RECORD(0, 3), 1, 500, -1, MONSTER_RECORD(0, 3), 0);
    put(MONSTER_RECORD(0, 3), WARRIOR, 4);
    removed = 0;
    hold_frames();
    assert(removed == 1);

    /* The CPU's view: its equips on the field are used. */
    clear();
    D_8009B1D5 = 1;
    put(MONSTER_RECORD(1, 0), WARRIOR, 3);
    play(MONSTER_RECORD(1, 0), 1, 500, -1, MONSTER_RECORD(1, 0), 0);
    memset(gDuel_aActiveCards, 0, sizeof gDuel_aActiveCards);
    snapshot();
    assert(gDuel_aActiveCards[AI_SPELL_SLOT].flags & DUEL_CARD_FLAG_USED_THIS_TURN);
    assert(!(gDuel_aActiveCards[AI_SPELL_SLOT + 1].flags & DUEL_CARD_FLAG_USED_THIS_TURN));
    assert(Equips_FreeZones(1) == 4);

    /* Whose equip: the line under the player's field cursor. */
    clear();
    put(MONSTER_RECORD(0, 2), WARRIOR, 3);
    play(MONSTER_RECORD(0, 2), 2, 500, -1, MONSTER_RECORD(0, 2), 0);
    gDuel_wSceneStateFlags = SCENE_FIELD;
    D_8009B174 = FIELD_IDLE;
    {
        DuelSelectionRecord *cursor = (DuelSelectionRecord *)D_800E9F10 + FIELD_CURSOR_RECORD;
        cursor->row = 3;
        cursor->col = 2;
        drawn_count = 0;
        overlay();
        assert(earlier_overlays == 1 && drawn_count == 1 && !strcmp(drawn[0], "Equipado em: Warrior"));
        cursor->row = 2;
        drawn_count = 0;
        overlay();
        assert(drawn_count == 1 && !strcmp(drawn[0], "Equipped with: Legendary Sword, Legendary Sword"));
        /* The glow: on the monster, its two equips; drawn so, then back. */
        for (i = 0; i < DUEL_CARD_RECORD_COUNT; i++)
            if (D_801A7AD8[i].object) ((DuelCardDisplayObject *)D_801A7AD8[i].object)->color = DUEL_DISPLAY_COLOR_NORMAL;
        draw_field();
        assert(drawn_color[SPELL_RECORD(0, 1)] != DUEL_DISPLAY_COLOR_NORMAL &&
               drawn_color[SPELL_RECORD(0, 2)] != DUEL_DISPLAY_COLOR_NORMAL &&
               (drawn_color[SPELL_RECORD(0, 2)] & 0xFF) > 0x80 && drawn_color[MONSTER_RECORD(0, 2)] == DUEL_DISPLAY_COLOR_NORMAL);
        assert(((DuelCardDisplayObject *)D_801A7AD8[SPELL_RECORD(0, 1)].object)->color == DUEL_DISPLAY_COLOR_NORMAL);
        cursor->row = 3;                          /* on an equip: its monster */
        draw_field();
        assert(drawn_color[MONSTER_RECORD(0, 2)] != DUEL_DISPLAY_COLOR_NORMAL &&
               drawn_color[SPELL_RECORD(0, 1)] == DUEL_DISPLAY_COLOR_NORMAL);
        ((DuelCardDisplayObject *)D_801A7AD8[MONSTER_RECORD(0, 2)].object)->color = 0x203040;   /* fading */
        draw_field();
        assert(drawn_color[MONSTER_RECORD(0, 2)] == 0x203040);
        cursor->row = 2;
        cursor->col = 0;
        drawn_count = 0;
        overlay();
        assert(drawn_count == 0);
        draw_field();
        assert(drawn_color[SPELL_RECORD(0, 1)] == DUEL_DISPLAY_COLOR_NORMAL);
        D_8009B174 = 6;   /* picking an equip's monster, not walking the field */
        cursor->col = 2;
        overlay();
        assert(drawn_count == 0);
    }

    printf("Equips on the field: placement, landing, nearest zone, from the field, fusion, Reverse Trap, "
           "off, equip destroyed, monster destroyed, CPU view, whose equip, the glow passed\n");
    return 0;
}
