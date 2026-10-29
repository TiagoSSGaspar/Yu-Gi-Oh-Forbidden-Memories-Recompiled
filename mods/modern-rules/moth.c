/* The moth's evolution, as in the anime's Duelist Kingdom (the setting
 * "moth_evolution"; Weevil's duels with Yugi and with Joey): Cocoon of
 * Evolution put onto the larva -- Petit Moth, or Larvae Moth as the anime
 * calls it -- or the two marked in the hand, fuse into the cocoon, which
 * comes down in defense. At its owner's 5th turn after, it opens: Perfectly
 * Ultimate Great Moth. Destroyed before, it dies with the larva, unless it
 * has had 4 of those turns: then the larva comes out half grown, Great Moth,
 * in attack in its place (Yugi destroyed Weevil's cocoon so).
 * Only a cocoon made of the larva evolves: the fusion marks its side while
 * the placement plays it (the CPU's script, the fusion guard and the fusion
 * helper ask for fusions in the hand), and that side's next summoned Cocoon
 * in the same turn is the one. The evolution is the ritual's
 * (duel_ritual_effect.c): the new card's picture loaded, stored into the
 * card's picture block, the record remade -- a destroyed cocoon's record
 * keeps its deck card, so Great Moth is remade in it the same way. The hand
 * and the field wait for it (Moth_Hold), looking only while nothing is in
 * play: a battle lifts its monsters off their records as it starts. */
#include "types.h"
#include "game/duel_apply_card_object_flags.h"
#include "game/duel_card.h"
#include "game/duel_card_layout.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_deck_card.h"
#include "game/duel_effect_request.h"
#include "game/duel_effect_resource_setup.h"
#include "game/duel_ritual_controller.h"
#include "game/duel_scene_state.h"
#include "game/duel_side_state.h"
#include "game/file_transfer.h"
#include "game/sound.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "unmatched.h"
#include "moth.h"
#include <stdio.h>
#include <string.h>

#define MOTH_PETIT 278
#define MOTH_LARVAE 56
#define MOTH_COCOON 72
#define MOTH_GREAT 57
#define MOTH_PERFECT 67
#define MOTH_GREAT_TURN 4     /* destroyed from then on, Great Moth comes out */
#define MOTH_PERFECT_TURN 5   /* it opens: Perfectly Ultimate Great Moth */
#define SCENE_MASK 0xF
#define SCENE_PLACEMENT 7
#define MOTH_STATE_VERSION 2

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
static int larva(int id) { return id == MOTH_PETIT || id == MOTH_LARVAE; }
static int moth_pair(int a, int b) { return (larva(a) && b == MOTH_COCOON) || (a == MOTH_COCOON && larva(b)); }
static int held(int r) { return (D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) && D_801A7AD8[r].card_id == S.card[r]; }

static void fusion(MemoriesModEvent *e)
{
    if (e->phase != MEMORIES_BEFORE || !on() || !moth_pair(e->a, e->b)) return;
    e->result = MOTH_COCOON;
    e->handled = 1;
    if ((gDuel_wSceneStateFlags & SCENE_MASK) == SCENE_PLACEMENT)
        S.made[D_8009B1D5 & 1] = (u8)(D_800E9FF0[D_8009B1D5 & 1].rank.turns_taken + 1);
}

/* The cocoons whose records lost them. Destroyed (the record empty) after 4
 * of its owner's turns, the cocoon lets Great Moth out; destroyed sooner,
 * fused or replaced, it is forgotten. Only while nothing is in play. */
static void settle(void)
{
    int r;
    for (r = 0; r < DUEL_CARD_RECORD_COUNT; r++) {
        if (!S.turns[r] || r == S.evolving || S.due[r] || held(r)) continue;
        if (!(D_801A7AD8[r].flags & DUEL_CARD_FLAG_OCCUPIED) && S.card[r] == MOTH_COCOON &&
            S.turns[r] - 1 >= MOTH_GREAT_TURN && D_801A7AD8[r].data) {
            S.due[r] = MOTH_GREAT;
            continue;
        }
        S.turns[r] = 0;
    }
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
    } else if (e->c == MEMORIES_MONSTER_DRAW && S.turns[r] && held(r) && S.card[r] == MOTH_COCOON) {
        S.turns[r]++;
        if (S.turns[r] - 1 == MOTH_PERFECT_TURN) S.due[r] = MOTH_PERFECT;
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
        if ((D_8009B0F4_abs & FILE_TRANSFER_REQUEST_BLOCKED_MASK) | D_8009B134_abs) return 1;
        deck->id = (u16)into;
        card->card_id = (s16)into;
        /* The picture's source, as the ritual reads it (D_800EA128[20], [21]). */
        rect.x = D_800EA128[20] + 56;
        rect.y = D_800EA128[21];
        rect.w = 8;
        rect.h = 88;
        StoreImage(&rect, (u32 *)(D_8018C2D8 + deck->data_block_index * 1408));
        S.step = 2;
        return 1;
    }
    default: {  /* the record remade: face up, in attack, what equips gave the cocoon kept */
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
        S.turns[(int)S.evolving] = 0;   /* nothing comes after either */
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
    if (S.evolving < 0) {
        settle();
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
/* A line for each of the player's cocoons: the turns left before it opens,
 * from the picture's line just above the hand upwards, while the hand or
 * the field waits for the player. Its last turn, Great Moth is ready. */
#define LINES_Y 140
#define LINE_STEP 12
static int left(int r)
{
    return S.turns[r] && held(r) && S.card[r] == MOTH_COCOON ? MOTH_PERFECT_TURN - (S.turns[r] - 1) : 0;
}
static int shown(void)
{
    int scene = gDuel_wSceneStateFlags & SCENE_MASK;
    return on() && (scene == 4 || scene == 5);
}
static void overlay(void)
{
    char text[128];
    int r, n, row = 0, width, height, scale, w, cx, cy;
    if (!shown()) return;
    host->overlay_size(host, &width, &height, &scale);
    if (scale < 1) scale = 1;
    for (r = 5; r < 10; r++) {
        if ((n = left(r)) <= 0) continue;
        if (n == 1)
            snprintf(text, sizeof text, "%s",
                     T("Cocoon: Perfectly Ultimate Great Moth next turn; destroyed now, Great Moth comes out"));
        else
            snprintf(text, sizeof text, T("Cocoon: Perfectly Ultimate Great Moth in %d turns"), n);
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
    for (s = 1, r = 5; r < 10; r++) s = s * 31u + (unsigned)left(r);
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
