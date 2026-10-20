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

/* How many cards `side` may hold (`*max`, 5 to 10) and keep at the end of
 * its turn (`*keep`, 0 for no limit). Both come in at the disc's: 5 and 0.
 * Asked whenever they are needed. */
typedef void (*ExtraHandLimits)(int side, int *max, int *keep);

/* Whether the player's turn runs in phases, as in the GX games. Asked
 * while the player's hand or field is up. */
typedef int (*ExtraHandPhases)(void);

/* 1: the first three entries; 2: set_optional_play; 3: pass_hand; 4: the
 * hand past five cards; 5: set_phases. New entries only go at the end, so
 * check version before calling a later one. */
#define EXTRA_HAND_API_VERSION 5

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
    /* Version 4: the hand limits, or NULL for the disc's. One mod sets it.
     * Past five cards the hand is laid out overlapped, and the player picks
     * from all of it; the CPU's hand script sees the cards past the fifth
     * ahead of the upcoming deck. */
    void (*set_hand_limits)(ExtraHandLimits limits);
    /* The hand of the side whose turn it is, slot by slot (0 up): how many
     * slots it has, the card in one (0 empty) and its place in the order
     * the player marked cards in (0 unmarked). */
    int (*hand_slots)(void);
    int (*hand_card)(int slot);
    int (*hand_mark)(int slot);
    /* Version 5: the player's turn in phases while `fn` says so. START
     * opens a menu of the next phases instead of ending the hand or the
     * turn: Main Phase 1 (the hand) goes to the Battle Phase (the field) or
     * the End Phase; the Battle Phase to Main Phase 2 (the hand again) or
     * the End Phase; Main Phase 2 to the End Phase. The CPU plays as
     * before. NULL goes back to START ending the hand or the turn. One mod
     * sets it. */
    void (*set_phases)(ExtraHandPhases fn);
} ExtraHandApi;

#endif
