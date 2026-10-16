#ifndef MEMORIES_PC_MESH_MODEL_H
#define MEMORIES_PC_MESH_MODEL_H

/* A glTF binary (.glb, or .gltf with its files beside it) read into what
 * mesh_pass.c draws: the model in its rest pose, each node's mesh placed by
 * its node and each skinned mesh by its joints as they stand, as triangles
 * grouped by material. Positions and normals are turned half round x, from
 * glTF's y up into the console's y down, which also turns the model's front
 * from glTF's +z to -z, where a duel model faces (the player's edge of the
 * mat at yaw 0). No GL here; on macOS this is a native unit (build_arm64.py,
 * ORDINARY), so it is handed and hands back host pointers only. */

/* x y z, nx ny nz, u v */
#define MESH_VERTEX_FLOATS 8

typedef struct MeshPart {
    float *vertices; /* count vertices, three to a triangle */
    int count;
    float color[4];  /* the material's base colour factor */
    int image;       /* its base colour texture in MeshModel.images, or -1 */
} MeshPart;

typedef struct MeshImage {
    unsigned char *rgba; /* width x height, row 0 the image's top */
    int width, height;
} MeshImage;

typedef struct MeshModel {
    MeshPart *parts;
    int part_count;
    MeshImage *images;
    int image_count;
    float min[3], max[3]; /* over every vertex, in the console's axes */
} MeshModel;

/* NULL, with a line in `error`, when the file cannot be read or holds no
 * triangles. An image that cannot be decoded leaves its parts untextured. */
MeshModel *MeshModel_Load(const char *path, char *error, int error_size);
void MeshModel_Free(MeshModel *model);

#endif
