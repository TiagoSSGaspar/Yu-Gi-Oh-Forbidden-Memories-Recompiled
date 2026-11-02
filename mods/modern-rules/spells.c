/* Change of Heart and Monster Reborn, as Yugi and Kaiba play them in the
 * Duelist Kingdom (the settings "change_of_heart" and "monster_reborn";
 * both cards are this mod's, copies of Hinotama with no effect of their
 * own, so off they do nothing).
 * - Change of Heart: the user picks a monster of the opponent's (left and
 *   right, cross), which comes to a free monster zone of the user's as it
 *   was (position, face, what equips and effects gave it), and may attack
 *   that turn. When the next turn starts it goes back to its owner: to the
 *   zone it left, or another free one; with none, it is destroyed.
 * - Monster Reborn: the disc keeps no graveyard, so this mod writes one: each
 *   monster destroyed in the duel (MEMORIES_EVENT_MONSTER), its owner's (a
 *   monster taken by Change of Heart is still its owner's). The user picks
 *   one of either side's (left and right, a line naming it), which comes
 *   back in a free monster zone of the user's, face up in attack, with the
 *   ritual's light, and leaves the graveyard.
 * The player's card is played as any magic card (the magic scene,
 * func_80019608): its effect is this mod's (MEMORIES_EVENT_EFFECT, both of
 * the scene's calls handled), and the pick holds the duel in the card
 * effect's update (DuelEffect_UpdateCardEffect, 1 while it lasts) until the
 * cross. With nothing to take or revive, or no zone, its use is refused
 * before it is played (modern_rules.c). The CPU plays them from its hand
 * (Extra hand plays' take_hand), taking the player's strongest monster when
 * it is stronger than all of the CPU's, or reviving the strongest monster of
 * the graveyard when that one is.
 * A monster is moved by the record lifecycle the ritual uses: the record
 * emptied (DuelCard_RemoveFromField) and another made from the same deck
 * card (func_80024D34; the deck cards are the two sides' together). */
#include "types.h"
#include "game/card_constants.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_object_helpers.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect_request.h"
#include "game/duel_grid.h"
#include "game/duel_hand.h"
#include "game/duel_scene_state.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/input.h"
#include "game/sound.h"
#include "pc/cards/cards.h"
#include "pc/cards/monster_effects_duel.h"
#include "unmatched.h"
#include "spells.h"
#include "equips.h"
#include "harpies.h"
#include "hats.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#define S (gModernRules_State.spells)
#define MONSTER_RECORD(side, col) ((side) * DUEL_CARD_SIDE_RECORD_COUNT + DUEL_FIELD_ROW_SIZE + (col))
#define THEIR_ROW 1
#define PICK_HEART 1
#define PICK_REBORN 2
#define REFUSED_SOUND 8
#define MOVE_SOUND 0xB
#define TAKE_SOUND 0x0D
#define RITUAL_SOUND 2
#define RITUAL_LIGHT 0x12
#define DESTROY_SOUND 0x1F
#define DESTROY_EFFECT 0xB

static const MemoriesModHost *host;
static const ExtraHandApi *ehp;
static int heart_id, reborn_id;
static void (*next_overlay)(void);
static unsigned (*next_signature)(void);

static int heart_on(void) { return host && host->applied(host) && host->setting(host, "change_of_heart", 1); }
static int reborn_on(void) { return host && host->applied(host) && host->setting(host, "monster_reborn", 1); }
static int card_id(const char *identity, int *cached)
{
    if (*cached <= 0 && host->card_id) *cached = host->card_id(host, identity);
    return *cached;
}
static int is_heart(int id) { return id > 0 && id == card_id("modern-rules:change_of_heart:1", &heart_id); }
static int is_reborn(int id) { return id > 0 && id == card_id("modern-rules:monster_reborn:1", &reborn_id); }

static int base_attack(int id) { return (gDuel_adwCardStats[id - 1] & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE; }
static int base_defense(int id)
{
    return ((gDuel_adwCardStats[id - 1] >> CARD_STAT_DEFENSE_SHIFT) & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE;
}
static int occupied(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) != 0; }
static int deck_of(DuelCardRecord *card) { return card->data ? (u8)((DuelDeckCardRecord *)card->data)->deck_index : 0xFF; }
static int attack_of(int r) { return Duel_CalcCardStats(&D_801A7AD8[r]) & 0xFFFF; }
static int free_zone(int side)
{
    int col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (!occupied(MONSTER_RECORD(side, col))) return MONSTER_RECORD(side, col);
    return -1;
}
/* The record seen at column `col` of the opponent's monster row, from side
 * `side`, as the field cursor walks it. */
static int their_record(int side, int col)
{
    return D_800907D8[THEIR_ROW * DUEL_FIELD_ROW_SIZE + col + side * DUEL_FIELD_SIDE_GRID_SLOT_COUNT];
}
static int has_target(int side)
{
    int col;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (occupied(their_record(side, col))) return 1;
    return 0;
}

int Spells_Refuses(int card, int side)
{
    if (is_heart(card) && heart_on()) return !has_target(side) || free_zone(side) < 0;
    if (is_reborn(card) && reborn_on()) return !S.grave_count || free_zone(side) < 0;
    return 0;
}

/* --- moving a monster ----------------------------------------------------- */

/* Record `to` made from deck card `deck`, with these flags and modifiers. */
static void bring(int to, int deck, u16 flags, s16 attack, s16 defense)
{
    DuelCardRecord *card = &D_801A7AD8[to];
    func_80024D34(to, deck);
    card->flags = (u16)((card->flags | flags | DUEL_CARD_FLAG_OCCUPIED) & ~DUEL_CARD_FLAG_USED_THIS_TURN);
    card->stat_modifier = attack;
    card->defense_modifier = defense;
    if (card->object) Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
}

/* The monster on record `from` moved to record `to`, as it is (Magical
 * Hats moves one too). */
/* The monster on record `from` moved to record `to` as deck card `deck`
 * (its own, or one lent it: adopt). */
static void move_as(int from, int to, int deck)
{
    DuelCardRecord *card = &D_801A7AD8[from];
    u16 flags = (u16)(card->flags & (DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_DEFENSE_POSITION));
    s16 attack = card->stat_modifier, defense = card->defense_modifier;
    Harpies_Moved(from, to);
    Equips_Moved(from, to);
    /* Moved, not destroyed: the monster effects' look at the zones takes
     * the zone it left as one played over (no "destroyed" there). */
    MonsterEffects_Placed(from, 0);
    DuelCard_RemoveFromField(card);
    bring(to, deck, flags, attack, defense);
}

void Spells_Move(int from, int to) { move_as(from, to, deck_of(&D_801A7AD8[from])); }

/* A card of the player's deck cannot stand on the CPU's side: a record of
 * the CPU's takes a deck index under 40 as the CPU's own deck card (40 on;
 * Duel_SetupCardRecord), so the card would be made, and remade after each
 * battle, as the CPU's card at that place in its deck. The card the CPU
 * spends to bring it over (Change of Heart, Monster Reborn) leaves the duel
 * and lends it its deck card: the card's id and picture block go into it,
 * as Magical Hats lends its own to the decoys. Returns the deck card to
 * make it from: `spent`, or `deck` itself when no lending is needed. */
static int adopt(int side, int deck, int spent)
{
    DuelDeckCardRecord *into, *from;
    if (side == 0 || deck >= DECK_SIZE || spent < DECK_SIZE || spent >= 2 * DECK_SIZE) return deck;
    into = &gDuel_aDeckCardRecords[spent];
    from = &gDuel_aDeckCardRecords[deck];
    into->id = from->id;
    into->data_block_index = from->data_block_index;
    into->flags_04 = from->flags_04;
    return spent;
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

/* --- Change of Heart ------------------------------------------------------- */

static void take(int side, int r, int spent)
{
    int to = free_zone(side), home, made;
    if (to < 0 || !occupied(r) || S.lent_count >= SPELLS_LENT_MAX) return;
    home = deck_of(&D_801A7AD8[r]);
    made = adopt(side, home, spent);
    S.lent_to[S.lent_count] = (s8)to;
    S.lent_from[S.lent_count] = (s8)r;
    S.lent_deck[S.lent_count] = (u8)made;
    S.lent_home[S.lent_count] = (u8)home;
    S.lent_count++;
    move_as(r, to, made);
    SD_SEPlayFull(TAKE_SOUND);
}

/* Whether lent monster `i` still stands where it was taken to. */
static int lent_there(int i)
{
    DuelCardRecord *card = &D_801A7AD8[(int)S.lent_to[i]];
    return occupied(S.lent_to[i]) && deck_of(card) == S.lent_deck[i];
}

void Spells_TurnStarts(int side)
{
    (void)side;
    if (S.lent_count) S.lent_due = 1;
    /* A new duel: nothing in the graveyard, nothing lent. */
    if (!D_800E9FF0[0].rank.turns_taken && !D_800E9FF0[1].rank.turns_taken &&
        !D_800E9FF0[0].deck_draw_cursor && !D_800E9FF0[1].deck_draw_cursor) {
        S.grave_count = 0;
        S.lent_count = 0;
        S.lent_due = 0;
        S.picking = 0;
    }
}

int Spells_Hold(void)
{
    int i;
    if (!S.lent_due) return 0;
    for (i = 0; i < S.lent_count; i++) {
        int owner = S.lent_from[i] / DUEL_CARD_SIDE_RECORD_COUNT, to;
        if (!lent_there(i)) continue;
        to = occupied(S.lent_from[i]) ? free_zone(owner) : S.lent_from[i];
        if (to < 0) destroy(&D_801A7AD8[(int)S.lent_to[i]]);
        else move_as(S.lent_to[i], to, S.lent_home[i]);   /* as its own card again */
    }
    S.lent_count = 0;
    S.lent_due = 0;
    return 0;
}

/* --- the graveyard and Monster Reborn ----------------------------------- */

/* The owner of the monster on record `r`: a lent one's is the side it was
 * taken from. */
static int owner_of(int r)
{
    int i;
    for (i = 0; i < S.lent_count; i++)
        if (S.lent_to[i] == r && deck_of(&D_801A7AD8[r]) == S.lent_deck[i])
            return S.lent_from[i] / DUEL_CARD_SIDE_RECORD_COUNT;
    return r / DUEL_CARD_SIDE_RECORD_COUNT;
}

static void monster(MemoriesModEvent *e)
{
    int r = e->b, deck;
    if (e->phase != MEMORIES_AFTER || e->c != MEMORIES_MONSTER_DESTROYED || r < 0 || r >= DUEL_CARD_RECORD_COUNT ||
        e->a <= 0 || S.grave_count >= SPELLS_GRAVE_MAX || Hats_IsDecoy(e->a))
        return;
    deck = deck_of(&D_801A7AD8[r]);
    if (deck == 0xFF) return;
    S.grave_card[S.grave_count] = (s16)e->a;
    S.grave_deck[S.grave_count] = (u8)deck;
    S.grave_owner[S.grave_count] = (u8)owner_of(r);
    S.grave_count++;
}

static void revive(int side, int i, int spent)
{
    int to = free_zone(side), n, made;
    DuelEffectRequest *request;
    DuelDeckCardRecord *deck;
    if (to < 0 || i < 0 || i >= S.grave_count) return;
    /* The deck card is the one it was: its id as it died. */
    deck = &gDuel_aDeckCardRecords[S.grave_deck[i]];
    deck->id = (u16)S.grave_card[i];
    made = adopt(side, S.grave_deck[i], spent);
    bring(to, made, 0, 0, 0);
    request = DuelEffect_CreateRequest(RITUAL_LIGHT);
    if (request) {
        request->field_00 = 0xA0;
        request->field_02 = 0x78;
        request->field_1A = 1;
    }
    SD_SEPlayFull(RITUAL_SOUND);
    for (n = i; n + 1 < S.grave_count; n++) {
        S.grave_card[n] = S.grave_card[n + 1];
        S.grave_deck[n] = S.grave_deck[n + 1];
        S.grave_owner[n] = S.grave_owner[n + 1];
    }
    S.grave_count--;
}

static int strongest_grave(void)
{
    int i, best = -1;
    for (i = 0; i < S.grave_count; i++)
        if (best < 0 || base_attack(S.grave_card[i]) > base_attack(S.grave_card[best])) best = i;
    return best;
}

/* --- the player's pick ------------------------------------------------- */

static int next_target(int side, int col, int step)
{
    int n;
    for (n = 0; n < DUEL_FIELD_ROW_SIZE; n++) {
        col = (col + step + DUEL_FIELD_ROW_SIZE) % DUEL_FIELD_ROW_SIZE;
        if (occupied(their_record(side, col))) return col;
    }
    return -1;
}

static int strongest_their(int side)
{
    int col, best = -1;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (occupied(their_record(side, col)) &&
            (best < 0 || attack_of(their_record(side, col)) > attack_of(their_record(side, best))))
            best = col;
    return best;
}

/* The card's effect begins (the magic scene's second call): the player
 * picks; the CPU, should it ever play one so, takes the strongest. */
static void begin(int kind, int side)
{
    if (D_8009B360[side] >= 0) {
        if (kind == PICK_HEART && strongest_their(side) >= 0) take(side, their_record(side, strongest_their(side)), -1);
        if (kind == PICK_REBORN) revive(side, strongest_grave(), -1);
        return;
    }
    S.kind = (u8)kind;
    S.side = (s8)side;
    S.at = (s8)(kind == PICK_HEART ? strongest_their(side) : strongest_grave());
    S.picking = S.at >= 0;
}

/* One frame of the pick: 1 while it lasts. */
static int pick(void)
{
    u16 pressed = gInput_wPad1Pressed;
    int count = S.kind == PICK_HEART ? DUEL_FIELD_ROW_SIZE : S.grave_count;
    if (S.kind == PICK_HEART && !has_target(S.side)) S.picking = 0;
    if (S.kind == PICK_REBORN && (!S.grave_count || S.at >= S.grave_count)) S.picking = 0;
    if (!S.picking || free_zone(S.side) < 0) {
        S.picking = 0;
        return 0;
    }
    if (pressed & (PAD_DIRECTION_LEFT | PAD_DIRECTION_RIGHT)) {
        int step = pressed & PAD_DIRECTION_LEFT ? -1 : 1;
        S.at = (s8)(S.kind == PICK_HEART ? next_target(S.side, S.at, step) : (S.at + step + count) % count);
        SD_SEPlayFull(MOVE_SOUND);
    } else if (pressed & PAD_BUTTON_CONFIRM_MASK) {
        S.picking = 0;
        if (S.kind == PICK_HEART) take(S.side, their_record(S.side, S.at), -1);
        else revive(S.side, S.at, -1);
        return 0;
    }
    gInput_wPad1Pressed = 0;
    return 1;
}

int Spells_Aimed(int r)
{
    return S.picking && S.kind == PICK_HEART && S.at >= 0 && r == their_record(S.side, S.at);
}

static void effect(MemoriesModEvent *e)
{
    int kind;
    if (e->phase != MEMORIES_BEFORE) return;
    if (e->c) {   /* the update, each frame */
        if (!S.picking) return;
        e->handled = 1;
        e->result = pick();
        return;
    }
    kind = is_heart(e->a) && heart_on() ? PICK_HEART : is_reborn(e->a) && reborn_on() ? PICK_REBORN : 0;
    if (!kind) return;
    e->handled = 1;
    if (e->b) begin(kind, D_8009B1D5 & 1);
}

/* --- the CPU ---------------------------------------------------------------- */

static int hand_slot(int (*is)(int))
{
    int slot, slots = ehp->hand_slots();
    for (slot = 0; slot < slots; slot++)
        if (is(ehp->hand_card(slot))) return slot;
    return -1;
}

static int strongest_own(int side)
{
    int col, best = 0;
    for (col = 0; col < DUEL_FIELD_ROW_SIZE; col++)
        if (occupied(MONSTER_RECORD(side, col)) && attack_of(MONSTER_RECORD(side, col)) > best)
            best = attack_of(MONSTER_RECORD(side, col));
    return best;
}

/* The deck card of the CPU's hand card in slot `slot` (one of the five,
 * the disc's hand records), or -1. */
static int hand_deck(int side, int slot)
{
    int order, record;
    if (slot < 0 || slot >= HAND_SIZE) return -1;
    order = D_800907CC[slot + side * HAND_SIZE];
    record = order & 0x80 ? (order & 0x7F) + DUEL_CARD_SIDE_RECORD_COUNT : order;
    return deck_of(&D_801A7AD8[record]) == 0xFF ? -1 : deck_of(&D_801A7AD8[record]);
}

int Spells_Cpu(void)
{
    int side = D_8009B1D5 & 1, slot, col, best;
    if (free_zone(side) < 0) return 0;
    if (heart_on() && (slot = hand_slot(is_heart)) >= 0 && hand_deck(side, slot) >= 0 &&
        (col = strongest_their(side)) >= 0 &&
        attack_of(their_record(side, col)) > strongest_own(side)) {
        take(side, their_record(side, col), hand_deck(side, slot));
        ehp->take_hand(&slot, 1);
        return 1;
    }
    if (reborn_on() && (slot = hand_slot(is_reborn)) >= 0 && hand_deck(side, slot) >= 0 &&
        (best = strongest_grave()) >= 0 &&
        base_attack(S.grave_card[best]) > strongest_own(side)) {
        revive(side, best, hand_deck(side, slot));
        ehp->take_hand(&slot, 1);
        return 1;
    }
    return 0;
}

/* --- the line while the player picks ----------------------------------- */

static const char *T(const char *english) { return host->api >= 22 ? host->text(host, english) : english; }
static void name(int id, char *out, size_t size)
{
    if (!Cards_NameUtf8(id, out, size)) snprintf(out, size, "#%d", id);
}
static void overlay(void)
{
    char text[192], card[64];
    int width, height, scale, w, cx, cy;
    if (next_overlay) next_overlay();
    if (!S.picking) return;
    if (S.kind == PICK_HEART) {
        int r = their_record(S.side, S.at);
        name(D_801A7AD8[r].card_id, card, sizeof card);
        snprintf(text, sizeof text, T("Change of Heart: %s (ATK %d)  Left/Right, X: take it"), card, attack_of(r));
    } else {
        int id = S.grave_card[(int)S.at];
        name(id, card, sizeof card);
        snprintf(text, sizeof text,
                 S.grave_owner[(int)S.at] == (u8)S.side ? T("Monster Reborn: %s %d/%d, yours  Left/Right, X: revive")
                                                         : T("Monster Reborn: %s %d/%d, the opponent's  Left/Right, X: revive"),
                 card, base_attack(id), base_defense(id));
    }
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
    return S.picking ? s * 31u + 11u + (unsigned)S.kind * 7u + (unsigned)S.at : s;
}

int Spells_Install(const MemoriesModHost *from, MemoriesMod *mod, const ExtraHandApi *api)
{
    host = from;
    ehp = api;
    heart_id = reborn_id = 0;
    memset(&S, 0, sizeof S);
    next_overlay = mod->overlay;
    next_signature = mod->overlay_signature;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    if (host->api < 10) return 1;
    return host->subscribe(host, MEMORIES_EVENT_EFFECT, 0, effect) > 0 &&
        host->subscribe(host, MEMORIES_EVENT_MONSTER, 0, monster) > 0;
}
