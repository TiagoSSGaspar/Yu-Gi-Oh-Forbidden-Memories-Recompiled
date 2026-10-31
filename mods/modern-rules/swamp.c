/* The swamp brothers, as in Joey's duel with Rex Raptor (the setting
 * "swamp_brothers"): with Swamp Battleguard and Lava Battleguard on the same
 * side, a monster attacking one of them meets both: the attacked one fights
 * in attack position with its brother's ATK added (1800 + 1550 = 3350), even
 * from defense. The weaker side loses its monster and the difference in
 * life points, as any battle of two monsters in attack position.
 *
 * The battle (DuelScene_UpdateBattle) lifts both monsters off their records
 * as it starts, keeping the attacked one's stat_modifier and flags
 * (D_8009B170[1], D_8009B178[1]), and puts them back from those before it
 * works out the battle a second time for the models. So the brother's ATK
 * goes into the record and into the kept values alike, and defense comes off
 * both; once the battle is over, a battleguard still there gets its own ATK
 * and position back. */
#include "types.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/display_object.h"
#include "game/display_object_work_slots.h"
#include "game/duel_scene_battle.h"
#include "game/duel_scene_state.h"
#define D_8009B170_AS_SIDE_ARRAY
#define D_8009B178_AS_SIDE_ARRAY
#include "game/duel_scene_field_actions.h"
#include "unmatched.h"
#include "swamp.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#define W (gModernRules_State.swamp)
#define SWAMP_BATTLEGUARD 12
#define LAVA_BATTLEGUARD 554
#define SCENE_MASK 0xF
#define SCENE_STARTED 0x8000
#define SCENE_BATTLE 9
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;
static void *original_battle;
static void (*next_overlay)(void);
static unsigned (*next_signature)(void);

static int on(void) { return host->applied(host) && host->setting(host, "swamp_brothers", 1); }
static int brother_of(int id)
{
    return id == SWAMP_BATTLEGUARD ? LAVA_BATTLEGUARD : id == LAVA_BATTLEGUARD ? SWAMP_BATTLEGUARD : 0;
}

/* The battle has begun: the attacked battleguard takes its brother's ATK. */
static void join(void)
{
    DisplayObject *attacked = D_800E9EF0[1];
    DuelCardRecord *card;
    int r, side, col, brother;
    W.record = -1;
    if (!attacked || !on()) return;
    r = attacked->field_6A;
    if (r < 0 || r >= DUEL_CARD_RECORD_COUNT) return;
    card = &D_801A7AD8[r];
    if (!(brother = brother_of(card->card_id))) return;
    side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        DuelCardRecord *other = &D_801A7AD8[MONSTER_RECORD(side, col)];
        if (MONSTER_RECORD(side, col) == r || !(other->flags & DUEL_CARD_FLAG_OCCUPIED) || other->card_id != brother)
            continue;
        W.record = (s8)r;
        W.card = card->card_id;
        W.boost = (s16)(Duel_CalcCardStats(other) & 0xFFFF);
        W.was_defense = (card->flags & DUEL_CARD_FLAG_DEFENSE_POSITION) != 0;
        card->stat_modifier = (s16)(card->stat_modifier + W.boost);
        D_8009B170[1] = (u16)(D_8009B170[1] + W.boost);
        card->flags &= (u16)~DUEL_CARD_FLAG_DEFENSE_POSITION;
        D_8009B178[1] &= (u16)~DUEL_CARD_FLAG_DEFENSE_POSITION;
        W.total = (s16)(Duel_CalcCardStats(card) & 0xFFFF);
        return;
    }
}

/* The battle is over: a battleguard still there gets its own back. */
static void part(void)
{
    DuelCardRecord *card = &D_801A7AD8[(int)W.record];
    if ((card->flags & DUEL_CARD_FLAG_OCCUPIED) && card->card_id == W.card) {
        card->stat_modifier = (s16)(card->stat_modifier - W.boost);
        if (W.was_defense) card->flags |= DUEL_CARD_FLAG_DEFENSE_POSITION;
        if (card->object) Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
    }
    W.record = -1;
}

static void battle(void)
{
    int starting = !(gDuel_wSceneStateFlags & SCENE_STARTED);
    ((void (*)(void))original_battle)();
    if (starting) join();
    else if (W.record >= 0 && (gDuel_wSceneStateFlags & SCENE_MASK) != SCENE_BATTLE) part();
}

/* The line while the brothers fight together. */
static const char *T(const char *english) { return host->api >= 22 ? host->text(host, english) : english; }
static int shown(void) { return W.record >= 0 && (gDuel_wSceneStateFlags & SCENE_MASK) == SCENE_BATTLE; }
static void overlay(void)
{
    char text[96];
    int width, height, scale, w, cx, cy;
    if (next_overlay) next_overlay();
    if (!shown()) return;
    snprintf(text, sizeof text, T("The swamp brothers join: %d"), W.total);
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
    return shown() ? s * 31u + 1u + (unsigned)W.total : s;
}

int Swamp_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    host = from;
    memset(&W, 0, sizeof W);
    W.record = -1;
    next_overlay = mod->overlay;
    next_signature = mod->overlay_signature;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    return host->hook(host, (void *)DuelScene_UpdateBattle, (void *)battle, &original_battle);
}
