/* Equips that stay on the field, as in the anime and the card game (the
 * setting "equips_stay"). On the disc an equip adds its bonus to the
 * monster's modifiers and is gone. Here it goes to the magic and trap zone
 * behind its monster (or the nearest free one; an equip used from the field
 * keeps its own zone), face up, and is tied to the monster:
 * - its bonus is measured while the placement plays it
 *   (DuelScene_UpdateCardPlacement, step 5 then 4): what the monster's
 *   stat_modifier and defense_modifier gained;
 * - a fusion, or a card thrown away, after it in the same chain takes it
 *   with the card it equipped, as on the disc;
 * - it lands when the monster lands in its zone (func_8001B170), turned
 *   round with the rest when Reverse Trap springs there;
 * - the equip leaving the field (Harpie's Feather Duster, a card put over
 *   it) takes its bonus off the monster; the monster leaving the field (a
 *   battle, Raigeki, a fusion onto it) destroys its equips, one explosion at
 *   a time, while the hand or the field waits (Equips_Hold). Looked at only
 *   while nothing is in play: a battle lifts its monsters off their records
 *   as it starts;
 * - an equip on the field is not used again: the player's confirm on it is
 *   refused (modern_rules.c), and the CPU sees it as used this turn;
 * - with the player's field cursor on an equip, its monster glows gold (and
 *   a line names it); on a monster, its equips do. The field's cards take
 *   their colour from their display objects each frame
 *   (Duel_DrawFieldCards), so the glow is put on for the drawing only.
 * With no free zone the equip cannot be played (modern_rules.c); should one
 * still come down, it keeps the disc's way. */
#include "types.h"
#include "game/ai.h"
#include "game/card_constants.h"
#include "game/display_object.h"
#include "game/display_object_work_slots.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_checks.h"
#include "game/duel_card_display_state.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_object_helpers.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_card_state_helpers.h"
#include "game/duel_deck_card.h"
#include "game/duel_display.h"
#include "game/duel_effect_request.h"
#include "game/duel_scene_callbacks.h"
#include "game/duel_scene_card_placement.h"
#include "game/duel_grid.h"
#include "game/duel_scene_state.h"
#include "game/duel_selection_layout.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/sound.h"
#include "pc/cards/cards.h"
#include "unmatched.h"
#include "equips.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#define E (gModernRules_State.equips)
#define SCENE_MASK 0xF
#define SCENE_STARTED 0x8000
#define SCENE_FIELD 5
#define SCENE_LANDING 8
#define STEP_ENTERED 0x80
#define STEP_COMBINE 3          /* the placement's steps: the next card comes in, */
#define STEP_EQUIP 5            /* an equip, */
#define STEP_FUSION 6           /* a fusion, */
#define STEP_THROWN 7           /* the card before thrown away, */
#define STEP_DONE 8             /* the result goes on */
#define LANDING_REVERSE 6       /* func_8001B170's step for Reverse Trap */
#define AI_SPELL_SLOT 6         /* gDuel_aActiveCards[6..10]: the side's magic and trap zones */
#define DESTROY_SOUND 0x1F
#define DESTROY_EFFECT 0xB
#define DESTROY_GAP 24          /* frames between two equips destroyed */
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))
#define SPELL_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + 2 * DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;
static void *original_placement, *original_landing, *original_snapshot, *original_draw;
static void (*next_overlay)(void);
static unsigned (*next_signature)(void);

int Equips_On(void) { return host && host->applied(host) && host->setting(host, "equips_stay", 1); }

static int occupied(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }
static int deck_of(DuelCardRecord *card) { return card->data ? (u8)((DuelDeckCardRecord *)card->data)->deck_index : 0xFF; }

/* Whether the link of record `r` still holds: the equip in its zone. */
static int equip_here(int r)
{
    return E.link[r].equip_card && occupied(r) && D_801A7AD8[r].card_id == E.link[r].equip_card;
}
/* And its monster in the monster's: the same card, the same deck card. */
static int monster_here(int r)
{
    DuelCardRecord *monster = &D_801A7AD8[(int)E.link[r].monster];
    return (monster->flags & DUEL_CARD_FLAG_OCCUPIED) && monster->card_id == E.link[r].monster_card &&
        deck_of(monster) == E.link[r].monster_deck;
}

int Equips_InZone(int side, int col)
{
    int r = SPELL_RECORD(side, col);
    return Equips_On() && equip_here(r) && monster_here(r);
}

int Equips_FreeZones(int side)
{
    int col, n = 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (!occupied(SPELL_RECORD(side, col))) n++;
    return n;
}

int Equips_Kept(int first, const int *ids, int count)
{
    int i, current = first, kept = 0, result;
    for (i = 0; i < count; i++) {
        int next = ids[i];
        if (!current) {
            current = next;
        } else if ((result = Duel_CheckFusion(current, next)) != 0) {
            current = result;
            kept = 0;
        } else if (Duel_CheckEquip(next, current)) {
            kept++;
        } else if (Duel_CheckEquip(current, next)) {
            kept++;
            current = next;
        } else {
            current = next;
            kept = 0;
        }
    }
    return kept;
}

/* --- the placement: each equip's bonus ---------------------------------- */

/* Step 5 has begun: the equip and the monster it goes onto are the
 * placement's two cards, the equip the one of its type. */
static void measure_start(void)
{
    DisplayObject *a = D_800E9EF0[0], *b = D_800E9EF0[1], *equip, *monster;
    int side = D_8009B1D5 & 1, col;
    if (!a || !b) return;
    equip = a->field_68 == CARD_TYPE_EQUIP ? a : b;
    monster = equip == a ? b : a;
    col = equip->field_6A - SPELL_RECORD(side, 0);
    E.measuring.card = D_8009B206;
    E.measuring.deck = equip->field_6B;
    E.measuring.from_col = (s8)(col >= 0 && col < DUEL_FIELD_ROW_SIZE ? col : -1);
    E.measured = (s8)monster->field_6A;
    E.before_attack = D_801A7AD8[monster->field_6A].stat_modifier;
    E.before_defense = D_801A7AD8[monster->field_6A].defense_modifier;
}

/* Step 4 is over: what the monster gained is the equip's. */
static void measure_end(void)
{
    DuelCardRecord *card;
    if (E.measured < 0) return;
    card = &D_801A7AD8[(int)E.measured];
    if (E.pending_count < EQUIPS_PENDING_MAX) {
        EquipPending *p = &E.pending[E.pending_count++];
        *p = E.measuring;
        p->attack = (s16)(card->stat_modifier - E.before_attack);
        p->defense = (s16)(card->defense_modifier - E.before_defense);
    }
    E.measured = -1;
}

static void forget_pending(void)
{
    E.pending_count = 0;
    E.measured = -1;
}

static void placement(void)
{
    int started = gDuel_wSceneStateFlags & SCENE_STARTED, step = D_8009B174 & 0xF;
    int entering = started && !(D_8009B174 & STEP_ENTERED);
    if (!Equips_On()) {
        ((void (*)(void))original_placement)();
        return;
    }
    if (!started) {
        forget_pending();
        E.reversed = 0;
    } else if (entering && (step == STEP_COMBINE || step == STEP_DONE)) {
        measure_end();
    } else if (entering && (step == STEP_FUSION || step == STEP_THROWN)) {
        forget_pending();
    }
    ((void (*)(void))original_placement)();
    if (entering && step == STEP_EQUIP) measure_start();
}

/* --- the landing: the equips to their zones ----------------------------- */

/* The free magic and trap zone of `side` nearest `col`, `col` first, the
 * left one first of two as near; -1 for none. */
static int free_zone(int side, int col)
{
    int d;
    for (d = 0; d < DUEL_FIELD_ROW_SIZE; d++) {
        if (col - d >= 0 && !occupied(SPELL_RECORD(side, col - d))) return col - d;
        if (d && col + d < DUEL_FIELD_ROW_SIZE && !occupied(SPELL_RECORD(side, col + d))) return col + d;
    }
    return -1;
}

static void land_pending(void)
{
    int side = D_8009B1D5 & 1, r = D_8009B19C, mcol = r - MONSTER_RECORD(side, 0), pass, i;
    DuelCardRecord *monster = &D_801A7AD8[r];
    if (mcol < 0 || mcol >= DUEL_FIELD_ROW_SIZE || !(monster->flags & DUEL_CARD_FLAG_OCCUPIED)) {
        forget_pending();
        return;
    }
    /* Equips used from the field first: their own zones are free again. */
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < E.pending_count; i++) {
            EquipPending *p = &E.pending[i];
            DuelCardRecord *card;
            EquipLink *link;
            int col, s;
            if ((p->from_col >= 0) != (pass == 0)) continue;
            if ((col = free_zone(side, p->from_col >= 0 ? p->from_col : mcol)) < 0) continue;
            s = SPELL_RECORD(side, col);
            func_80024D34(s, p->deck);
            card = &D_801A7AD8[s];
            card->flags = (u16)((card->flags | DUEL_CARD_FLAG_OCCUPIED) &
                                ~(DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_USED_THIS_TURN));
            Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
            link = &E.link[s];
            link->equip_card = card->card_id;
            link->monster_card = monster->card_id;
            link->monster = (s8)r;
            link->monster_deck = (u8)deck_of(monster);
            link->attack = (s16)(E.reversed ? -p->attack : p->attack);
            link->defense = (s16)(E.reversed ? -p->defense : p->defense);
        }
    }
    forget_pending();
    E.reversed = 0;
}

static void landing(void)
{
    ((void (*)(void))original_landing)();
    if (!Equips_On()) return;
    if ((gDuel_wSceneStateFlags & SCENE_MASK) == SCENE_LANDING && (D_8009B174 & 0xF) == LANDING_REVERSE)
        E.reversed = 1;
    if (gDuel_wSceneStateFlags == SCENE_FIELD && E.pending_count) land_pending();
}

/* --- the field: equips and monsters that left it ------------------------- */

static void destroy(DuelCardRecord *card)
{
    DisplayObject *object = (DisplayObject *)card->object;
    DuelEffectRequest *request = object ? DuelEffect_CreateRequest(DESTROY_EFFECT) : 0;
    if (request) {
        request->field_00 = (s16)object->field_30.h.field_30;
        request->field_02 = (s16)object->field_30.h.field_32;
        request->field_04 = object->field_34.h.field_34;
        request->field_1A = (s16)Duel_GetCardEffectVariant((CardObject *)object);
    }
    DuelCard_RemoveFromField(card);
    SD_SEPlayFull(DESTROY_SOUND);
}

int Equips_Hold(void)
{
    int r, busy = 0;
    if (!Equips_On()) return 0;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++) {
        EquipLink *link = &E.link[r];
        if (!link->equip_card) continue;
        if (!monster_here(r)) {
            if (!equip_here(r)) {
                link->equip_card = 0;
                continue;
            }
            busy = 1;
            if (E.cooldown) continue;
            destroy(&D_801A7AD8[r]);
            link->equip_card = 0;
            E.cooldown = DESTROY_GAP;
        } else if (!equip_here(r)) {
            DuelCardRecord *monster = &D_801A7AD8[(int)link->monster];
            monster->stat_modifier = (s16)(monster->stat_modifier - link->attack);
            monster->defense_modifier = (s16)(monster->defense_modifier - link->defense);
            link->equip_card = 0;
        }
    }
    if (E.cooldown) {
        E.cooldown--;
        busy = 1;
    }
    return busy;
}

/* --- the CPU: an equip on the field is used --------------------------- */

static void snapshot(void)
{
    int side = D_8009B1D5 & 1, col;
    ((void (*)(void))original_snapshot)();
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (Equips_InZone(side, col)) gDuel_aActiveCards[AI_SPELL_SLOT + col].flags |= DUEL_CARD_FLAG_USED_THIS_TURN;
}

/* --- the overlay: whose equip it is ------------------------------------ */

#define FIELD_IDLE 3
#define FIELD_CURSOR_RECORD 2
#define TEXT_Y 34

static const char *T(const char *english) { return host->api >= 22 ? host->text(host, english) : english; }

/* The player's field cursor's record, while the player walks the field;
 * -1 otherwise. */
static int cursor_record(void)
{
    DuelSelectionRecord *records;
    int side = D_8009B1D5 & 1, row, col;
    if (!Equips_On() || D_8009B360[side] >= 0 || (gDuel_wSceneStateFlags & SCENE_MASK) != SCENE_FIELD ||
        (D_8009B174 & 0xF) != FIELD_IDLE)
        return -1;
    records = (DuelSelectionRecord *)(D_800E9F10 + side * DUEL_SELECTION_SIDE_SIZE);
    row = records[FIELD_CURSOR_RECORD].row;
    col = records[FIELD_CURSOR_RECORD].col;
    if (col < 0 || col >= DUEL_FIELD_ROW_SIZE) return -1;
    if (row < 0 || row > 3) return -1;
    return D_800907D8[row * DUEL_FIELD_ROW_SIZE + col + side * DUEL_FIELD_SIDE_GRID_SLOT_COUNT];
}

static void name(int id, char *out, size_t size)
{
    if (!Cards_NameUtf8(id, out, size)) snprintf(out, size, "#%d", id);
}

/* The line for the record under the cursor: the monster an equip is on, or
 * a monster's equips; 0 for none. */
static int line_for(int r, char *text, size_t size)
{
    char card[48];
    int s, n = 0;
    size_t used;
    if (r < 0) return 0;
    if (E.link[r].equip_card && equip_here(r) && monster_here(r)) {
        name(E.link[r].monster_card, card, sizeof card);
        snprintf(text, size, T("Equipped to: %s"), card);
        return 1;
    }
    snprintf(text, size, "%s", T("Equipped with: "));
    for (s = 0; s < DUEL_CARD_RECORD_COUNT; s++) {
        if (!E.link[s].equip_card || E.link[s].monster != r || !equip_here(s) || !monster_here(s)) continue;
        name(E.link[s].equip_card, card, sizeof card);
        used = strlen(text);
        snprintf(text + used, size - used, "%s%s", n++ ? ", " : "", card);
    }
    return n > 0;
}

/* The records to glow for the record under the cursor: an equip's monster,
 * or a monster's equips. Their count. */
static int glowing(int r, u8 *lit)
{
    int s, n = 0;
    memset(lit, 0, DUEL_CARD_RECORD_COUNT);
    if (r < 0) return 0;
    if (E.link[r].equip_card && equip_here(r) && monster_here(r)) {
        lit[(int)E.link[r].monster] = 1;
        return 1;
    }
    for (s = 0; s < DUEL_CARD_RECORD_COUNT; s++)
        if (E.link[s].equip_card && E.link[s].monster == r && equip_here(s) && monster_here(s)) {
            lit[s] = 1;
            n++;
        }
    return n;
}

#define GLOW_PERIOD 48   /* frames of one pulse */
static unsigned glow_frame;

/* Gold, pulsing from a little over the cards' grey to full: CVECTOR order,
 * red in the low byte. */
static u32 glow_color(void)
{
    int phase = (int)(glow_frame % GLOW_PERIOD), k = phase < GLOW_PERIOD / 2 ? phase : GLOW_PERIOD - phase;
    int level = 8 + k;   /* 8..32 of 32 */
    u32 red = 0x80 + 0x60 * level / 32, green = 0x80 + 0x48 * level / 32, blue = 0x80 - 0x40 * level / 32;
    return red | green << 8 | blue << 16;
}

static void draw_field(void)
{
    u8 lit[DUEL_CARD_RECORD_COUNT];
    u32 saved[DUEL_CARD_RECORD_COUNT];
    int r;
    glow_frame++;
    if (!glowing(cursor_record(), lit)) {
        ((void (*)(void))original_draw)();
        return;
    }
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++) {
        DuelCardDisplayObject *object = (DuelCardDisplayObject *)D_801A7AD8[r].object;
        /* Only a card in its plain colour or dimmed: not one fading. */
        if (!lit[r] || !object || (object->color != DUEL_DISPLAY_COLOR_NORMAL &&
                                   object->color != DUEL_DISPLAY_COLOR_DIMMED)) {
            lit[r] = 0;
            continue;
        }
        saved[r] = object->color;
        object->color = glow_color();
    }
    ((void (*)(void))original_draw)();
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++)
        if (lit[r]) ((DuelCardDisplayObject *)D_801A7AD8[r].object)->color = saved[r];
}

static void picture_point(int x, int y, int *cx, int *cy)
{
    int width, height, scale;
    if (host->api >= 22) {
        host->picture_point(host, x, y, cx, cy);
        return;
    }
    host->overlay_size(host, &width, &height, &scale);
    *cx = (width - height * 4 / 3) / 2 + x * height / 240;
    *cy = y * height / 240;
}

static void overlay(void)
{
    char text[160];
    int width, height, scale, w, cx, cy;
    if (next_overlay) next_overlay();
    if (!line_for(cursor_record(), text, sizeof text)) return;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    picture_point(160, TEXT_Y, &cx, &cy);
    w = host->text_width(host, text, scale);
    host->fill(host, cx - w / 2 - 8 * scale, cy - 10 * scale, w + 16 * scale, 20 * scale, 0x000000, 160);
    host->draw_text(host, cx - w / 2, cy, text, 0xFFFFFF, scale);
}

static unsigned overlay_signature(void)
{
    char text[160];
    unsigned s = next_signature ? next_signature() : 0;
    const char *c;
    if (line_for(cursor_record(), text, sizeof text))
        for (s = s * 31u + 1u, c = text; *c; c++) s = s * 31u + (unsigned char)*c;
    return s;
}

int Equips_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    host = from;
    memset(&E, 0, sizeof E);
    E.measured = -1;
    next_overlay = mod->overlay;
    next_signature = mod->overlay_signature;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    return host->hook(host, (void *)DuelScene_UpdateCardPlacement, (void *)placement, &original_placement) &&
        host->hook(host, (void *)func_8001B170, (void *)landing, &original_landing) &&
        host->hook(host, (void *)func_80028220, (void *)snapshot, &original_snapshot) &&
        host->hook(host, (void *)Duel_DrawFieldCards, (void *)draw_field, &original_draw);
}
