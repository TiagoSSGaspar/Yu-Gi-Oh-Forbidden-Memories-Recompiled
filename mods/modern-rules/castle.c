/* Castle of Dark Illusions, as in PaniK's duel with Yugi (the setting
 * "dark_castle"): the castle face up on the field wraps its side in
 * darkness, and its owner's other monsters cannot be picked for an attack;
 * the castle itself can. Swords of Revealing Light, used by the other side,
 * lights the field: that castle hides no one any more.
 * - The player's attack: the confirm on a monster in the dark is refused
 *   (modern_rules.c, field state 6) with a line saying why.
 * - The CPU's: an attack aimed into the dark goes to the monster it beats
 *   by the most, or nowhere (cpu.c, Cpu_Attack).
 * - The monsters in the dark are drawn dark: the cards for the field's
 *   drawing only (Duel_DrawFieldCards, as the equips' glow), the 3D models
 *   by arrow.c's tint.
 * The lit castle is known by its record, its deck card and the turn of its
 * owner's it was lit in: another castle, or another duel, is dark again. */
#include "types.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_display_state.h"
#include "game/duel_deck_card.h"
#include "game/duel_display.h"
#include "game/duel_grid.h"
#include "game/duel_scene_state.h"
#include "game/duel_selection_layout.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "unmatched.h"
#include "castle.h"
#include "spells.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#define C (gModernRules_State.castle)
#define DARK_CASTLE 83
#define SWORDS 348
#define SCENE_MASK 0xF
#define SCENE_FIELD 5
#define FIELD_TARGETING 6
#define THEIR_ROW 1
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;
static void *original_draw;
static void (*next_overlay)(void);
static unsigned (*next_signature)(void);

static int on(void) { return host && host->applied(host) && host->setting(host, "dark_castle", 1); }
static int deck_of(DuelCardRecord *card) { return card->data ? (u8)((DuelDeckCardRecord *)card->data)->deck_index : 0xFF; }
static int turns(int side) { return D_800E9FF0[side].rank.turns_taken; }

static int lit(int side, int r)
{
    return C.lit_record[side] == r && C.lit_deck[side] == deck_of(&D_801A7AD8[r]) && C.lit_turn[side] <= turns(side);
}

/* The side's face-up castle still dark, or -1. */
static int dark_castle(int side)
{
    int col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        int r = MONSTER_RECORD(side, col);
        DuelCardRecord *card = &D_801A7AD8[r];
        if ((card->flags & DUEL_CARD_FLAG_OCCUPIED) && !(card->flags & DUEL_CARD_FLAG_FACE_DOWN) &&
            card->card_id == DARK_CASTLE && !lit(side, r))
            return r;
    }
    return -1;
}

int Castle_Hides(int r)
{
    int side, at;
    if (!on() || r < 0 || r >= DUEL_CARD_RECORD_COUNT) return 0;
    side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    at = r % DUEL_CARD_SIDE_RECORD_COUNT;
    if (at < DUEL_FIELD_ROW_SIZE || at >= 2 * DUEL_FIELD_ROW_SIZE) return 0;
    if (!(D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) || D_801A7AD8[r].card_id == DARK_CASTLE) return 0;
    return dark_castle(side) >= 0;
}

int Castle_Refuses(int row, int col)
{
    int side = D_8009B1D5 & 1;
    if (row != THEIR_ROW || col < 0 || col >= DUEL_FIELD_ROW_SIZE) return 0;
    return Castle_Hides(D_800907D8[row * DUEL_FIELD_ROW_SIZE + col + side * DUEL_FIELD_SIDE_GRID_SLOT_COUNT]);
}

/* Swords of Revealing Light: the side whose turn it is lights the other
 * side's castle. */
static void effect(MemoriesModEvent *e)
{
    int side, r;
    if (e->phase != MEMORIES_AFTER || e->c || e->a != SWORDS || !on()) return;
    side = !(D_8009B1D5 & 1);
    if ((r = dark_castle(side)) < 0) return;
    C.lit_record[side] = (s8)r;
    C.lit_deck[side] = (u8)deck_of(&D_801A7AD8[r]);
    C.lit_turn[side] = (u8)turns(side);
}

/* The line while the player's attack aims into the dark. */
static const char *T(const char *english) { return host->api >= 23 ? host->text(host, english) : english; }
static int aiming_dark(void)
{
    DuelSelectionRecord *target;
    if ((gDuel_wSceneStateFlags & SCENE_MASK) != SCENE_FIELD || (D_8009B174 & 0xF) != FIELD_TARGETING ||
        (D_8009B1D5 & 1) || D_8009B360[0] >= 0)
        return 0;
    target = (DuelSelectionRecord *)(D_800E9F10 + 3 * DUEL_SELECTION_RECORD_SIZE);
    return Castle_Refuses(target->row, target->col);
}
static void overlay(void)
{
    const char *text;
    int width, height, scale, w, cx, cy;
    if (next_overlay) next_overlay();
    if (!aiming_dark()) return;
    text = T("In the dark: it cannot be attacked");
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    if (host->api >= 23) {
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
    return aiming_dark() ? s * 31u + 7u : s;
}

/* The cards in the dark drawn dark, and the one Change of Heart's pick is on
 * lit (spells.c): their display objects' colour put on for the drawing only, as the field takes it from them each frame. Only a
 * card in its plain colour or dimmed, not one fading. */
#define DARK_COLOR 0x342C2Cu   /* CVECTOR order, red in the low byte */
#define AIMED_COLOR 0xE0D060u  /* Change of Heart's pick (spells.c): a cold light */
static void draw_field(void)
{
    u32 saved[DUEL_CARD_RECORD_COUNT];
    u8 dark[DUEL_CARD_RECORD_COUNT];
    int r, any = 0;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++) {
        DuelCardDisplayObject *object = (DuelCardDisplayObject *)D_801A7AD8[r].object;
        dark[r] = 0;
        int aimed = Spells_Aimed(r);
        if (!object || (!aimed && !Castle_Hides(r)) ||
            (object->color != DUEL_DISPLAY_COLOR_NORMAL && object->color != DUEL_DISPLAY_COLOR_DIMMED))
            continue;
        saved[r] = object->color;
        object->color = aimed ? AIMED_COLOR
                      : object->color == DUEL_DISPLAY_COLOR_DIMMED ? DARK_COLOR / 2 & 0x7F7F7Fu : DARK_COLOR;
        dark[r] = 1;
        any = 1;
    }
    ((void (*)(void))original_draw)();
    if (!any) return;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++)
        if (dark[r]) ((DuelCardDisplayObject *)D_801A7AD8[r].object)->color = saved[r];
}

int Castle_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    host = from;
    memset(&C, 0, sizeof C);
    C.lit_record[0] = C.lit_record[1] = -1;
    next_overlay = mod->overlay;
    next_signature = mod->overlay_signature;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    if (!host->hook(host, (void *)Duel_DrawFieldCards, (void *)draw_field, &original_draw)) return 0;
    if (host->api < 3) return 1;
    return host->subscribe(host, MEMORIES_EVENT_EFFECT, 0, effect) > 0;
}
