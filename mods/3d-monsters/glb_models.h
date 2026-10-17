#ifndef MODS_3D_MONSTERS_GLB_MODELS_H
#define MODS_3D_MONSTERS_GLB_MODELS_H
#include "types.h"
#include "psyq/libgte.h"
#include "pc/mods/modapi.h"

/* glTF models standing in for the monsters' own (glb_models.c). */
void GlbModels_Init(const MemoriesModHost *host);
/* The model in models/<card>.glb, read once: its mesh pass id, or 0. */
int GlbModels_Load(int card);
/* Sizes it against its monster's polygons: `ls` placed them `height` lines
 * high, and the body's feet are at x, y, z in their units. */
void GlbModels_Fit(int model, int card, const MATRIX *ls, int height, int x, int y, int z);
/* The clips a model can play, by the names its file gives them. */
enum { GLB_IDLE, GLB_ATTACK, GLB_HIT, GLB_DIE, GLB_SUMMON, GLB_CLIPS };
/* The word to sort in place of the monster's polygons, drawn through `ls`
 * faded `fade` (0 opaque to 255 gone), playing clip `which` (GLB_*)
 * `permille` thousandths of the way through; 0 to draw the polygons. A clip
 * the model lacks plays its idle, and a model with no idle stands. */
u32 GlbModels_Marker(int model, const MATRIX *ls, int fade, int which, int permille);

#endif
