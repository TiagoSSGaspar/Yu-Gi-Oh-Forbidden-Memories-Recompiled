/* Mirror Force (the setting "mirror_force"; the card is this mod's, a copy
 * of Widespread Ruin): set on the field, it springs on any attack of the
 * other side's, as Widespread Ruin does (the disc's attack traps,
 * Duel_SelectAttackTrap). The disc's trap destroys the attacker and stops
 * the attack; the mirror sends the attack back on all of them, so once the
 * battle is over every other monster of the attacker's in attack position
 * is destroyed too, one explosion at a time, while the hand or the field
 * waits (Mirror_Hold). Off, the card is a Widespread Ruin. */
#include "types.h"
#include "game/display_object.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_object_helpers.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_effect_request.h"
#include "game/duel_side_state.h"
#include "game/duel_trap_resolution.h"
#include "game/sound.h"
#include "unmatched.h"
#include "mirror.h"
#include "state.h"
#include <string.h>

#define M (gModernRules_State.mirror)
#define WIDESPREAD_RUIN 686
#define DESTROY_SOUND 0x1F
#define DESTROY_EFFECT 0xB
#define DESTROY_GAP 24
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;
static void *original_select;
static int mirror_id;

static int on(void) { return host && host->applied(host) && host->setting(host, "mirror_force", 1); }
static int is_mirror(int id)
{
    if (mirror_id <= 0 && host->card_id) mirror_id = host->card_id(host, "modern-rules:mirror_force:1");
    return mirror_id > 0 && id == mirror_id;
}

/* The attack's trap picked: the mirror's is Widespread Ruin's, and its
 * record holds the mirror. The attacker's side is the one whose turn it is. */
static s32 select_trap(u8 *attacker)
{
    s32 sprang = ((s32 (*)(u8 *))original_select)(attacker);
    if (sprang && on() && D_8009B22A == WIDESPREAD_RUIN && D_8009B1B8 < DUEL_CARD_RECORD_COUNT &&
        is_mirror(D_801A7AD8[D_8009B1B8].card_id)) {
        M.side = (s8)(D_8009B1D5 & 1);
        M.cooldown = DESTROY_GAP;   /* after the trap's own explosion */
    }
    return sprang;
}

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

int Mirror_Hold(void)
{
    int col;
    if (M.side < 0) return 0;
    if (M.cooldown) {
        M.cooldown--;
        return 1;
    }
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        DuelCardRecord *card = &D_801A7AD8[MONSTER_RECORD(M.side, col)];
        if ((card->flags & DUEL_CARD_FLAG_OCCUPIED) && !(card->flags & DUEL_CARD_FLAG_DEFENSE_POSITION)) {
            destroy(card);
            M.cooldown = DESTROY_GAP;
            return 1;
        }
    }
    M.side = -1;
    return 0;
}

int Mirror_Install(const MemoriesModHost *from)
{
    host = from;
    mirror_id = 0;
    memset(&M, 0, sizeof M);
    M.side = -1;
    return host->hook(host, (void *)Duel_SelectAttackTrap, (void *)select_trap, &original_select);
}
