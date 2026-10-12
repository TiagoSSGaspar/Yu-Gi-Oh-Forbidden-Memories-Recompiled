#ifndef HD_TEXTURES_H
#define HD_TEXTURES_H
/* HD textures for the models (hd_textures.c), called from field_models.c:
 * part of the 3D Monsters mod, not a mod of its own. */
#include "types.h"
#include "pc/mods/modapi.h"
#include <stddef.h>

void HdTextures_Init(const MemoriesModHost *host);
/* With the hidden dump_bank setting on, write what keep_textures took (the
 * block, then the stance row) to the mod's data as bank-<card>-<stance>.raw. */
void HdTextures_Dump(int card, int position, const u16 *block, size_t block_words, const u16 *row,
                     size_t row_words);
/* Lay the PNGs of hd/<card>/ that this monster's packets use over its bank. */
void HdTextures_Register(int bank, int card, int position, const u32 *from, const u32 *to);
/* Take a bank's PNGs away again; every bank's. */
void HdTextures_Forget(int bank);
void HdTextures_ForgetAll(void);

#endif
