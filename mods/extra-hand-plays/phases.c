/* The player's turn in phases, as in the GX games (set_phases in
 * extra_hand_plays.h): Main Phase 1 is the hand, the Battle Phase the
 * field, Main Phase 2 the hand again after it, then the End Phase (the
 * discard and the turn switch). START opens a menu of the next phases
 * instead of ending the hand or the turn at once: UP and DOWN move its
 * cursor, X takes the phase under it, circle or START close it. The CPU
 * plays as before. */
#include "hand_internal.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#include "game/input.h"

static ExtraHandPhases phases;

static int human(void) { return D_8009B360[D_8009B1D5] < 0; }

void Ehp_SetPhases(ExtraHandPhases fn) { phases = fn; }
int Ehp_Phases(void) { return phases && phases() && human(); }

/* In a hand, the Battle Phase (from Main Phase 1 only) and the End Phase;
 * on the field, Main Phase 2 while the hand holds a card the turn may still
 * play, and the End Phase. */
int Ehp_PhaseItems(int field, int items[2])
{
    int n = 0;
    if (field) {
        if (Ehp_PlayableLeft()) items[n++] = PHASE_MAIN2;
    } else if (S.main2 != 2) {
        items[n++] = PHASE_BATTLE;
    }
    items[n++] = PHASE_END;
    return n;
}

/* The menu for this frame's buttons, while the hand or the field is idle:
 * START opens it; then the phase taken, or -1 while it is open (and the
 * frame it closes), so that no button reaches the game. */
int Ehp_PhaseMenu(int field)
{
    int items[2], n = Ehp_PhaseItems(field, items), item;
    u16 pressed = gInput_wPad1Pressed;
    if (!S.menu) {
        if (!(pressed & PAD_BUTTON_START)) return 0;
        S.menu = 1;
        return -1;
    }
    if (S.menu > n) S.menu = (u8)n;
    if (pressed & (PAD_BUTTON_CIRCLE | PAD_BUTTON_START)) {
        S.menu = 0;
        return -1;
    }
    if ((pressed & PAD_DIRECTION_UP) && S.menu > 1) S.menu--;
    if ((pressed & PAD_DIRECTION_DOWN) && S.menu < n) S.menu++;
    if (!(pressed & PAD_BUTTON_CROSS)) return -1;
    item = items[S.menu - 1];
    S.menu = 0;
    return item;
}
