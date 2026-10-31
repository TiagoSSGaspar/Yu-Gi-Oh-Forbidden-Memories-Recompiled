/* Kuriboh and Multiply, as in Yugi's duel with Kaiba at Pegasus's castle
 * (the setting "multiply"; episode 24): Multiply makes a wall of hundreds of
 * Kuribohs, and every attack of Blue-Eyes Ultimate Dragon's on them hits the
 * wall, a Kuriboh gone and another in its place; nor does any attack reach
 * Yugi himself. The wall guards the Kuribohs and Yugi alone: the Celtic
 * Guardian he summons at the end is attacked as any monster is.
 *
 * Multiply is an equip of Kuriboh's alone, added by the manifest, with no
 * bonus: it stays on the field as every equip does (equips.c), and is not
 * destroyed with its Kuriboh. While it is on the field, every attack on a
 * Kuriboh of its side, and every direct attack on that side, is stopped by
 * the wall: the battle shows the Kuriboh (Multiply with none left)
 * springing as a trap does, with Fake Trap's way of stopping an attack (an
 * explosion on the target, nothing destroyed, no damage). The wall takes no
 * monster zone: it is drawn, nine more Kuribohs in two rows in front of
 * the Kuriboh, toward the other side (the field's cards are drawn one by one
 * by func_80015EF4 from their display object's place, 0x30 and 0x34, so the
 * Kuriboh is drawn again at those places). It falls with Multiply. A second
 * Multiply on a side that has one cannot be played (modern_rules.c).
 *
 * The battle (DuelScene_UpdateBattle) clears the trap that springs as it
 * starts and looks for one; once the opponent's trap, if any, has been
 * chosen, an attack on a Kuriboh of the wall's side, or a direct one on
 * it, gets Fake Trap's effect
 * (D_8009B22A) and the Kuriboh as the card shown (D_8009B1B8). The trap's
 * own presentation, which takes the card off the field, comes before the
 * battle, so nothing is taken. */
#include "types.h"
#include "game/display_object.h"
#include "game/display_object_work_slots.h"
#include "psyq/libgte.h"
#include "game/func_80015EF4.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_effect_request.h"
#include "game/duel_scene_battle.h"
#include "game/duel_scene_state.h"
#include "game/duel_side_state.h"
#include "game/sound.h"
#include "unmatched.h"
#include "multiply.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#define K (gModernRules_State.multiply)
#define KURIBOH 58
#define FAKE_TRAP 690
#define RITUAL_LIGHT 0x12
#define RITUAL_SOUND 2
#define SCENE_MASK 0xF
#define SCENE_STARTED 0x8000
#define SCENE_BATTLE 9
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))
#define SPELL_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + 2 * DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;
static void *original_battle, *original_draw_card;
static void (*next_overlay)(void);
static unsigned (*next_signature)(void);
static int multiply_id;

static int on(void) { return host && host->applied(host) && host->setting(host, "multiply", 1); }

int Multiply_Is(int card)
{
    if (!on() || card <= 0) return 0;
    if (multiply_id <= 0 && host->card_id) multiply_id = host->card_id(host, "modern-rules:multiply:1");
    return multiply_id > 0 && card == multiply_id;
}

static int occupied(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }

int Multiply_Kuriboh(int side)
{
    int col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (occupied(MONSTER_RECORD(side, col)) && D_801A7AD8[MONSTER_RECORD(side, col)].card_id == KURIBOH)
            return MONSTER_RECORD(side, col);
    return -1;
}

/* The record of a Multiply of `side` on the field, or -1. */
static int wall(int side)
{
    int col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (occupied(SPELL_RECORD(side, col)) && Multiply_Is(D_801A7AD8[SPELL_RECORD(side, col)].card_id))
            return SPELL_RECORD(side, col);
    return -1;
}

int Multiply_Up(int side) { return wall(side) >= 0; }

int Multiply_InChain(const int *ids, int count)
{
    int i;
    for (i = 0; i < count; i++)
        if (Multiply_Is(ids[i])) return 1;
    return 0;
}

/* The wall: the Kuriboh again at these places, from its own (x, and z
 * toward the other side), a little turned each. */
#define CLONES 9
static const s16 clone_x[CLONES] = {-70, -35, 0, 35, 70, -52, -17, 17, 52};
static const s16 clone_z[CLONES] = {40, 42, 38, 42, 40, 72, 70, 74, 70};
static const s8 clone_turn[CLONES] = {3, -2, 1, -3, 2, -1, 3, -2, 1};
#define WALL_HALF 70   /* the wall's middle stays this far from the field's edges */

static void draw_card(void *record, POLY_GT4 *prim, POLY_FT4 *sprite, s32 *color)
{
    DuelCardRecord *card = (DuelCardRecord *)record;
    u8 *object;
    s16 x, z, middle;
    u8 turn;
    int r = (int)(card - D_801A7AD8), side, i;
    ((void (*)(void *, POLY_GT4 *, POLY_FT4 *, s32 *))original_draw_card)(record, prim, sprite, color);
    if (r < 0 || r >= DUEL_CARD_RECORD_COUNT || !(object = (u8 *)card->object)) return;
    side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    if (card->card_id != KURIBOH || Multiply_Kuriboh(side) != r || wall(side) < 0) return;
    x = *(s16 *)(object + 0x30);
    z = *(s16 *)(object + 0x34);
    turn = object[0x21];
    middle = x < -WALL_HALF ? -WALL_HALF : x > WALL_HALF ? WALL_HALF : x;
    for (i = 0; i < CLONES; i++) {
        *(s16 *)(object + 0x30) = (s16)(middle + clone_x[i]);
        *(s16 *)(object + 0x34) = (s16)(side ? z - clone_z[i] : z + clone_z[i]);
        object[0x21] = (u8)(turn + clone_turn[i]);
        ((void (*)(void *, POLY_GT4 *, POLY_FT4 *, s32 *))original_draw_card)(record, prim, sprite, color);
    }
    *(s16 *)(object + 0x30) = x;
    *(s16 *)(object + 0x34) = z;
    object[0x21] = turn;
}

void Multiply_Landed(int monster)
{
    DuelEffectRequest *request;
    (void)monster;
    if (!on()) return;
    /* The wall comes up: the ritual's light and sound. */
    request = DuelEffect_CreateRequest(RITUAL_LIGHT);
    if (request) {
        request->field_00 = 0xA0;
        request->field_02 = 0x78;
        request->field_1A = 1;
    }
    SD_SEPlayFull(RITUAL_SOUND);
}

/* The battle has begun: an attack on a Kuriboh of the wall's side, or a
 * direct attack on that side, is stopped by it. */
static void block(void)
{
    DisplayObject *attacked = D_800E9EF0[1];
    int side, shown;
    K.blocking = 0;
    if (!on() || D_8009B22A || !D_800E9EF0[0]) return;
    if (attacked) {
        shown = attacked->field_6A;
        if (shown >= DUEL_CARD_RECORD_COUNT || D_801A7AD8[shown].card_id != KURIBOH) return;
        side = shown / DUEL_CARD_SIDE_RECORD_COUNT;
        if (wall(side) < 0) return;
    } else {
        side = (D_8009B1D5 & 1) ^ 1;
        if ((shown = wall(side)) < 0) return;
        if (Multiply_Kuriboh(side) >= 0) shown = Multiply_Kuriboh(side);
    }
    D_8009B22A = FAKE_TRAP;
    D_8009B1B8 = (u8)shown;
    K.blocking = 1;
}

static void battle(void)
{
    int starting = !(gDuel_wSceneStateFlags & SCENE_STARTED);
    ((void (*)(void))original_battle)();
    if (starting) block();
    else if ((gDuel_wSceneStateFlags & SCENE_MASK) != SCENE_BATTLE) K.blocking = 0;
}

/* The line while the wall stops an attack. */
static const char *T(const char *english) { return host->api >= 22 ? host->text(host, english) : english; }
static int shown(void) { return K.blocking && (gDuel_wSceneStateFlags & SCENE_MASK) == SCENE_BATTLE; }
static void overlay(void)
{
    char text[96];
    int width, height, scale, w, cx, cy;
    if (next_overlay) next_overlay();
    if (!shown()) return;
    snprintf(text, sizeof text, "%s", T("The wall of Kuribohs stops the attack"));
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    if (host->api >= 22) {
        host->picture_point(host, 160, 34, &cx, &cy);
    } else {
        cx = width / 2;
        cy = 34 * height / 240;
    }
    w = host->text_width(host, text, scale);
    host->fill(host, cx - w / 2 - 8 * scale, cy - 10 * scale, w + 16 * scale, 20 * scale, 0x000000, 160);
    host->draw_text(host, cx - w / 2, cy, text, 0xFFFFFF, scale);
}
static unsigned overlay_signature(void)
{
    unsigned s = next_signature ? next_signature() : 0;
    return shown() ? s * 31u + 7u : s;
}

int Multiply_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    host = from;
    multiply_id = 0;
    memset(&K, 0, sizeof K);
    next_overlay = mod->overlay;
    next_signature = mod->overlay_signature;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    return host->hook(host, (void *)DuelScene_UpdateBattle, (void *)battle, &original_battle) &&
        host->hook(host, (void *)func_80015EF4, (void *)draw_card, &original_draw_card);
}
