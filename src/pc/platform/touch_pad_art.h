#ifndef MEMORIES_PC_TOUCH_PAD_ART_H
#define MEMORIES_PC_TOUCH_PAD_ART_H
/* The on-screen controller's pictures (touch_pad.h), drawn here at the
 * window's own size so they stay smooth on any screen: dark see-through
 * buttons with a thin light edge, the face buttons' symbols in the
 * PlayStation's colors, the D-pad's arrows, and the L/R, SELECT, START and
 * MENU pills labelled in the overlay's face (overlay_text.h). Each button's
 * picture, let go and held, is drawn once per size. Main thread only. */
#include "menu.h"

/* Draws the pad where touch_pad.c has laid it out (nothing while hidden),
 * and gives the bounds it covered (all 0 when nothing). */
void TouchPadArt_Draw(MenuCanvas *canvas, int *x, int *y, int *w, int *h);
/* Drops the pictures; the next draw makes them again. */
void TouchPadArt_Reset(void);

#endif
