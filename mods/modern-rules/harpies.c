/* Mai's harpies, as in her duels with Yugi and Joey (the setting
 * "harpies"):
 * - Elegant Egotist put onto Harpie Lady, or the two fused from the hand,
 *   makes Harpie Lady Sisters: the one harpy splits into three. On the disc
 *   it is an equip of Harpie Lady's; here the fusion answers first, so the
 *   placement fuses them (the fusion's own light and sound) and the equip
 *   leaves no card in a magic and trap zone.
 * - Harpie's Pet Dragon gains 300 ATK for each harpy on its side: Harpie
 *   Lady counts one, Harpie Lady Sisters three. The bonus goes into the
 *   record's stat_modifier and follows the field up and down; it is brought
 *   up to date while the hand or the field waits (Harpies_Update), never in
 *   a battle, which lifts its monsters off their records. */
#include "types.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_deck_card.h"
#include "unmatched.h"
#include "harpies.h"
#include "state.h"
#include <string.h>

#define H (gModernRules_State.harpies)
#define HARPIE_LADY 62
#define HARPIE_SISTERS 63
#define PET_DRAGON 386
#define ELEGANT_EGOTIST 318
#define PET_BONUS 300
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))

static const MemoriesModHost *host;

static int on(void) { return host && host->applied(host) && host->setting(host, "harpies", 1); }
static int deck_of(DuelCardRecord *card) { return card->data ? (u8)((DuelDeckCardRecord *)card->data)->deck_index : 0xFF; }

static void fusion(MemoriesModEvent *e)
{
    if (e->phase != MEMORIES_BEFORE || !on()) return;
    if ((e->a == HARPIE_LADY && e->b == ELEGANT_EGOTIST) || (e->a == ELEGANT_EGOTIST && e->b == HARPIE_LADY)) {
        e->result = HARPIE_SISTERS;
        e->handled = 1;
    }
}

/* The harpies on a side: Harpie Lady one, Harpie Lady Sisters three. */
static int harpies(int side)
{
    int col, n = 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        DuelCardRecord *card = &D_801A7AD8[MONSTER_RECORD(side, col)];
        if (!(card->flags & DUEL_CARD_FLAG_OCCUPIED)) continue;
        if (card->card_id == HARPIE_LADY) n += 1;
        else if (card->card_id == HARPIE_SISTERS) n += 3;
    }
    return n;
}

void Harpies_Update(void)
{
    int side, col;
    for (side = 0; side < 2; side++) {
        int want = on() ? PET_BONUS * harpies(side) : 0;
        for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
            int z = side * DUEL_FIELD_ROW_SIZE + col;
            DuelCardRecord *card = &D_801A7AD8[MONSTER_RECORD(side, col)];
            int here = (card->flags & DUEL_CARD_FLAG_OCCUPIED) && card->card_id == PET_DRAGON;
            /* The dragon given the bonus is gone (destroyed, fused): the
             * bonus went with it. */
            if (H.bonus[z] && (!here || deck_of(card) != H.deck[z])) H.bonus[z] = 0;
            if (!here || H.bonus[z] == want) continue;
            /* ATK only: stat_modifier is ATK's and DEF's alike, and
             * defense_modifier is what DEF gets beyond it. */
            card->stat_modifier = (s16)(card->stat_modifier + want - H.bonus[z]);
            card->defense_modifier = (s16)(card->defense_modifier - (want - H.bonus[z]));
            H.bonus[z] = (s16)want;
            H.deck[z] = (u8)deck_of(card);
        }
    }
}

int Harpies_Install(const MemoriesModHost *from)
{
    host = from;
    memset(&H, 0, sizeof H);
    if (host->api < 3) return 1;
    return host->subscribe(host, MEMORIES_EVENT_FUSION, 0, fusion) > 0;
}
