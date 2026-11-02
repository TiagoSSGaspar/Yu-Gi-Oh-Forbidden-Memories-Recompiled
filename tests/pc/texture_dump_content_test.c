/* Provenance by content (found_by_content in texture_dump.c): an upload of
 * bytes copied from a disc delivery, through memory the ring does not know,
 * is traced to the disc offset it came from. Palettes and blocks often
 * start with a run of 0x0000, which every delivery holds too; the newest
 * delivery holding the bytes names them, and bytes no delivery holds are
 * not traced. Native 32/64-bit hosts, without the translated runtime. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "pc/render/texture_dump.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const uint16_t *SoftGpu_Vram(void) { return NULL; }

#define SECTOR_WORDS 1024

static uint16_t delivered_a[SECTOR_WORDS], delivered_b[SECTOR_WORDS], filler[SECTOR_WORDS];
static uint16_t upload[256];

static uint32_t tag(int x, int y) { return TextureDump_Tags[(size_t)y * SOFT_GPU_WIDTH + (size_t)x]; }

int main(void)
{
    size_t i;
    int n;
    assert(TextureDump_EnableTags());
    /* A sector: zeros in runs, and a palette row of 256 words that starts
     * with 40 of them, at word 300. */
    for (i = 0; i < SECTOR_WORDS; i++) delivered_a[i] = (uint16_t)((i / 7) % 3 ? 0 : 0x1234 + i);
    for (i = 0; i < 256; i++) delivered_a[300 + i] = (uint16_t)(i < 40 ? 0 : 0x4000 + i * 3);
    TextureDump_Delivered(delivered_a, sizeof(delivered_a), 10, 0);
    /* Many other deliveries after it, full of zeros and other words. */
    for (n = 0; n < 200; n++) {
        for (i = 0; i < SECTOR_WORDS; i++) filler[i] = (uint16_t)(i % 5 ? 0 : n * 31 + i);
        TextureDump_Delivered(filler, sizeof(filler), 100 + n, 0);
    }
    /* The row copied elsewhere, then uploaded: traced to sector 10, word 300. */
    memcpy(upload, delivered_a + 300, sizeof(upload));
    TextureDump_Loaded(512, 242, 256, 1, upload);
    assert(tag(512, 242) == 10u * 2048u + 600u + 1u);
    assert(tag(512 + 255, 242) == 10u * 2048u + 600u + 255u * 2u + 1u);
    /* The same bytes delivered again later: the newest names them. */
    memcpy(delivered_b, delivered_a, sizeof(delivered_b));
    TextureDump_Delivered(delivered_b, sizeof(delivered_b), 20, 0);
    TextureDump_Loaded(512, 243, 256, 1, upload);
    assert(tag(512, 243) == 20u * 2048u + 600u + 1u);
    /* One word different (past the zeros): no delivery holds it. */
    upload[200] ^= 1;
    TextureDump_Loaded(512, 244, 256, 1, upload);
    assert(tag(512, 244) == 0 && tag(512 + 255, 244) == 0);
    /* A single value is not looked for: it would match anywhere. */
    memset(upload, 0, sizeof(upload));
    TextureDump_Loaded(512, 245, 256, 1, upload);
    assert(tag(512, 245) == 0);
    puts("Texture provenance by content: copies traced past leading zeros, the newest delivery, misses untraced");
    return 0;
}
