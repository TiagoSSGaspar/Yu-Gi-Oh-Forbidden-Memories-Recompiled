#ifndef MEMORIES_PC_FUSION_HELPER_H
#define MEMORIES_PC_FUSION_HELPER_H
#include "pc/platform/menu.h"
/* View > Fusion helper (SET_FUSION_HELPER): the best fusion in the human
 * player's hand, and what the cards picked so far make. */
void FusionHelper_Viewport(int x, int y, int w, int h);
/* Where the picture is in the window, as the platform last set it (also
 * View > Duel rank's, rank_meter.h, and View > Free Duel progress's,
 * free_duel_progress.h). */
void FusionHelper_GetViewport(int *x, int *y, int *w, int *h);
/* A hand of more than the disc's five slots (D_800EA030), as Extra hand
 * plays keeps one: `slot` gives hand slot `index` laid out as a
 * DuelHandSlot (game/duel_hand.h), or NULL past the last. NULL goes back to
 * the five. A mod finds this through its host's `symbol`, so it loads on a
 * port without it. */
void FusionHelper_SetHand(const void *(*slot)(int index));
unsigned FusionHelper_Signature(void);
void FusionHelper_Draw(MenuCanvas *, int *x, int *y, int *w, int *h);
#endif
