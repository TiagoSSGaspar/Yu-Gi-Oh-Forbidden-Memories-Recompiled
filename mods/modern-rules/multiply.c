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
 * defense, and cannot leave it to attack, so a battle with it costs no life
 * points, and a Kuriboh that leaves the field is born again in its zone,
 * with the ritual's light and sound: the side always has one, so there is no direct attack on it. The
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
#include "game/duel_card_can_act_this_turn.h"
#include "game/duel_card_object_helpers.h"
#include "game/duel_scene_field_actions.h"
#include "game/ai.h"
#include "game/ai_opponent_data.h"
#include "game/duel_grid.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect_request.h"
#include "game/duel_side_state.h"
#include "game/sound.h"
#include "unmatched.h"
#include "multiply.h"
#include "cpu.h"
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
static void *original_can_act;

static int on(void) { return host && host->applied(host) && host->setting(host, "multiply", 1); }

int Multiply_Is(int card)
{
    if (!on() || card <= 0) return 0;
    if (multiply_id <= 0 && host->card_id) multiply_id = host->card_id(host, "modern-rules:multiply:1");
    return multiply_id > 0 && card == multiply_id;
}

static int occupied(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }

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

/* The wall's Kuriboh: the one Multiply went onto, or the one born again in
 * its place; -1 when it is not on the field or the wall is down. */
int Multiply_Wall(int side)
{
    int k = K.zone[side];
    if (!on() || k < 0 || !occupied(k) || D_801A7AD8[k].card_id != KURIBOH || wall(side) < 0) return -1;
    return k;
}

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

/* Whether record `r` is the wall's Kuriboh. Another Kuriboh of the side is
 * a monster as any other. */
static int walled(int r)
{
    return r >= 0 && r < DUEL_CARD_RECORD_COUNT && Multiply_Wall(r / DUEL_CARD_SIDE_RECORD_COUNT) == r;
}

static s16 clone_z_at(int side, int z, int i) { return (s16)(side ? z - clone_z[i] : z + clone_z[i]); }

/* The clones, but the one that died, until another takes its place. */
static int model_copies(int record, int x, int z, Monsters3dCopy *copies, int room)
{
    int side = record / DUEL_CARD_SIDE_RECORD_COUNT, i, n = 0;
    (void)x;
    if (!walled(record)) return 0;
    if (K.dead_frames && !--K.dead_frames) K.dead = -1;
    for (i = 0; i < CLONES && n < room; i++) {
        if (i == K.dead) continue;
        copies[n].x = clone_x[i];
        copies[n].z = clone_z_at(side, z, i);
        copies[n].yaw = clone_yaw[i];
        n++;
    }
    return n;
}

/* The wall's Kuriboh does not leave defense: its position cannot be
 * changed (L1 and R1, DuelScene_UpdateFieldActions), and in defense it does
 * not attack. */
static int can_act(DuelCardRecord *card)
{
    if (walled((int)(card - D_801A7AD8))) return 0;
    return ((int (*)(DuelCardRecord *))original_can_act)(card);
}

/* 3D Monsters, when it is loaded, is asked to draw them. */
static int models_asked;   /* 3D Monsters draws the clones */
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
    K.learned[side ^ 1] = 0;   /* a new wall: the other side has to learn it */
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
        int k = K.zone[side];
        if (k < 0 || wall(side) < 0) continue;
        if (occupied(k) && D_801A7AD8[k].card_id == KURIBOH) {
            if (D_801A7AD8[k].data) K.deck[side] = ((DuelDeckCardRecord *)D_801A7AD8[k].data)->deck_index;
            defend(k);
            continue;
        }
        if (K.deck[side] < 0) continue;
        /* The wall's Kuriboh is gone: another is born, in its zone or, when
         * that has been taken, a free one. */
        if (occupied(k)) {
            for (k = -1, col = 0; k < 0 && col < DUEL_FIELD_ROW_SIZE; col++)
                if (!occupied(MONSTER_RECORD(side, col))) k = MONSTER_RECORD(side, col);
            if (k < 0) continue;
        }
        func_80024D34(k, K.deck[side]);
        D_801A7AD8[k].flags &= (u16)~DUEL_CARD_FLAG_USED_THIS_TURN;
        D_801A7AD8[k].stat_modifier = D_801A7AD8[k].defense_modifier = 0;
        defend(k);
        K.zone[side] = (s8)k;
        light();
    }
    return 0;
}

/* The battle (swamp.c's hook of DuelScene_UpdateBattle): an attack on the
 * wall's Kuriboh goes to the wall. The Kuriboh holds: its DEF is the
 * attacker's ATK for the battle, which destroys neither (the record and the
 * value the battle restores it from, gDuel_awSavedDefenseModifier, alike,
 * as swamp.c does the ATK). Once it is over a clone dies where it stands,
 * and another takes its place. */
#define DESTROY_EFFECT 0xB
#define DESTROY_SOUND 0x1F
#define CLONE_GONE 45   /* frames before another clone stands in its place */

void Multiply_BattleBegan(void)
{
    DisplayObject *attacker = D_800E9EF0[0], *attacked = D_800E9EF0[1];
    DuelCardRecord *kuriboh, *against;
    int r, a;
    K.guard = -1;
    if (!on() || !attacker || !attacked) return;
    r = attacked->field_6A;
    a = attacker->field_6A;
    /* The battle has lifted both off their records already (swamp.c): the
     * wall's Kuriboh by its zone and its card. */
    if (r < 0 || r >= DUEL_CARD_RECORD_COUNT || r != K.zone[r / DUEL_CARD_SIDE_RECORD_COUNT] ||
        D_801A7AD8[r].card_id != KURIBOH || wall(r / DUEL_CARD_SIDE_RECORD_COUNT) < 0 || a < 0 ||
        a >= DUEL_CARD_RECORD_COUNT)
        return;
    kuriboh = &D_801A7AD8[r];
    against = &D_801A7AD8[a];
    K.lift = (s16)(Duel_CalcBattleAttack(against, kuriboh) - Duel_CalcBattleDefense(kuriboh, against));
    kuriboh->defense_modifier = (s16)(kuriboh->defense_modifier + K.lift);
    gDuel_awSavedDefenseModifier[1] = (s16)(gDuel_awSavedDefenseModifier[1] + K.lift);
    K.guard = (s8)r;
    if (K.learned[a / DUEL_CARD_SIDE_RECORD_COUNT] < 255)
        K.learned[a / DUEL_CARD_SIDE_RECORD_COUNT]++;   /* it has seen the wall hold */
}

void Multiply_BattleOver(void)
{
    DuelCardRecord *kuriboh;
    DisplayObject *object;
    int side;
    if (K.guard < 0) return;
    kuriboh = &D_801A7AD8[(int)K.guard];
    side = K.guard / DUEL_CARD_SIDE_RECORD_COUNT;
    K.guard = -1;
    if (kuriboh->card_id != KURIBOH) return;
    kuriboh->defense_modifier = (s16)(kuriboh->defense_modifier - K.lift);
    if (!models_asked || !(object = (DisplayObject *)kuriboh->object)) return;
    /* A clone dies in its place. */
    K.dead = (s8)(K.next % CLONES);
    K.next = (u8)((K.next + 2) % CLONES);
    K.dead_frames = CLONE_GONE;
    {
        DuelEffectRequest *request = DuelEffect_CreateRequest(DESTROY_EFFECT);
        if (request) {
            request->field_00 = clone_x[(int)K.dead];
            request->field_02 = (s16)object->field_30.h.field_32;
            request->field_04 = clone_z_at(side, (s16)object->field_34.h.field_34, K.dead);
            request->field_1A = (s16)Duel_GetCardEffectVariant((CardObject *)object);
        }
    }
    SD_SEPlayFull(DESTROY_SOUND);
}

/* The CPU (cpu.c): the wall's Kuriboh, and how often the other side has
 * attacked the wall. */
int Multiply_Walled(int r) { return walled(r); }
int Multiply_Attacks(int side) { return K.learned[side & 1]; }

/* The bosses break the player's wall first, with Harpie's Feather Duster
 * when they hold it. */
#define HARPIES_FEATHER_DUSTER 672

int Multiply_CpuBreak(void)
{
    int side = D_8009B1D5 & 1, slot, col;
    if (!on() || Cpu_Level() != CPU_BOSS || wall(side ^ 1) < 0 || !(slot = Cpu_HandSlot(HARPIES_FEATHER_DUSTER)) ||
        (col = Cpu_FreeZone(1)) < 0)
        return 0;
    Cpu_Play(slot, col, 1);
    return 1;
}

/* The CPU raises a wall of its own: a boss as soon as it can, a middle one
 * when behind. Multiply onto its face-up Kuriboh, or that Kuriboh down
 * first (the next hand play brings Multiply). */
int Multiply_CpuRaise(void)
{
    int side = D_8009B1D5 & 1, level, multiply, col, kuriboh;
    Multiply_Is(1);   /* the card's id, looked up once */
    if (!on() || wall(side) >= 0 || (level = Cpu_Level()) == CPU_BEGINNER || (level == CPU_MIDDLE && !Cpu_Behind()) ||
        !(multiply = Cpu_HandSlot(multiply_id)))
        return 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        int r = D_800907D8[2 * DUEL_FIELD_ROW_SIZE + col + side * DUEL_FIELD_SIDE_GRID_SLOT_COUNT];
        if (occupied(r) && D_801A7AD8[r].card_id == KURIBOH && !(D_801A7AD8[r].flags & DUEL_CARD_FLAG_FACE_DOWN)) {
            if (Cpu_FreeZone(1) < 0) return 0;
            Cpu_Play(multiply, col, 0);
            return 1;
        }
    }
    if (!(kuriboh = Cpu_HandSlot(KURIBOH)) || (col = Cpu_FreeZone(0)) < 0) return 0;
    Cpu_Play(kuriboh, col, 0);
    return 1;
}

int Multiply_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    (void)mod;
    host = from;
    multiply_id = 0;
    models_asked = 0;
    memset(&K, 0, sizeof K);
    K.zone[0] = K.zone[1] = K.deck[0] = K.deck[1] = -1;
    K.guard = K.dead = -1;
    K.learned[0] = K.learned[1] = 0;
    ask_models();
    return host->hook(host, (void *)DuelCard_CanActThisTurn, (void *)can_act, &original_can_act);
}
