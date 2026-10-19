#ifndef EXTRA_HAND_PLAYS_H
#define EXTRA_HAND_PLAYS_H
/* What the Extra hand plays mod shares with other mods, as
 * host->find(host, "extra-hand-plays:api"). A mod that uses it lists
 * extra-hand-plays under "requires", so it is applied and initialized
 * first. */

/* How many cards `side` draws at the start of its turn `turn` (counting
 * from 1), with `room` empty hand slots; -1 leaves the disc's refill to
 * five. Asked once a turn; the cards that do not fit wait (queued). */
typedef int (*ExtraHandTurnDraws)(int side, int turn, int room);

#define EXTRA_HAND_API_VERSION 1

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
} ExtraHandApi;

#endif
