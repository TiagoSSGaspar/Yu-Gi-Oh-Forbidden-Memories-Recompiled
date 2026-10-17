#ifndef MEMORIES_PC_MESH_PASS_H
#define MEMORIES_PC_MESH_PASS_H
#include <stdint.h>

/* glTF models drawn by the OpenGL picture (gl_picture.h) in place of a
 * model the game, or a mod, would draw from its own polygons.
 *
 * A mod loads a model (MeshPass_Load), sizes it against the polygons it
 * stands in for (MeshPass_Fit), and then, each time it would sort those
 * polygons, submits the model with the matrix they would be drawn with
 * (MeshPass_Submit). Submitting gives back one word, a GP0 no-op (command
 * 0) carrying MESH_PASS_MAGIC and a handle, which the mod puts in the
 * ordering table where the polygons would have gone. The software GPU skips
 * it; the picture's replay meets it in the frame's order and draws the model
 * there, with a depth buffer of its own, projected as the GTE projects:
 * screen = offset + H * camera / camera z. So the model lands where its
 * polygons would have and is layered against the rest of the scene as they
 * would have been.
 *
 * Only the picture draws models, so only at an internal resolution of 2x and
 * up (MeshPass_Ready); below it, or when MeshPass_Submit gives back 0, the
 * caller draws its polygons as ever. */

#define MESH_PASS_MAGIC 0xD3u

/* The console's MATRIX: a 3x3 rotation in 4.12 fixed point, then a
 * translation (libgte.h). */
typedef struct MeshPassMatrix {
    int16_t m[3][3];
    int16_t pad;
    int32_t t[3];
} MeshPassMatrix;

/* The model in the glTF file at `path` (mesh_model.h), read once: its id,
 * above 0, or 0 when it cannot be read (said once, and not tried again). */
int MeshPass_Load(const char *path);
/* Sizes model `model` against the polygons it replaces, which `ls` (their
 * root's local-to-screen matrix, GsGetLsUnit) projected `height_px` lines
 * high with the GTE as it is now. The model's feet (the middle of its lowest
 * face) go at feet_x, feet_y, feet_z in the polygons' own units, it is
 * turned `yaw` (4096 a whole turn) about its upright axis, and made
 * `scale_percent` of the fitted size. 0 if it projects to nothing. */
int MeshPass_Fit(int model, const MeshPassMatrix *ls, int height_px, int feet_x, int feet_y, int feet_z, int yaw,
                 int scale_percent);
/* Whether a submitted model would be drawn: the picture is on at 2x and up
 * and the pass works. */
int MeshPass_Ready(void);
/* The marker word for model `model` drawn through `ls` with the GTE's
 * projection as it is now, faded `fade` (0 opaque to 255 gone); 0 when it
 * would not be drawn, and the caller's polygons should be. It stands in its
 * rest pose. */
uint32_t MeshPass_Submit(int model, const MeshPassMatrix *ls, int fade);
/* The same, posed as clip `clip` has it `ms` milliseconds in (mesh_model.h);
 * a clip it does not have leaves it in its rest pose. */
uint32_t MeshPass_SubmitPosed(int model, const MeshPassMatrix *ls, int fade, int clip, int ms);
/* Model `model`'s clip called `name`: its index, or -1. */
int MeshPass_Clip(int model, const char *name);
/* Its length and when its blow lands, in milliseconds; 0 for no such clip. */
int MeshPass_ClipLength(int model, int clip);
int MeshPass_ClipStrike(int model, int clip);

/* For gl_picture.c. Where the replay is drawing: the framebuffer, its size in
 * pixels and samples, the picture's scale, and the GPU's drawing offset and
 * clip (in VRAM words) at the marker. */
typedef struct MeshPassTarget {
    unsigned fbo;
    int width, height, scale, samples;
    int offset_x, offset_y;
    int clip_x1, clip_y1, clip_x2, clip_y2;
} MeshPassTarget;
int MeshPass_IsMarker(uint32_t word);
/* Draws the model `word` was handed out for, with the GL context current.
 * Leaves the depth test, blending, the scissor and the colour mask off or
 * reset, its program, buffers and vertex array unbound, and texture unit 0
 * active with nothing bound: the caller binds its own again. */
void MeshPass_Draw(uint32_t word, const MeshPassTarget *target);
/* The context went with its names; and the pass stops, deleting them. */
void MeshPass_GlLost(void);
void MeshPass_GlStop(void);

#endif
