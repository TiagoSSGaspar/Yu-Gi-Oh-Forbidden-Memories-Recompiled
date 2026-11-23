/* A code mod as the fork's own are: it says mod->api = 4 whatever it was
 * built against, so the loader tells an old copy by its mod.json's version
 * alone, before any of this runs (tests/pc/mods_lifecycle_test.c). */
#include "types.h"
#include "pc/mods/modapi.h"
static const MemoriesModHost *host;
static void frame(void) { host->set_setting(host, "frame_callback", 1); }
int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    host = from;
    host->set_setting(host, "init_ran", 1);
    mod->api = 4;
    mod->frame = frame;
    return 1;
}
