/* Kuriboh and Multiply, as in Yugi's duel with Kaiba at Pegasus's castle
 * (the setting "multiply"). Multiply is an equip of Kuriboh's alone, added
 * by the manifest, with no bonus: it stays on the field as every equip does
 * (equips.c). As it lands, the free monster zones of its side fill with
 * Kuribohs, in defense, with the ritual's light and sound. While it is on
 * the field, every Kuriboh of its side that leaves the field comes back as
 * two, into the free zones: a wall only breaking Multiply ends. It is not
 * destroyed with its Kuriboh (equips.c ties it to another one instead).
 *
 * The new Kuribohs are made from a Kuriboh's deck card (func_80024D34, as
 * the moth's evolution remakes a record). The Kuribohs are counted while the
 * hand or the field is idle, as the equips are looked at. */
#include "types.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect_request.h"
#include "game/duel_scene_state.h"
#include "game/sound.h"
#include "unmatched.h"
#include "multiply.h"
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

static int kuribohs(int side)
{
    int col, n = 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        DuelCardRecord *card = &D_801A7AD8[MONSTER_RECORD(side, col)];
        if (!(card->flags & DUEL_CARD_FLAG_OCCUPIED) || card->card_id != KURIBOH) continue;
        n++;
        if (card->data) K.deck[side] = ((DuelDeckCardRecord *)card->data)->deck_index;
    }
    return n;
}

/* Whether a Multiply of `side` is on the field. */
static int active(int side)
{
    int col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (occupied(SPELL_RECORD(side, col)) && Multiply_Is(D_801A7AD8[SPELL_RECORD(side, col)].card_id)) return 1;
    return 0;
}

/* Up to `n` Kuribohs into the free monster zones of `side`; how many. */
static int spawn(int side, int n)
{
    int col, made = 0;
    if (K.deck[side] < 0) return 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE && made < n; col++) {
        int r = MONSTER_RECORD(side, col);
        DuelCardRecord *card = &D_801A7AD8[r];
        if (occupied(r)) continue;
        func_80024D34(r, K.deck[side]);
        card->flags = (u16)((card->flags | DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_DEFENSE_POSITION) &
                            ~(DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_USED_THIS_TURN));
        card->stat_modifier = card->defense_modifier = 0;
        if (card->object) Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
        made++;
    }
    if (made) {
        DuelEffectRequest *request = DuelEffect_CreateRequest(RITUAL_LIGHT);
        if (request) {
            request->field_00 = 0xA0;
            request->field_02 = 0x78;
            request->field_1A = 1;
        }
        SD_SEPlayFull(RITUAL_SOUND);
    }
    return made;
}

void Multiply_Landed(int monster)
{
    int side = monster / DUEL_CARD_SIDE_RECORD_COUNT;
    if (!on()) return;
    kuribohs(side);
    spawn(side, DUEL_FIELD_ROW_SIZE);
    K.kuribohs[side] = (u8)kuribohs(side);
}

int Multiply_Hold(void)
{
    int side;
    if (!on()) return 0;
    for (side = 0; side < 2; side++) {
        int now = kuribohs(side);
        if (now < K.kuribohs[side] && active(side)) now += spawn(side, 2 * (K.kuribohs[side] - now));
        K.kuribohs[side] = (u8)now;
    }
    return 0;
}

int Multiply_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    (void)mod;
    host = from;
    multiply_id = 0;
    memset(&K, 0, sizeof K);
    K.deck[0] = K.deck[1] = -1;
    return 1;
}
