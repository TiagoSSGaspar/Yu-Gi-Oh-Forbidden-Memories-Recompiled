/* Petit Moth's evolution, as in the anime (the setting "moth_evolution"):
 * Cocoon of Evolution put onto a Petit Moth, or the two marked in the hand,
 * fuse into the cocoon, which comes down in defense. At its owner's 4th turn
 * after, it becomes Great Moth; at the 6th, Perfectly Ultimate Great Moth.
 * Only a cocoon made of the moth evolves: the fusion marks its side while
 * the placement plays it (the CPU's script, the fusion guard and the fusion
 * helper ask for fusions in the hand), and that side's next summoned Cocoon
 * in the same turn is the one. The evolution is the ritual's
 * (duel_ritual_effect.c): the new card's picture loaded, stored into the
 * card's picture block, the record remade; the hand state waits for it. */
#include "types.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect_request.h"
#include "game/duel_effect_resource_record.h"
#include "game/duel_effect_resource_setup.h"
#include "game/duel_scene_state.h"
#include "game/duel_side_state.h"
#include "game/file_transfer.h"
#include "game/sound.h"
#include "unmatched.h"
#include "moth.h"
#include <stdio.h>
#include <string.h>

#define MOTH_PETIT 278
#define MOTH_COCOON 72
#define MOTH_GREAT 57
#define MOTH_PERFECT 67
#define MOTH_GREAT_TURN 4
#define MOTH_PERFECT_TURN 6
#define SCENE_MASK 0xF
#define SCENE_PLACEMENT 7
#define MOTH_STATE_VERSION 1

static const MemoriesModHost *host;
static struct {
    u8 turns[DUEL_CARD_RECORD_COUNT];  /* 0: no cocoon; else 1 + its owner's turns since it was made */
    s16 card[DUEL_CARD_RECORD_COUNT];  /* the card the record held when last seen */
    s16 due[DUEL_CARD_RECORD_COUNT];   /* the card it is to become, 0 for none */
    u8 made[2];                        /* turns_taken + 1 of the side whose placement fused the cocoon */
    s8 evolving;                       /* the record evolving, -1 for none */
    u8 step;
} S;

static int on(void) { return host->applied(host) && host->setting(host, "moth_evolution", 1); }
static int moth_pair(int a, int b)
{
    return (a == MOTH_PETIT && b == MOTH_COCOON) || (a == MOTH_COCOON && b == MOTH_PETIT);
}

static void fusion(MemoriesModEvent *e)
{
    if (e->phase != MEMORIES_BEFORE || !on() || !moth_pair(e->a, e->b)) return;
    e->result = MOTH_COCOON;
    e->handled = 1;
    if ((gDuel_wSceneStateFlags & SCENE_MASK) == SCENE_PLACEMENT)
        S.made[D_8009B1D5 & 1] = (u8)(D_800E9FF0[D_8009B1D5 & 1].rank.turns_taken + 1);
}

/* A cocoon whose record lost it (destroyed, fused, replaced) is forgotten. */
static void forget_gone(void)
{
    int r;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++)
        if (S.turns[r] && r != S.evolving &&
            (!(D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) || D_801A7AD8[r].card_id != S.card[r]))
            S.turns[r] = 0, S.due[r] = 0;
}

static void monster(MemoriesModEvent *e)
{
    int r = e->b, side;
    if (e->phase != MEMORIES_AFTER || !on() || r < 0 || r >= DUEL_CARD_RECORD_COUNT) return;
    side = r / DUEL_CARD_SIDE_RECORD_COUNT;
    if (e->c == MEMORIES_MONSTER_SUMMON) {
        if (e->a == MOTH_COCOON && S.made[side] == (u8)(D_800E9FF0[side].rank.turns_taken + 1)) {
            S.turns[r] = 1;
            S.card[r] = MOTH_COCOON;
            S.due[r] = 0;
            D_801A7AD8[r].flags |= DUEL_CARD_FLAG_DEFENSE_POSITION;
            Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)D_801A7AD8[r].object);
        }
        S.made[side] = 0;
    } else if (e->c == MEMORIES_MONSTER_DRAW && S.turns[r]) {
        forget_gone();
        if (!S.turns[r]) return;
        S.turns[r]++;
        if (S.turns[r] - 1 == MOTH_GREAT_TURN && S.card[r] == MOTH_COCOON) S.due[r] = MOTH_GREAT;
        else if (S.turns[r] - 1 == MOTH_PERFECT_TURN && S.card[r] == MOTH_GREAT) S.due[r] = MOTH_PERFECT;
    }
}

/* One frame of the evolution of S.evolving, as the ritual plays it
 * (DuelEffect_StartRitual, DuelEffect_ApplyRitual): 1 while it goes on. */
static int evolve(void)
{
    DuelCardRecord *card = &D_801A7AD8[(int)S.evolving];
    DuelDeckCardRecord *deck = (DuelDeckCardRecord *)card->data;
    int into = S.due[(int)S.evolving];
    switch (S.step) {
    case 0: {   /* the ritual's light and sound, the new card's picture asked for */
        DuelEffectRequest *request = DuelEffect_CreateRequest(0x12);
        if (request) {
            request->field_00 = 0xA0;
            request->field_02 = 0x78;
            request->field_1A = 1;
        }
        SD_SEPlayFull(2);
        func_80029164(1, into);
        S.step = 1;
        return 1;
    }
    case 1: {   /* the picture in: into the card's picture block */
        RECT rect;
        DuelEffectResourceRecord *resource = &D_800EA0E8[1];
        if ((D_8009B0F4_abs & FILE_TRANSFER_REQUEST_BLOCKED_MASK) | D_8009B134_abs) return 1;
        deck->id = (u16)into;
        card->card_id = (s16)into;
        rect.x = resource->src_x + 56;
        rect.y = resource->src_y;
        rect.w = 8;
        rect.h = 88;
        StoreImage(&rect, (u32 *)(D_8018C2D8 + deck->data_block_index * 1408));
        S.step = 2;
        return 1;
    }
    default: {  /* the record remade: face up, in attack, its modifiers kept */
        u16 flags = card->flags;
        s16 stat = card->stat_modifier, defense = card->defense_modifier;
        DuelCard_DeactivateRecord(card);
        func_80024D34(S.evolving, deck->deck_index);
        card->flags = (u16)((flags | card->flags | DUEL_CARD_FLAG_OCCUPIED) &
                            ~(DUEL_CARD_FLAG_FACE_DOWN | DUEL_CARD_FLAG_DEFENSE_POSITION));
        card->stat_modifier = stat;
        card->defense_modifier = defense;
        Duel_ApplyCardObjectFlags((DuelCardDisplayObject *)card->object);
        S.card[(int)S.evolving] = (s16)into;
        if (into == MOTH_PERFECT) S.turns[(int)S.evolving] = 0;   /* nothing after it */
        S.due[(int)S.evolving] = 0;
        S.evolving = -1;
        return 0;
    }
    }
}

int Moth_Hold(void)
{
    int r;
    if (!on()) return 0;
    forget_gone();
    if (S.evolving < 0) {
        for (r = 0; r < DUEL_CARD_RECORD_COUNT && !S.due[r]; r++) { }
        if (r == DUEL_CARD_RECORD_COUNT) return 0;
        S.evolving = (s8)r;
        S.step = 0;
    }
    if (evolve()) return 1;
    return Moth_Hold();
}

static const char *T(const char *english) { return host->api >= 22 ? host->text(host, english) : english; }
/* Where a point of the game's picture is on the canvas (API 22), else a 4:3
 * picture as tall as the canvas, in its middle. */
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
/* A line for each of the player's cocoons and Great Moths still to evolve:
 * the turns left, from the picture's line just above the hand upwards,
 * while the hand or the field waits for the player. */
#define LINES_Y 140
#define LINE_STEP 12
static int left(int r)
{
    if (!S.turns[r]) return 0;
    if (S.card[r] == MOTH_COCOON) return MOTH_GREAT_TURN - (S.turns[r] - 1);
    if (S.card[r] == MOTH_GREAT) return MOTH_PERFECT_TURN - (S.turns[r] - 1);
    return 0;
}
static int shown(void)
{
    int scene = gDuel_wSceneStateFlags & SCENE_MASK;
    return on() && (scene == 4 || scene == 5);
}
static void overlay(void)
{
    char text[96];
    int r, n, row = 0, width, height, scale, w, cx, cy;
    if (!shown()) return;
    forget_gone();
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    for (r = 5; r < 10; r++) {
        if ((n = left(r)) <= 0) continue;
        if (S.card[r] == MOTH_COCOON)
            snprintf(text, sizeof text,
                     n == 1 ? T("Cocoon: Great Moth in %d turn") : T("Cocoon: Great Moth in %d turns"), n);
        else
            snprintf(text, sizeof text,
                     n == 1 ? T("Great Moth: Perfectly Ultimate Great Moth in %d turn")
                            : T("Great Moth: Perfectly Ultimate Great Moth in %d turns"), n);
        picture_point(160, LINES_Y - row++ * LINE_STEP, &cx, &cy);
        w = host->text_width(host, text, scale);
        host->fill(host, cx - w / 2 - 8 * scale, cy - 10 * scale, w + 16 * scale, 20 * scale, 0x000000, 160);
        host->draw_text(host, cx - w / 2, cy, text, 0xFFFFFF, scale);
    }
}
static unsigned overlay_signature(void)
{
    unsigned s;
    int r;
    if (!shown()) return 0;
    for (s = 1, r = 5; r < 10; r++) s = s * 31u + (unsigned)left(r) * 7u + (unsigned)(S.card[r] == MOTH_GREAT);
    return s;
}

int Moth_Install(const MemoriesModHost *from, MemoriesMod *mod)
{
    host = from;
    memset(&S, 0, sizeof S);
    S.evolving = -1;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    if (host->api < 10) return 1;   /* no monster events: no evolution, nothing else changes */
    return host->subscribe(host, MEMORIES_EVENT_FUSION, 0, fusion) > 0 &&
        host->subscribe(host, MEMORIES_EVENT_MONSTER, 0, monster) > 0 &&
        host->register_state(host, &S, sizeof S, MOTH_STATE_VERSION);
}
