/* Kuriboh and Multiply, as in Yugi's duel with Kaiba at Pegasus's castle
 * (the setting "multiply"; episode 24): Multiply makes a wall of hundreds of
 * Kuribohs. Blue-Eyes Ultimate Dragon attacks it: a Kuriboh dies, another
 * takes its place, and Yugi loses no life points. The wall guards the
 * Kuribohs alone: the Celtic Guardian Yugi summons at the end is attacked as
 * any monster is.
 *
 * Multiply is an equip of Kuriboh's alone, added by the manifest, with no
 * bonus: it stays on the field as every equip does (equips.c), and is not
 * destroyed with its Kuriboh. While it is on the field its Kuriboh stays in
 * defense, so a battle with it costs no life points, and a Kuriboh that
 * leaves the field is born again in its zone, with the ritual's light and
 * sound: the side always has one, so there is no direct attack on it. The
 * wall itself takes no monster zone and has no cards: with 3D Monsters it is
 * five more Kuribohs floating in front of the side's zones, more of the
 * Kuriboh's model (monsters_3d.h). It falls with Multiply. A second Multiply
 * on a side that has one cannot be played
 * (modern_rules.c).
 *
 * The new Kuriboh is made from the Kuriboh's deck card (func_80024D34, as
 * the moth's evolution remakes a record), while the hand or the field is
 * idle, as the equips are looked at (Multiply_Hold). */
#include "types.h"
#include "game/display_object.h"
#include "game/display_object_work_slots.h"
#include "psyq/libgte.h"
#include "game/duel_card.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect_request.h"
#include "game/duel_side_state.h"
#include "game/sound.h"
#include "unmatched.h"
#include "multiply.h"
#include "../3d-monsters/monsters_3d.h"
#include "state.h"
#include <string.h>

#define K (gModernRules_State.multiply)
#define KURIBOH 58
#define RITUAL_LIGHT 0x12
#define RITUAL_SOUND 2
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))
#define SPELL_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + 2 * DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;
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

/* The wall: five more Kuribohs floating in front of the side's zones, as
 * 3D Monsters' models (monsters_3d.h): no card under them, and no zone. */
#define CLONES 5
static const s16 clone_x[CLONES] = {-112, -56, 0, 56, 112};
static const s16 clone_z[CLONES] = {44, 58, 44, 58, 44};      /* from the monsters' row, toward the other side */
static const s16 clone_yaw[CLONES] = {96, -64, 0, 64, -96};  /* a little turned each */

/* Whether record `r` is a Kuriboh with its wall up: the Kuriboh of a side
 * with Multiply. */
static int walled(int r)
{
    int side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    return on() && r >= 0 && r < DUEL_CARD_RECORD_COUNT && D_801A7AD8[r].card_id == KURIBOH &&
           Multiply_Kuriboh(side) == r && wall(side) >= 0;
}

static int model_copies(int record, int x, int z, Monsters3dCopy *copies, int room)
{
    int side = record / DUEL_CARD_SIDE_RECORD_COUNT, i;
    (void)x;
    if (!walled(record)) return 0;
    for (i = 0; i < CLONES && i < room; i++) {
        copies[i].x = clone_x[i];
        copies[i].z = (short)(side ? z - clone_z[i] : z + clone_z[i]);
        copies[i].yaw = clone_yaw[i];
    }
    return i;
}

/* 3D Monsters, when it is loaded, is asked to draw them. */
static int models_asked;
static void ask_models(void)
{
    const Monsters3dApi *models;
    if (models_asked || host->api < 4 || !host->find) return;
    models = (const Monsters3dApi *)host->find(host, "3d-monsters:api");
    if (!models) return;
    models_asked = 1;
    if (models->version >= 1) models->set_copies(model_copies);
}

static void light(void);
static void defend(int r);
void Multiply_Landed(int monster)
{
    int side = monster / DUEL_CARD_SIDE_RECORD_COUNT;
    if (!on()) return;
    ask_models();
    /* The wall comes up: the ritual's light and sound; the Kuriboh to
     * defense. */
    K.zone[side] = (s8)monster;
    if (D_801A7AD8[monster].data) K.deck[side] = ((DuelDeckCardRecord *)D_801A7AD8[monster].data)->deck_index;
    defend(monster);
    light();
}

static void light(void)
{
    DuelEffectRequest *request = DuelEffect_CreateRequest(RITUAL_LIGHT);
    if (request) {
        request->field_00 = 0xA0;
        request->field_02 = 0x78;
        request->field_1A = 1;
    }
    SD_SEPlayFull(RITUAL_SOUND);
}

/* The wall's Kuriboh `r` in defense, face up. */
static void defend(int r)
{
    DuelCardRecord *card = &D_801A7AD8[r];
    if ((card->flags & (DUEL_CARD_FLAG_DEFENSE_POSITION | DUEL_CARD_FLAG_FACE_DOWN)) == DUEL_CARD_FLAG_DEFENSE_POSITION)
        return;
    card->flags = (u16)((card->flags | DUEL_CARD_FLAG_DEFENSE_POSITION) & ~DUEL_CARD_FLAG_FACE_DOWN);
    if (card->object) Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
}

int Multiply_Hold(void)
{
    int side, col;
    if (!on()) return 0;
    ask_models();
    for (side = 0; side < 2; side++) {
        int k = Multiply_Kuriboh(side);
        if (k >= 0) {
            K.zone[side] = (s8)k;
            if (D_801A7AD8[k].data) K.deck[side] = ((DuelDeckCardRecord *)D_801A7AD8[k].data)->deck_index;
            if (wall(side) >= 0) defend(k);
            continue;
        }
        if (wall(side) < 0 || K.deck[side] < 0) continue;
        /* The Kuriboh is gone: another is born, in its zone or a free one. */
        k = K.zone[side] >= 0 && !occupied(K.zone[side]) ? K.zone[side] : -1;
        for (col = 0; k < 0 && col < DUEL_FIELD_ROW_SIZE; col++)
            if (!occupied(MONSTER_RECORD(side, col))) k = MONSTER_RECORD(side, col);
        if (k < 0) continue;
        func_80024D34(k, K.deck[side]);
        D_801A7AD8[k].flags &= (u16)~DUEL_CARD_FLAG_USED_THIS_TURN;
        D_801A7AD8[k].stat_modifier = D_801A7AD8[k].defense_modifier = 0;
        defend(k);
        K.zone[side] = (s8)k;
        light();
    }
    return 0;
}

int Multiply_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    (void)mod;
    host = from;
    multiply_id = 0;
    models_asked = 0;
    memset(&K, 0, sizeof K);
    K.zone[0] = K.zone[1] = K.deck[0] = K.deck[1] = -1;
    ask_models();
    return 1;
}
