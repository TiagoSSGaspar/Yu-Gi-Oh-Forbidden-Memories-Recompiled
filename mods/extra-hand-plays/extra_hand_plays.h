#ifndef EXTRA_HAND_PLAYS_H
#define EXTRA_HAND_PLAYS_H
/* What the Extra hand plays mod shares with other mods, as
 * host->find(host, "extra-hand-plays:api"). A mod that uses it lists
 * extra-hand-plays under "requires", so it is applied and initialized
 * first. */

/* How many cards `side` draws at the start of its turn `turn` (its
 * turns_taken: the duel's first turn has no draw and is not counted), with
 * `room` empty hand slots; -1 leaves the disc's refill to five. Asked once
 * a turn; the cards that do not fit wait (queued). */
typedef int (*ExtraHandTurnDraws)(int side, int turn, int room);

/* Whether the side whose turn it is may skip its first hand (START), so
 * that playing a card is optional. Asked while that hand is up. */
typedef int (*ExtraHandOptionalPlay)(void);

/* 1: the first three entries; 2: set_optional_play; 3: pass_hand. New
 * entries only go at the end, so check version before calling a later one. */
#define EXTRA_HAND_API_VERSION 3

typedef struct {
    unsigned version;   /* EXTRA_HAND_API_VERSION */
    /* The turn draws, or NULL for the disc's. One mod sets it. */
    void (*set_turn_draws)(ExtraHandTurnDraws draws);
    /* Once the card being played has resolved, the hand comes up again for
     * the side whose turn it is, with `draws` cards drawn into it first. It
     * does not count against the plays per turn. */
    void (*bonus_hand)(int draws);
    /* Cards drawn for `side` that did not fit in its hand, still owed. */
    int (*queued)(int side);
    /* Version 2: playing a card in the first hand of a turn becomes
     * optional while `fn` says so; NULL makes it required again, as on the
     * disc. One mod sets it. */
    void (*set_optional_play)(ExtraHandOptionalPlay fn);
    /* Version 3: the side whose turn it is plays no card from the hand now
     * up, as START does for the player: the hand slides away, the field
     * starts and no other hand follows this turn. For the CPU, called from
     * inside its hand script (AiScript_Run) with its decision emptied
     * (D_800EAE88 all zero). Does nothing outside a hand. */
    void (*pass_hand)(void);
} ExtraHandApi;

#endif
