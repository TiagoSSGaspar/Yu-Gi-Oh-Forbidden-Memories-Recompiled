#ifndef MEMORIES_PC_TOUCH_PAD_ART_H
#define MEMORIES_PC_TOUCH_PAD_ART_H
/* The on-screen controller's pictures (touch_pad.h), in its style (View >
 * Touch controls > Style), see-through at View > Touch controls > Opacity
 * (a button held more solid):
 *
 * Game: drawn here at the window's own size so they stay smooth on any
 * screen: dark see-through buttons with a thin light edge, the face
 * buttons' symbols in the PlayStation's colors, the D-pad's arrows, and the
 * L/R, SELECT, START and MENU pills labelled in the overlay's face
 * (overlay_text.h). Each button's picture, let go and held, is drawn once
 * per size.
 *
 * Simple (touch_pad_simple_art.c): one grey scale, frames and discs with a
 * light edge, the D-pad's solid arrows in a ring, the face buttons'
 * outlined triangle, circle, cross and square, MENU's cog and STATES' and
 * SPEED's triangles, all drawn; L1/L2/R1/R2, SELECT, START and the speed in
 * the game text's font.
 *
 * Main thread only. */
#include "menu.h"

/* Draws the pad where touch_pad.c has laid it out (nothing while hidden),
 * and gives the bounds it covered (all 0 when nothing). */
void TouchPadArt_Draw(MenuCanvas *canvas, int *x, int *y, int *w, int *h);
/* Drops the pictures; the next draw makes them again. */
void TouchPadArt_Reset(void);
/* TouchPadArt_Draw for Simple. */
void TouchPadArt_DrawSimple(MenuCanvas *canvas, int *x, int *y, int *w, int *h);

#endif
