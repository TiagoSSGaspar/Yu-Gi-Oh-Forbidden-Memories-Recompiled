#ifndef MONSTERS_3D_H
#define MONSTERS_3D_H
/* What the 3D Monsters mod shares with other mods, as
 * host->find(host, "3d-monsters:api"). It is optional: a mod that uses it
 * finds it once 3D Monsters is loaded (it need not "require" it), and does
 * without when find gives NULL. */

/* One more of a standing monster: where on the field (the field's own units,
 * as D_800908A0 gives the zones) and its turn from the monster's own
 * facing (MODEL_ANGLE_FULL_TURN is a whole turn). */
typedef struct {
    short x, z, yaw;
} Monsters3dCopy;

/* How many more of the monster standing on field record `record` to draw
 * this frame, at most `room`, written to `copies`; it stands at `x`, `z`.
 * Asked for every monster standing, every frame the field is drawn. */
typedef int (*Monsters3dCopies)(int record, int x, int z, Monsters3dCopy *copies, int room);

/* Another card's model drawn with a standing monster: where (as a copy's,
 * with `y` field units above where the monster stands), its turn and its
 * size (`share` of its own fitted size, 4096 the whole). */
typedef struct {
    short card, x, y, z, yaw, share;
} Monsters3dExtra;

/* The other models drawn with the monster standing on field record `record`
 * this frame, at most `room`, written to `extras`; it stands at `x`, `z`.
 * Asked for every monster standing, every frame the field is drawn. */
typedef int (*Monsters3dExtras)(int record, int x, int z, Monsters3dExtra *extras, int room);

/* The colour the monster standing on field record `record` is drawn in:
 * each of `rgb` multiplies its own (128 leaves it as it is). 0 for none. */
typedef int (*Monsters3dTint)(int record, unsigned char rgb[3]);

/* 1: set_copies; 2: set_extras, set_tint. New entries only go at the end, so
 * check version before calling a later one. */
#define MONSTERS_3D_API_VERSION 2
#define MONSTERS_3D_COPIES 12   /* the most copies of one monster */
#define MONSTERS_3D_EXTRAS 4    /* the most other models with one monster */

typedef struct {
    unsigned version;   /* MONSTERS_3D_API_VERSION */
    /* The copies, or NULL for none. One mod sets it. */
    void (*set_copies)(Monsters3dCopies copies);
    /* Version 2: the other models, or NULL; the colour, or NULL. One mod
     * sets each. */
    void (*set_extras)(Monsters3dExtras extras);
    void (*set_tint)(Monsters3dTint tint);
} Monsters3dApi;
#endif
