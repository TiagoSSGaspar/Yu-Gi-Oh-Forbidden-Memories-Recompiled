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

/* 1: set_copies. New entries only go at the end, so check version before
 * calling a later one. */
#define MONSTERS_3D_API_VERSION 1
#define MONSTERS_3D_COPIES 12   /* the most copies of one monster */

typedef struct {
    unsigned version;   /* MONSTERS_3D_API_VERSION */
    /* The copies, or NULL for none. One mod sets it. */
    void (*set_copies)(Monsters3dCopies copies);
} Monsters3dApi;
#endif
