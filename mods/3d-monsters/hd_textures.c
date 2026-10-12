/* HD textures for the monsters' models, from PNGs the mod carries.
 *
 * A model's texture is VRAM words: four 64-word columns at row 0x100 (three
 * of the record's, the fourth its stance's) read through palettes at rows
 * 0xF0..0xFF and the stance row (field_models.c, run_record_slot). Each
 * column is one texture page. A PNG in hd/<card>/ replaces one page read
 * through one palette, laid over the monster's texture bank with
 * TexturePack_AddBankSprite: above 1x the renderers sample the PNG; at 1x,
 * and through any other palette, the bank's own texels show. Which pages
 * and palettes a model uses is read from its first sorted packets. */
#include "hd_textures.h"
#include "pc/render/packets.h"
#include "pc/render/texture_pack.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OWNER_BASE 0x3D000u /* above the engine's owners (texture_pack.h) */
#define USES 32             /* (page, palette) pairs a model may use: one owner each */
#define TEXTURE_Y 0x100     /* the model's texture band */
#define BAND_W 0x100        /* four columns of 64 words */

typedef struct {
    int page_x, depth, clut_x, clut_y;
} Use;

static const MemoriesModHost *host;
static unsigned registered; /* bit n: bank n has PNGs laid over it */

void HdTextures_Init(const MemoriesModHost *from)
{
    host = from;
}

static void say(const char *format, ...)
{
    char message[256];
    va_list args;
    if (!host->log_enabled(host)) return;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    host->log(host, "%s", message);
}

/* A bank sprite is told apart by its owner, bank and rectangle, not its
 * palette (texture_pack.c), so each pair of a bank has an owner of its own. */
static unsigned owner(int bank, int use)
{
    return OWNER_BASE + (unsigned)bank * USES + (unsigned)use;
}

/* The (page, palette) pairs of the textured polygons in a run of packets,
 * walked as stamp_bank walks them (field_models.c): the palette is in the
 * first corner's texture word, the page in the second's. Pairs beyond USES
 * are not listed; *dropped counts them. */
static int uses_of(const u32 *from, const u32 *to, Use *uses, int *dropped)
{
    int count = 0;
    *dropped = 0;
    while (from < to) {
        unsigned words = *from >> 24, at = 1;
        while (at <= words) {
            u32 command = from[at];
            unsigned op = command >> 24, length = Memories_GpuCommandWords(command);
            if (!length || at + length > words + 1) {
                break;
            }
            if (op >= 0x24 && op <= 0x3f && (op & 4)) {
                u32 clut = from[at + 2] >> 16;
                u32 page = from[at + ((op & 0x10) ? 5 : 4)] >> 16;
                Use use;
                int i;
                memset(&use, 0, sizeof(use));
                use.page_x = (int)(page & 0xF) * 64;
                use.depth = (int)(page >> 7) & 3;
                use.clut_x = (int)(clut & 0x3F) * 16;
                use.clut_y = (int)(clut >> 6) & 0x1FF;
                if ((page & 0x10) && use.page_x < BAND_W) {
                    for (i = 0; i < count && memcmp(&uses[i], &use, sizeof(use)); i++) {
                    }
                    if (i == count && count < USES) {
                        uses[count++] = use;
                    } else if (i == count) {
                        (*dropped)++;
                    }
                }
            }
            at += length;
        }
        from += words + 1;
    }
    return count;
}

static void file_of(char *out, size_t size, int card, int position, const Use *use)
{
    int column = use->page_x / 64;
    if (column == 3) {
        snprintf(out, size, "hd/%04d/p3s%d_%03x_%03x.png", card, position, use->clut_x, use->clut_y);
    } else {
        snprintf(out, size, "hd/%04d/p%d_%03x_%03x.png", card, column, use->clut_x, use->clut_y);
    }
}

void HdTextures_Register(int bank, int card, int position, const u32 *from, const u32 *to)
{
    Use uses[USES];
    char relative[64], path[1024];
    int dropped, count = uses_of(from, to, uses, &dropped), on = host->setting(host, "hd_textures", 1), laid = 0, i;

    HdTextures_Forget(bank);
    if (dropped > 0) {
        say("hd: card %d stance %d: more than %d pairs, %d keep the game's texels\n", card, position, USES, dropped);
    }
    for (i = 0; i < count; i++) {
        const Use *use = &uses[i];
        int width = use->depth == 0 ? 256 : use->depth == 1 ? 128 : 64;
        FILE *file;
        const char *what = "off";
        file_of(relative, sizeof(relative), card, position, use);
        file = on ? host->open_asset(host, relative) : NULL;
        if (file) {
            fclose(file);
            snprintf(path, sizeof(path), "%s/%s", host->directory, relative);
            TexturePack_BankSpritesUseOwner(owner(bank, i));
            if (TexturePack_AddBankSprite(bank, use->page_x, TEXTURE_Y, use->depth, use->clut_x, use->clut_y, 0,
                                          0, width, 256, path)) {
                registered |= 1u << bank;
                laid++;
                what = "laid";
            } else {
                what = "unreadable";
            }
        } else if (on) {
            what = "missing";
        }
        say("hd: card %d stance %d: page %d clut %03x,%03x depth %d %s %s\n", card, position,
            use->page_x / 64, use->clut_x, use->clut_y, use->depth, relative, what);
    }
    TexturePack_BankSpritesUseOwner(0);
    say("hd: card %d stance %d: %d of %d pages laid\n", card, position, laid, count);
}

void HdTextures_Forget(int bank)
{
    int i;
    if (bank < 0 || bank >= 32 || !(registered & (1u << bank))) {
        return;
    }
    for (i = 0; i < USES; i++) {
        TexturePack_BankSpritesClear(owner(bank, i));
    }
    registered &= ~(1u << bank);
}

void HdTextures_ForgetAll(void)
{
    int bank;
    for (bank = 0; registered && bank < 32; bank++) {
        HdTextures_Forget(bank);
    }
}

void HdTextures_Dump(int card, int position, const u16 *block, size_t block_words, const u16 *row,
                     size_t row_words)
{
    char name[64];
    FILE *file;
    if (!host->setting(host, "dump_bank", 0)) {
        return;
    }
    snprintf(name, sizeof(name), "bank-%04d-%d.raw", card, position);
    file = host->open_data(host, name, "wb");
    if (!file) {
        say("hd: card %d: could not write %s\n", card, name);
        return;
    }
    fwrite(block, 2, block_words, file);
    fwrite(row, 2, row_words, file);
    fclose(file);
    say("hd: card %d stance %d: bank written to %s\n", card, position, name);
}
