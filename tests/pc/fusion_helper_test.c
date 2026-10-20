/* View > Fusion helper (src/pc/cards/fusion_helper.c) over a hand of more
 * than five, as Extra hand plays keeps one: the slots past the fifth come
 * from FusionHelper_SetHand, laid out overlapped. Compile for i386. */
#include "../../src/pc/cards/fusion.c"
#include "../../src/pc/cards/fusion_helper.c"
#include <assert.h>

u8 D_8009B26C, D_8009B26E, D_8009B174, D_8009B1D5;
s8 D_8009B360[2];
u16 gDuel_wSceneStateFlags;
u8 gDuel_bEffectState, gDuel_bQuitDialogState;
u16 gDuel_wCardEffectFlags;
DuelHandSlot D_800EA030[HAND_SIZE];
DuelCardRecord D_801A7AD8[DUEL_CARD_RECORD_COUNT];
s32 gDuel_adwCardStats[800];

static int fusion_calls;
int Settings_Get(SettingId id) { return id == SET_FUSION_HELPER; }
int Mods_HasSubscribers(unsigned event) { (void)event; return 0; }
int Hooks_IsHooked(const void *function) { (void)function; return 0; }
int Platform_Widescreen(void) { return 0; }
int Cards_Valid(int id) { return id >= 1 && id <= 800; }
int Cards_Type(int id) { (void)id; return 0; }
int Cards_NameUtf8(int id, char *out, size_t size) { (void)id; (void)out; (void)size; return 0; }
/* Cards 6 and 7 make 723, and only they. */
int CardRules_Fusion(int a, int b)
{
    fusion_calls++;
    return (a == 6 && b == 7) || (a == 7 && b == 6) ? 723 : 0;
}
int CardRules_Equip(int equipment, int monster) { (void)equipment; (void)monster; return 0; }
void Tables_EquipBoost(int equip, int monster, int *attack, int *defense)
{ (void)equip; (void)monster; *attack = *defense = 500; }
int Tables_StatCap(int defense) { (void)defense; return 9999; }
int Tables_StatCapEither(void) { return 9999; }
s32 Duel_GetTerrainBoost(s32 type) { (void)type; return 0; }
s32 Duel_CheckFusion(s32 a, s32 b) { return CardRules_Fusion(a, b); }
s32 Duel_CheckEquip(s32 a, s32 b) { return CardRules_Equip(a, b); }
void DuelScene_UpdateCardPlacement(void) {}
s32 Duel_CalcCardStats(DuelCardRecord *card) { (void)card; return 0; }

/* What the helper draws: its text, with where it went. */
static struct { char text[512]; int x, middle; } drawn[32];
static int draws;
int OverlayText_Width(const char *text, int pixels) { return (int)strlen(text) * pixels / 2; }
void OverlayText_Blend(const MenuCanvas *canvas, int x, int y, uint32_t color, unsigned alpha)
{ (void)canvas; (void)x; (void)y; (void)color; (void)alpha; }
void OverlayText_Draw(MenuCanvas *canvas, int x, int middle, int right, const char *text, int pixels, uint32_t color)
{
    (void)canvas; (void)right; (void)pixels; (void)color;
    assert(draws < 32);
    snprintf(drawn[draws].text, sizeof(drawn[draws].text), "%s", text);
    drawn[draws].x = x;
    drawn[draws++].middle = middle;
}
static int find(const char *prefix)
{
    int i;
    for (i = 0; i < draws; i++) if (!strncmp(drawn[i].text, prefix, strlen(prefix))) return i;
    return -1;
}

static DisplayObject objects[10];
static DuelHandSlot extra[5];
static int hand_count;
static const void *slot_at(int slot)
{
    if (slot < HAND_SIZE) return &D_800EA030[slot];
    return slot < hand_count ? &extra[slot - HAND_SIZE] : NULL;
}
/* Extra hand plays' layout: across the width of five, each card at 14 +
 * 240 * slot / (count - 1). The cards past five use the other side's
 * records (15 up), as theirs do. */
static void deal(int count)
{
    int slot;
    memset(objects, 0, sizeof(objects));
    memset(extra, 0, sizeof(extra));
    memset(D_800EA030, 0, sizeof(D_800EA030));
    memset(D_801A7AD8, 0, sizeof(D_801A7AD8));
    hand_count = count;
    for (slot = 0; slot < count; slot++) {
        DuelHandSlot *pick = (DuelHandSlot *)slot_at(slot);
        int record = slot < HAND_SIZE ? slot : DUEL_CARD_SIDE_RECORD_COUNT + slot - HAND_SIZE;
        objects[slot].field_6A = (u8)record;
        objects[slot].field_30.h.field_30 = (u16)(14 + 240 * slot / (count - 1));
        objects[slot].field_30.h.field_32 = 170;
        pick->object = (u8 *)&objects[slot];
        D_801A7AD8[record].card_id = (s16)(slot + 1);
        D_801A7AD8[record].attack = 1000;
        D_801A7AD8[record].defense = 1000;
        D_801A7AD8[record].flags = DUEL_CARD_FLAG_OCCUPIED;
    }
}
static void mark(int slot, int order)
{
    DuelHandSlot *pick = (DuelHandSlot *)slot_at(slot);
    pick->child = (u8 *)&objects[9];
    pick->active_09 = (u8)order;
}
static void draw(void)
{
    static uint32_t pixels[640 * 480];
    MenuCanvas canvas = {pixels, 640, 640, 480, 1};
    int x, y, w, h;
    draws = 0;
    FusionHelper_Draw(&canvas, &x, &y, &w, &h);
}
/* A pick-order badge's window x: right of the card's left edge and left of
 * the next card's, so that it is on the part of the card left showing. */
static void badge_on(const char *number, int slot)
{
    int i = find(number), left = screen_x(objects[slot].field_30.h.field_30);
    int next = screen_x(objects[slot + 1].field_30.h.field_30);
    assert(i >= 0);
    assert(drawn[i].x > left && drawn[i].x < next);
}

int main(void)
{
    FusionHelper_Viewport(0, 0, 640, 480);
    D_8009B26C = 3; D_8009B26E = 0x81; D_8009B174 = 1;
    gDuel_wSceneStateFlags = 4;
    D_8009B360[0] = -1;
    gDuel_adwCardStats[723 - 1] = 3000 / CARD_STAT_SCALE;

    /* The disc's five: cards 1-5 make nothing. */
    deal(7);
    draw();
    assert(find("No fusions in hand") >= 0);

    /* Seven cards through Extra hand plays: 6 and 7 make 723, and the
     * badges sit on the parts of the overlapped cards left showing. */
    FusionHelper_SetHand(slot_at);
    draw();
    assert(find("Card 723   ATK 3000") >= 0);
    badge_on("1", 5);
    assert(find("2") >= 0);

    /* Marking the seventh first: the sixth is the next pick. */
    mark(6, 1);
    draw();
    assert(find("Card 723") >= 0 && find("1") < 0 && find("2") >= 0);
    badge_on("2", 5);
    mark(5, 2);
    draw();
    assert(find("Result: Card 723") >= 0);

    /* Ten cards: the plan is made once while hand and marks stay as they
     * are, and asks each pair's fusion about once. */
    deal(10);
    fusion_calls = 0;
    draw();
    assert(find("Card 723") >= 0);
    badge_on("1", 5);
    assert(fusion_calls > 0 && fusion_calls <= 2 * 10 * 10);
    fusion_calls = 0;
    draw();
    FusionHelper_Signature();
    assert(find("Card 723") >= 0 && fusion_calls == 0);

    /* Back to the five. */
    FusionHelper_SetHand(NULL);
    draw();
    assert(find("No fusions in hand") >= 0);
    puts("fusion helper: passed");
    return 0;
}
