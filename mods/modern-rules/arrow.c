/* Living Arrow, as in Yugi's duel with Kaiba at Pegasus's castle (the
 * setting "living_arrow"): Polymerization and Living Arrow fuse Mammoth
 * Graveyard into Blue-Eyes Ultimate Dragon, and the dead flesh rots the
 * living dragon from inside, 1200 ATK at the end of each of Yugi's turns,
 * 4500 down to 900. Only a living creature rots: a Zombie is dead already,
 * and a Machine or a Rock was never alive. Mammoth Graveyard, a Dinosaur on
 * the disc, is the anime's Zombie.
 *
 * The player marks Polymerization, Living Arrow and a Zombie in the hand
 * (any order) and confirms: instead of placement, they pick one of the
 * opponent's face-up living monsters (left and right; cross confirms,
 * circle cancels), refused with the cancel sound when there is none. The
 * three cards leave the hand (Extra hand plays' take_hand: the hand comes
 * back without them, a bonus hand) and the target is infected: at the end
 * of each turn of the side that infected it (the other side's turn
 * beginning, turn_draws) it loses the Zombie's ATK, down to 0, until it
 * leaves the field. The CPU plays the combo too, on the player's strongest
 * living monster when that is stronger than all of its own.
 *
 * Polymerization and Living Arrow are equips of no monster (mod.json): on
 * their own they cannot be played. An infected card is drawn green
 * (func_80015EF4, its display object's colour for the drawing only), and a
 * line says what happened. */
#include "types.h"
#include "game/card_constants.h"
#include "game/display_object.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_check_quit_input.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect.h"
#include "game/duel_scene_state.h"
#include "game/input.h"
#include "psyq/libgte.h"
#include "game/func_80015EF4.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/sound.h"
#include "pc/cards/cards.h"
#include "unmatched.h"
#include "arrow.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#define A (gModernRules_State.arrow)
#define STATE_HAND 4
#define STATE_FIELD 5
#define STATE_MASK 0xF
#define STATE_STARTED 0x8000
#define STATE_LOOKING 0x4000
#define HAND_IDLE 1
#define HAND_STEP_DONE 0x80
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))
#define ZONE_OF(r) (((r) / DUEL_CARD_SIDE_RECORD_COUNT) * DUEL_FIELD_ROW_SIZE + \
                    (r) % DUEL_CARD_SIDE_RECORD_COUNT - DUEL_FIELD_ROW_SIZE)
#define RECORD_OF(zone) MONSTER_RECORD((zone) / DUEL_FIELD_ROW_SIZE, (zone) % DUEL_FIELD_ROW_SIZE)
#define REFUSED_SOUND 8
#define MOVE_SOUND 0xB
#define ARROW_SOUND 0x1F
#define SAID_FRAMES 180
#define INFECTED_COLOR 0x40A040u
#define TEXT_Y 52

static const MemoriesModHost *host;
static const ExtraHandApi *ehp;
static void *original_draw_card;
static void (*next_overlay)(void);
static unsigned (*next_signature)(void);
static int polymerization_id, arrow_id;

static int on(void)
{
    return host && host->applied(host) && host->setting(host, "living_arrow", 1) && ehp && ehp->version >= 8;
}

static int card_id(const char *identity, int *cached)
{
    if (*cached <= 0 && host->card_id) *cached = host->card_id(host, identity);
    return *cached;
}
static int is_polymerization(int id) { return id > 0 && id == card_id("modern-rules:polymerization:1", &polymerization_id); }
static int is_arrow(int id) { return id > 0 && id == card_id("modern-rules:living_arrow:1", &arrow_id); }

static int kind(int id) { return (gDuel_adwCardStats[id - 1] >> CARD_STAT_TYPE_SHIFT) & CARD_STAT_TYPE_MASK; }
static int base_attack(int id) { return (gDuel_adwCardStats[id - 1] & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE; }
/* A Zombie; Mammoth Graveyard too, the anime's own, a Dinosaur on the disc. */
#define MAMMOTH_GRAVEYARD 59
static int zombie(int id) { return id > 0 && (kind(id) == CARD_TYPE_ZOMBIE || id == MAMMOTH_GRAVEYARD); }
/* A living creature: a monster, not a Zombie, a Machine or a Rock. */
static int living(int id)
{
    int k;
    if (id <= 0 || zombie(id)) return 0;
    k = kind(id);
    return k < CARD_TYPE_MAGIC && k != CARD_TYPE_MACHINE && k != CARD_TYPE_ROCK;
}
static int attack_of(int r) { return Duel_CalcCardStats(&D_801A7AD8[r]) & 0xFFFF; }
static int deck_of(const DuelCardRecord *card)
{
    return card->data ? ((DuelDeckCardRecord *)card->data)->deck_index : -1;
}

/* A target of `side`'s combo: the other side's face-up living monster. */
static int target(int side, int r)
{
    const DuelCardRecord *card = &D_801A7AD8[r];
    return r / DUEL_CARD_SIDE_RECORD_COUNT != side &&
           (card->flags & (DUEL_CARD_FLAG_OCCUPIED | DUEL_CARD_FLAG_FACE_DOWN)) == DUEL_CARD_FLAG_OCCUPIED &&
           living(card->card_id);
}

/* The combo in the hand of the side whose turn it is: the player's marked
 * cards, exactly the three; the CPU's first of each. Their hand cards in
 * picks, and the Zombie's ATK; 0 when it is not there. */
static int combo(int marked, s8 picks[3])
{
    int slot, slots = ehp->hand_slots(), count = 0, zombie_attack = 0;
    picks[0] = picks[1] = picks[2] = -1;
    for (slot = 0; slot < slots; slot++) {
        int id = ehp->hand_card(slot);
        if (!id || (marked && !ehp->hand_mark(slot))) continue;
        count++;
        if (is_polymerization(id) && picks[0] < 0) picks[0] = (s8)slot;
        else if (is_arrow(id) && picks[1] < 0) picks[1] = (s8)slot;
        else if (zombie(id) && picks[2] < 0) {
            picks[2] = (s8)slot;
            zombie_attack = base_attack(id);
        }
    }
    if (picks[0] < 0 || picks[1] < 0 || picks[2] < 0 || (marked && count != 3)) return 0;
    return zombie_attack ? zombie_attack : 1;
}

static void say(int r, int loss)
{
    A.said = (s8)r;
    A.said_loss = (s16)loss;
    A.said_frames = SAID_FRAMES;
}

static void infect(int owner, int r, int loss)
{
    int zone = ZONE_OF(r);
    DuelCardRecord *card = &D_801A7AD8[r];
    A.card[zone] = card->card_id;
    A.deck[zone] = (u8)deck_of(card);
    A.owner[zone] = (s8)owner;
    A.loss[zone] = (s16)loss;
    say(r, 0);
    SD_SEPlayFull(ARROW_SOUND);
}

/* Whether zone `zone` still holds the monster infected there. */
static int still_there(int zone)
{
    const DuelCardRecord *card = &D_801A7AD8[RECORD_OF(zone)];
    return A.card[zone] && (card->flags & DUEL_CARD_FLAG_OCCUPIED) && card->card_id == A.card[zone] &&
           deck_of(card) == A.deck[zone];
}

void Arrow_TurnStarts(int side)
{
    int zone;
    if (!on()) return;
    for (zone = 0; zone < ARROW_ZONES; zone++) {
        DuelCardRecord *card;
        int attack, lost;
        if (!A.card[zone]) continue;
        if (!still_there(zone)) {
            A.card[zone] = 0;
            continue;
        }
        if (A.owner[zone] == side) continue;   /* the infecting side's turn begins: not yet */
        card = &D_801A7AD8[RECORD_OF(zone)];
        attack = attack_of(RECORD_OF(zone));
        lost = attack < A.loss[zone] ? attack : A.loss[zone];
        card->stat_modifier = (s16)(card->stat_modifier - lost);
        say(RECORD_OF(zone), lost);
    }
}

/* The next target from `from`, `step` 1 or -1 through the other side's
 * monster row; -1 for none. */
static int next_target(int side, int from, int step)
{
    int i, col = from >= 0 ? from % DUEL_CARD_SIDE_RECORD_COUNT - DUEL_FIELD_ROW_SIZE : (step > 0 ? -1 : DUEL_FIELD_ROW_SIZE);
    for (i = 0; i < DUEL_FIELD_ROW_SIZE; i++) {
        col = (col + step + DUEL_FIELD_ROW_SIZE) % DUEL_FIELD_ROW_SIZE;
        if (target(side, MONSTER_RECORD(side ^ 1, col))) return MONSTER_RECORD(side ^ 1, col);
    }
    return -1;
}

int Arrow_Hand(void)
{
    u16 pressed = gInput_wPad1Pressed;
    int side = D_8009B1D5 & 1, loss;
    s8 picks[3];
    if (!on()) {
        A.aiming = 0;
        return 0;
    }
    if (A.aiming) {
        if ((gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED)) != (STATE_HAND | STATE_STARTED) ||
            !target(side, A.target)) {
            A.aiming = 0;
            return 0;
        }
        if (pressed & PAD_DIRECTION_LEFT) {
            A.target = (s8)next_target(side, A.target, -1);
            SD_SEPlayFull(MOVE_SOUND);
        } else if (pressed & PAD_DIRECTION_RIGHT) {
            A.target = (s8)next_target(side, A.target, 1);
            SD_SEPlayFull(MOVE_SOUND);
        } else if (pressed & PAD_BUTTON_CIRCLE) {
            A.aiming = 0;
            SD_SEPlayFull(REFUSED_SOUND);
        } else if (pressed & PAD_BUTTON_CONFIRM_MASK) {
            int cards[3];
            A.aiming = 0;
            if (!(loss = combo(1, picks))) return 1;
            infect(side, A.target, loss);
            cards[0] = picks[0];
            cards[1] = picks[1];
            cards[2] = picks[2];
            ehp->take_hand(cards, 3);
        }
        return 1;
    }
    if (D_8009B360[D_8009B1D5] >= 0 || !(pressed & PAD_BUTTON_CONFIRM_MASK) || ehp->menu_open() ||
        (gDuel_wSceneStateFlags & (STATE_MASK | STATE_STARTED | STATE_LOOKING)) != (STATE_HAND | STATE_STARTED) ||
        (D_8009B174 & 0xF) != HAND_IDLE || !(D_8009B174 & HAND_STEP_DONE) || gDuel_bEffectState ||
        gDuel_bQuitDialogState || !combo(1, picks))
        return 0;
    if ((A.target = (s8)next_target(side, -1, 1)) < 0) {
        SD_SEPlayFull(REFUSED_SOUND);
        return 1;
    }
    /* The strongest first. */
    {
        int r = A.target, best = A.target;
        while ((r = next_target(side, r, 1)) != A.target && r >= 0)
            if (attack_of(r) > attack_of(best)) best = r;
        A.target = (s8)best;
    }
    A.aiming = 1;
    SD_SEPlayFull(MOVE_SOUND);
    return 1;
}

int Arrow_Cpu(void)
{
    int side = D_8009B1D5 & 1, col, best = -1, own = 0, loss, cards[3];
    s8 picks[3];
    if (!on() || !(loss = combo(0, picks))) return 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++) {
        int r = MONSTER_RECORD(side ^ 1, col), mine = MONSTER_RECORD(side, col);
        if (target(side, r) && (best < 0 || attack_of(r) > attack_of(best))) best = r;
        if ((D_801A7AD8[mine].flags & DUEL_CARD_FLAG_OCCUPIED) && attack_of(mine) > own) own = attack_of(mine);
    }
    if (best < 0 || attack_of(best) <= own || A.card[ZONE_OF(best)]) return 0;
    infect(side, best, loss);
    cards[0] = picks[0];
    cards[1] = picks[1];
    cards[2] = picks[2];
    ehp->take_hand(cards, 3);
    return 1;
}

/* An infected card, green. */
static void draw_card(void *record, POLY_GT4 *prim, POLY_FT4 *sprite, s32 *color)
{
    DuelCardRecord *card = (DuelCardRecord *)record;
    int r = (int)(card - D_801A7AD8), zone;
    DisplayObject *object = (DisplayObject *)card->object;
    u32 kept;
    void (*original)(void *, POLY_GT4 *, POLY_FT4 *, s32 *) =
        (void (*)(void *, POLY_GT4 *, POLY_FT4 *, s32 *))original_draw_card;
    if (!on() || r < 0 || r >= DUEL_CARD_RECORD_COUNT || r % DUEL_CARD_SIDE_RECORD_COUNT < DUEL_FIELD_ROW_SIZE ||
        r % DUEL_CARD_SIDE_RECORD_COUNT >= 2 * DUEL_FIELD_ROW_SIZE || !object ||
        !(still_there(zone = ZONE_OF(r)) || (A.aiming && r == A.target))) {
        original(record, prim, sprite, color);
        return;
    }
    kept = object->field_0C;
    object->field_0C = (kept & ~0xFFFFFFu) | (A.aiming && r == A.target ? 0x30C0C0u : INFECTED_COLOR);
    original(record, prim, sprite, color);
    object->field_0C = kept;
}

/* The line: the aim, or what the arrow just did. */
static const char *T(const char *english) { return host->api >= 22 ? host->text(host, english) : english; }
static void name(int id, char *out, size_t size)
{
    if (!Cards_NameUtf8(id, out, size)) snprintf(out, size, "#%d", id);
}
static int line(char *text, size_t size)
{
    char card[64];
    if (!on()) return 0;
    if (A.aiming && A.target >= 0) {
        name(D_801A7AD8[(int)A.target].card_id, card, sizeof card);
        snprintf(text, size, T("Living Arrow: %s (ATK %d)  Left/Right, X: infect, O: cancel"), card,
                 attack_of(A.target));
        return 1;
    }
    if (!A.said_frames || A.said < 0) return 0;
    name(D_801A7AD8[(int)A.said].card_id, card, sizeof card);
    if (A.said_loss)
        snprintf(text, size, T("%s rots: -%d ATK"), card, A.said_loss);
    else
        snprintf(text, size, T("%s is infected: it loses %d ATK each turn"), card,
                 A.loss[ZONE_OF((int)A.said)]);
    return 1;
}
static void overlay(void)
{
    char text[160];
    int width, height, scale, w, cx, cy;
    if (next_overlay) next_overlay();
    if (A.said_frames && !A.aiming) A.said_frames--;
    if (!line(text, sizeof text)) return;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    if (host->api >= 22) {
        host->picture_point(host, 160, TEXT_Y, &cx, &cy);
    } else {
        cx = width / 2;
        cy = TEXT_Y * height / 240;
    }
    w = host->text_width(host, text, scale);
    host->fill(host, cx - w / 2 - 8 * scale, cy - 10 * scale, w + 16 * scale, 20 * scale, 0x000000, 160);
    host->draw_text(host, cx - w / 2, cy, text, 0x80FF80, scale);
}
static unsigned overlay_signature(void)
{
    unsigned s = next_signature ? next_signature() : 0;
    if (!on() || (!A.aiming && !A.said_frames)) return s;
    return s * 31u + 1u + (unsigned)A.target * 7u + (unsigned)A.said * 13u + (unsigned)A.said_loss + A.aiming;
}

int Arrow_Install(const MemoriesModHost *from, MemoriesMod *mod, const ExtraHandApi *api)
{
    host = from;
    ehp = api;
    polymerization_id = arrow_id = 0;
    memset(&A, 0, sizeof A);
    A.target = A.said = -1;
    next_overlay = mod->overlay;
    next_signature = mod->overlay_signature;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    return host->hook(host, (void *)func_80015EF4, (void *)draw_card, &original_draw_card);
}
