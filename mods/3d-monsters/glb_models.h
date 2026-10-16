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
/* The word to sort in place of the monster's polygons, drawn through `ls`
 * faded `fade` (0 opaque to 255 gone); 0 to draw the polygons. */
u32 GlbModels_Marker(int model, const MATRIX *ls, int fade);

#endif
