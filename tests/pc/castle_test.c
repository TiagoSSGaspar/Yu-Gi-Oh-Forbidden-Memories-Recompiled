/* Compile for i386: Castle of Dark Illusions (Duelist Kingdom rules,
 * castle.c), against stand-ins for the game and the host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../mods/modern-rules/castle.c"

/* Change of Heart's pick is tested on its own (spells_test.c). */
int Spells_Aimed(int r) { (void)r; return 0; }

ModernRulesState gModernRules_State;
u16 gDuel_wSceneStateFlags;
u8 D_8009B174, D_8009B1D5;
s8 D_8009B360[2] = {-1, 0};
DuelSideState D_800E9FF0[DUEL_SIDE_COUNT];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
u8 D_800E9F10[DUEL_SELECTION_SIDE_SIZE * 2];
/* Side 0's grid as the player sees it: row 1 the opponent's monsters
 * (records 20-24), row 2 the player's (5-9). */
u8 D_800907D8[2 * DUEL_FIELD_SIDE_GRID_SLOT_COUNT] = {
    25, 26, 27, 28, 29, 20, 21, 22, 23, 24, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    10, 11, 12, 13, 14, 5, 6, 7, 8, 9, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29};
static DuelDeckCardRecord decks[40];
static DuelCardDisplayObject objects[DUEL_CARD_RECORD_COUNT];
/* The field drawn: the colour each card had then. */
static u32 drawn_color[DUEL_CARD_RECORD_COUNT];
void Duel_DrawFieldCards(void)
{
    int r;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++) drawn_color[r] = objects[r].color;
}
static void *hooked;
static int fake_hook(const MemoriesModHost *h, void *function, void *replacement, void **original)
{ (void)h; assert(function == (void *)Duel_DrawFieldCards); *original = function; hooked = replacement; return 1; }

static int castle_setting = 1, drawn;
static MemoriesModCallback subscribed;
static int fake_setting(const MemoriesModHost *h, const char *key, int fallback)
{ (void)h; return !strcmp(key, "dark_castle") ? castle_setting : fallback; }
static int fake_applied(const MemoriesModHost *h) { (void)h; return 1; }
static int fake_subscribe(const MemoriesModHost *h, unsigned type, int priority, MemoriesModCallback cb)
{ (void)h; (void)priority; assert(type == MEMORIES_EVENT_EFFECT); subscribed = cb; return 1; }
static void fake_overlay_size(const MemoriesModHost *h, int *w, int *hh, int *scale)
{ (void)h; *w = 1280; *hh = 960; *scale = 2; }
static int fake_text_width(const MemoriesModHost *h, const char *t, int scale) { (void)h; return (int)strlen(t) * 6 * scale; }
static void fake_fill(const MemoriesModHost *h, int x, int y, int w, int hh, uint32_t rgb, unsigned a)
{ (void)h; (void)x; (void)y; (void)w; (void)hh; (void)rgb; (void)a; }
static void fake_draw_text(const MemoriesModHost *h, int x, int middle, const char *t, uint32_t rgb, int scale)
{ (void)h; (void)x; (void)middle; (void)rgb; (void)scale; (void)t; drawn++; }
static void fake_picture_point(const MemoriesModHost *h, int x, int y, int *cx, int *cy)
{ (void)h; *cx = x * 4; *cy = y * 4; }
static const char *fake_text(const MemoriesModHost *h, const char *english) { (void)h; return english; }
static MemoriesModHost fake_host;
static MemoriesMod fake_mod;

static void put(int side, int col, int id, int deck, int face_down)
{
    DuelCardRecord *card = &D_801A7AD8[MONSTER_RECORD(side, col)];
    memset(card, 0, sizeof *card);
    card->card_id = (s16)id;
    card->flags = (u16)(DUEL_CARD_FLAG_OCCUPIED | (face_down ? DUEL_CARD_FLAG_FACE_DOWN : 0));
    decks[deck].deck_index = (u8)deck;
    card->data = &decks[deck];
    objects[MONSTER_RECORD(side, col)].color = DUEL_DISPLAY_COLOR_NORMAL;
    card->object = &objects[MONSTER_RECORD(side, col)];
}
static void swords(int side)
{
    MemoriesModEvent e = {MEMORIES_EVENT_EFFECT, MEMORIES_AFTER, SWORDS, 0, 0, 0, 0};
    D_8009B1D5 = (u8)side;
    subscribed(&e);
}

int main(void)
{
    DuelSelectionRecord *target = (DuelSelectionRecord *)D_800E9F10 + 3;
    fake_host.api = 22;
    fake_host.setting = fake_setting;
    fake_host.applied = fake_applied;
    fake_host.subscribe = fake_subscribe;
    fake_host.hook = fake_hook;
    fake_host.overlay_size = fake_overlay_size;
    fake_host.text_width = fake_text_width;
    fake_host.fill = fake_fill;
    fake_host.draw_text = fake_draw_text;
    fake_host.picture_point = fake_picture_point;
    fake_host.text = fake_text;
    assert(Castle_Install(&fake_host, &fake_mod) && subscribed && fake_mod.overlay == overlay && hooked == (void *)draw_field);

    /* The CPU's castle face up, a monster beside it: that one is in the
     * dark, the castle is not; the player's side is not. */
    put(1, 0, DARK_CASTLE, 1, 0);
    put(1, 2, 26, 2, 0);
    put(0, 1, 26, 3, 0);
    assert(Castle_Hides(MONSTER_RECORD(1, 2)) && !Castle_Hides(MONSTER_RECORD(1, 0)));
    assert(!Castle_Hides(MONSTER_RECORD(0, 1)) && !Castle_Hides(MONSTER_RECORD(1, 3)));
    /* Drawn dark for the drawing only; the castle and the others not. */
    draw_field();
    assert(drawn_color[MONSTER_RECORD(1, 2)] == DARK_COLOR && objects[MONSTER_RECORD(1, 2)].color == DUEL_DISPLAY_COLOR_NORMAL);
    assert(drawn_color[MONSTER_RECORD(1, 0)] == DUEL_DISPLAY_COLOR_NORMAL &&
           drawn_color[MONSTER_RECORD(0, 1)] == DUEL_DISPLAY_COLOR_NORMAL);
    /* The player's attack: row 1, column 2 is record 22. */
    D_8009B1D5 = 0;
    assert(Castle_Refuses(1, 2) && !Castle_Refuses(1, 0) && !Castle_Refuses(2, 1));
    /* The line while the player aims there. */
    gDuel_wSceneStateFlags = SCENE_FIELD;
    D_8009B174 = FIELD_TARGETING;
    target->row = 1; target->col = 2;
    overlay();
    assert(drawn == 1);
    target->col = 0;
    overlay();
    assert(drawn == 1);
    /* Face down, the castle hides no one. */
    D_801A7AD8[MONSTER_RECORD(1, 0)].flags |= DUEL_CARD_FLAG_FACE_DOWN;
    assert(!Castle_Hides(MONSTER_RECORD(1, 2)));
    D_801A7AD8[MONSTER_RECORD(1, 0)].flags &= (u16)~DUEL_CARD_FLAG_FACE_DOWN;
    /* Swords of Revealing Light by the castle's own side: still dark; by
     * the other side: lit. */
    D_800E9FF0[1].rank.turns_taken = 3;
    swords(1);
    assert(Castle_Hides(MONSTER_RECORD(1, 2)));
    swords(0);
    assert(!Castle_Hides(MONSTER_RECORD(1, 2)));
    D_800E9FF0[1].rank.turns_taken = 5;
    assert(!Castle_Hides(MONSTER_RECORD(1, 2)));
    /* Another castle in its place: dark again. Another duel too. */
    put(1, 0, DARK_CASTLE, 7, 0);
    assert(Castle_Hides(MONSTER_RECORD(1, 2)));
    swords(0);
    assert(!Castle_Hides(MONSTER_RECORD(1, 2)));
    D_800E9FF0[1].rank.turns_taken = 1;
    assert(Castle_Hides(MONSTER_RECORD(1, 2)));
    /* The setting off. */
    castle_setting = 0;
    assert(!Castle_Hides(MONSTER_RECORD(1, 2)) && !Castle_Refuses(1, 2));
    puts("Castle of Dark Illusions: its side in the dark and drawn dark, the castle not, face down nothing, lit by the other side's Swords, dark again for another castle or duel, off");
    return 0;
}
