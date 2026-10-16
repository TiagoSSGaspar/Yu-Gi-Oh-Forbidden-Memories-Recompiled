/* glTF models for the monsters, from files the mod carries.
 *
 * models/<card>.glb (four digits, the card's id) stands in for that card's
 * battle model wherever this mod draws it: the mesh pass (mesh_pass.h)
 * draws it at Internal 2x and up, where the model's polygons would have
 * gone, and below that the polygons are drawn as ever. Its size is fitted
 * once against the polygons' own height on the screen, its feet go where
 * the body's are, and `glb_yaw` (4096 a turn) and `glb_scale` (percent)
 * adjust it. Setting `glb_models` switches them all. */
#include "glb_models.h"
#include "pc/render/mesh_pass.h"
#include <stdarg.h>
#include <stdio.h>

#define KNOWN 64

static const MemoriesModHost *host;
static struct {
    int card, model; /* model 0: none for that card */
    int fitted;      /* sized already: by the first stance loaded */
} known[KNOWN];
static int known_count;

void GlbModels_Init(const MemoriesModHost *from)
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

int GlbModels_Load(int card)
{
    char relative[32], path[1024];
    FILE *file;
    int model = 0, i;
    for (i = 0; i < known_count; i++) {
        if (known[i].card == card) return known[i].model;
    }
    snprintf(relative, sizeof(relative), "models/%04d.glb", card);
    file = host->open_asset(host, relative);
    if (file) {
        fclose(file);
        snprintf(path, sizeof(path), "%s/%s", host->directory, relative);
        model = MeshPass_Load(path);
        say("glb: card %d: %s %s\n", card, relative, model ? "loaded" : "unreadable");
    }
    if (known_count < KNOWN) {
        known[known_count].card = card;
        known[known_count].model = model;
        known[known_count].fitted = 0;
        known_count++;
    }
    return model;
}

/* Once a model: each stance of a card is loaded and fitted on its own, and
 * the defence stance measures taller than the attack one, but the glTF model
 * is the same in both and keeps the size the first gave it. */
void GlbModels_Fit(int model, int card, const MATRIX *ls, int height, int x, int y, int z)
{
    int yaw = host->setting(host, "glb_yaw", 0), scale = host->setting(host, "glb_scale", 100), i, at = -1;
    if (!model) return;
    for (i = 0; i < known_count && at < 0; i++) {
        if (known[i].card == card) at = i;
    }
    if (at >= 0 && known[at].fitted) return;
    if (MeshPass_Fit(model, (const MeshPassMatrix *)ls, height, x, y, z, yaw, scale)) {
        if (at >= 0) known[at].fitted = 1;
        say("glb: card %d fitted to %d lines, feet at %d,%d,%d, yaw %d, %d%%\n", card, height, x, y, z, yaw, scale);
    } else {
        say("glb: card %d could not be fitted (%d lines)\n", card, height);
    }
}

u32 GlbModels_Marker(int model, const MATRIX *ls, int fade)
{
    if (!model || !host->setting(host, "glb_models", 1)) return 0;
    return MeshPass_Submit(model, (const MeshPassMatrix *)ls, fade);
}
