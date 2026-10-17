/* glTF models in their rest pose (mesh_model.h). The parsing is Johannes
 * Kuhlmann's cgltf and the image decoding Sean Barrett's stb_image, both
 * vendored unchanged in src/pc/third_party (see its README.md); their
 * warnings are the upstream files' and are not ours to fix. */
#include "pc/compat/fs.h"
#define CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "pc/third_party/cgltf.h"
#include "pc/third_party/stb_image.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "mesh_model.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 4x4 matrices are glTF's: column-major, 16 floats. */
static void multiply(const float *a, const float *b, float *out)
{
    float c[16];
    int row, column, k;
    for (column = 0; column < 4; column++) {
        for (row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (k = 0; k < 4; k++) sum += a[k * 4 + row] * b[column * 4 + k];
            c[column * 4 + row] = sum;
        }
    }
    memcpy(out, c, sizeof(c));
}

static void transform_point(const float *m, const float *in, float *out)
{
    int i;
    for (i = 0; i < 3; i++) out[i] = m[i] * in[0] + m[4 + i] * in[1] + m[8 + i] * in[2] + m[12 + i];
}

/* By the inverse transpose of m's 3x3: a normal stays square to its surface
 * under a scale that is not the same along every axis. */
static void transform_normal(const float *m, const float *in, float *out)
{
    float a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], k = m[10];
    /* rows of the inverse transpose are the cofactors */
    float r[3][3] = {{e * k - f * h, f * g - d * k, d * h - e * g},
                     {c * h - b * k, a * k - c * g, b * g - a * h},
                     {b * f - c * e, c * d - a * f, a * e - b * d}};
    float length;
    int i;
    for (i = 0; i < 3; i++) out[i] = r[i][0] * in[0] + r[i][1] * in[1] + r[i][2] * in[2];
    length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (length > 0.0f) {
        for (i = 0; i < 3; i++) out[i] /= length;
    } else {
        out[0] = 0.0f;
        out[1] = 1.0f;
        out[2] = 0.0f;
    }
}

/* A vertex array that grows as triangles are added to its material, and
 * beside it, for posing, each vertex as the file has it (position and normal,
 * before any node or joint moves it) and the four palette entries that move
 * it, with their weights. */
typedef struct Building {
    const cgltf_material *material;
    float *vertices;
    float *source;
    unsigned short *which;
    float *weight;
    int count, room;
} Building;

static int grow(void **array, int room, size_t size)
{
    void *more = realloc(*array, (size_t)room * size);
    if (!more) return 0;
    *array = more;
    return 1;
}

/* Room for `vertices` more: the index of the first, or -1. */
static int room_for(Building *part, int vertices)
{
    if (part->count + vertices > part->room) {
        int room = part->room ? part->room * 2 : 3072;
        while (room < part->count + vertices) room *= 2;
        if (!grow((void **)&part->vertices, room, MESH_VERTEX_FLOATS * sizeof(float)) ||
            !grow((void **)&part->source, room, 6 * sizeof(float)) ||
            !grow((void **)&part->which, room, 4 * sizeof(unsigned short)) ||
            !grow((void **)&part->weight, room, 4 * sizeof(float)))
            return -1;
        part->room = room;
    }
    part->count += vertices;
    return part->count - vertices;
}

static Building *part_for(Building **parts, int *count, const cgltf_material *material)
{
    Building *more;
    int i;
    for (i = 0; i < *count; i++) {
        if ((*parts)[i].material == material) return &(*parts)[i];
    }
    more = realloc(*parts, (size_t)(*count + 1) * sizeof(*more));
    if (!more) return NULL;
    *parts = more;
    memset(&more[*count], 0, sizeof(*more));
    more[*count].material = material;
    return &more[(*count)++];
}

/* The matrix that places a vertex of `node`'s mesh: its joints' as they
 * stand, weighted, for a skinned mesh; the node's own world matrix else. */
static void vertex_matrix(const cgltf_node *node, const float *joint_matrices, const cgltf_accessor *joints,
                          const cgltf_accessor *weights, cgltf_size index, const float *node_world, float *out)
{
    cgltf_uint which[4] = {0, 0, 0, 0};
    float weight[4] = {0, 0, 0, 0}, total = 0.0f;
    int i, k;
    if (!node->skin || !joint_matrices || !joints || !weights) {
        memcpy(out, node_world, 16 * sizeof(float));
        return;
    }
    cgltf_accessor_read_uint(joints, index, which, 4);
    cgltf_accessor_read_float(weights, index, weight, 4);
    memset(out, 0, 16 * sizeof(float));
    for (i = 0; i < 4; i++) {
        if (weight[i] <= 0.0f || which[i] >= node->skin->joints_count) continue;
        for (k = 0; k < 16; k++) out[k] += weight[i] * joint_matrices[which[i] * 16 + k];
        total += weight[i];
    }
    if (total <= 0.0f) {
        memcpy(out, node_world, 16 * sizeof(float));
    } else if (total != 1.0f) {
        for (k = 0; k < 16; k++) out[k] /= total;
    }
}

/* Each joint's world matrix times its inverse bind matrix: in the rest pose,
 * where a skinned vertex stands. NULL for a node without a skin. */
static float *joint_matrices_of(const cgltf_skin *skin)
{
    float *out;
    cgltf_size i;
    if (!skin || !skin->joints_count) return NULL;
    out = malloc(skin->joints_count * 16 * sizeof(float));
    if (!out) return NULL;
    for (i = 0; i < skin->joints_count; i++) {
        float world[16], inverse_bind[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        cgltf_node_transform_world(skin->joints[i], world);
        if (skin->inverse_bind_matrices) cgltf_accessor_read_float(skin->inverse_bind_matrices, i, inverse_bind, 16);
        multiply(world, inverse_bind, out + i * 16);
    }
    return out;
}

/* Each triangle's own facing as all three of its vertices' normals. */
static void flat_normals(float *vertices, cgltf_size triangles)
{
    cgltf_size i;
    for (i = 0; i < triangles; i++) {
        float *a = vertices + i * 3 * MESH_VERTEX_FLOATS, *b = a + MESH_VERTEX_FLOATS, *c = b + MESH_VERTEX_FLOATS;
        float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        float f[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        float length = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
        int k;
        if (length > 0.0f) {
            f[0] /= length;
            f[1] /= length;
            f[2] /= length;
        } else {
            f[0] = 0.0f;
            f[1] = -1.0f;
            f[2] = 0.0f;
        }
        for (k = 0; k < 3; k++) memcpy(a + k * MESH_VERTEX_FLOATS + 3, f, sizeof(f));
    }
}

/* A primitive's triangles into its material's part. `skin_entry` is the
 * palette entry of its skin's first joint (-1 for none), `own_entry` that of
 * its own node, which moves it when it has no skin. */
static int add_primitive(Building **parts, int *part_count, const cgltf_node *node, const cgltf_primitive *primitive,
                         const float *node_world, const float *joint_matrices, int skin_entry, int own_entry)
{
    const cgltf_accessor *position = NULL, *normal = NULL, *texcoord = NULL, *joints = NULL, *weights = NULL;
    cgltf_size i, triangles;
    Building *part;
    float *out;
    int first, skinned;
    if (primitive->type != cgltf_primitive_type_triangles) return 1;
    for (i = 0; i < primitive->attributes_count; i++) {
        const cgltf_attribute *attribute = &primitive->attributes[i];
        if (attribute->type == cgltf_attribute_type_position) position = attribute->data;
        else if (attribute->type == cgltf_attribute_type_normal) normal = attribute->data;
        else if (attribute->type == cgltf_attribute_type_texcoord && attribute->index == 0) texcoord = attribute->data;
        else if (attribute->type == cgltf_attribute_type_joints && attribute->index == 0) joints = attribute->data;
        else if (attribute->type == cgltf_attribute_type_weights && attribute->index == 0) weights = attribute->data;
    }
    if (!position) return 1;
    triangles = (primitive->indices ? primitive->indices->count : position->count) / 3;
    if (!triangles) return 1;
    part = part_for(parts, part_count, primitive->material);
    if (!part || (first = room_for(part, (int)triangles * 3)) < 0) return 0;
    out = part->vertices + (size_t)first * MESH_VERTEX_FLOATS;
    skinned = node->skin && joints && weights && skin_entry >= 0;
    for (i = 0; i < triangles * 3; i++) {
        cgltf_size index = primitive->indices ? cgltf_accessor_read_index(primitive->indices, i) : i;
        float p[3] = {0, 0, 0}, n[3] = {0, 1, 0}, uv[2] = {0, 0}, m[16], placed[3], turned[3];
        float *v = out + i * MESH_VERTEX_FLOATS;
        float *source = part->source + ((size_t)first + i) * 6, *weight = part->weight + ((size_t)first + i) * 4;
        unsigned short *which = part->which + ((size_t)first + i) * 4;
        int k;
        cgltf_accessor_read_float(position, index, p, 3);
        if (normal) cgltf_accessor_read_float(normal, index, n, 3);
        if (texcoord) cgltf_accessor_read_float(texcoord, index, uv, 2);
        memcpy(source, p, sizeof(p));
        /* no normal: a zero one, which posing reads as the face's own */
        if (normal) memcpy(source + 3, n, sizeof(n));
        else source[3] = source[4] = source[5] = 0.0f;
        if (skinned) {
            cgltf_uint joint[4] = {0, 0, 0, 0};
            cgltf_accessor_read_uint(joints, index, joint, 4);
            cgltf_accessor_read_float(weights, index, weight, 4);
            for (k = 0; k < 4; k++) {
                which[k] = (unsigned short)(skin_entry + (joint[k] < node->skin->joints_count ? (int)joint[k] : 0));
                if (joint[k] >= node->skin->joints_count) weight[k] = 0.0f;
            }
        } else {
            which[0] = (unsigned short)own_entry;
            which[1] = which[2] = which[3] = 0;
            weight[0] = 1.0f;
            weight[1] = weight[2] = weight[3] = 0.0f;
        }
        vertex_matrix(node, joint_matrices, joints, weights, index, node_world, m);
        transform_point(m, p, placed);
        transform_normal(m, n, turned);
        /* glTF's y up and the model's front towards +z; a duel model's y
         * down and its front towards -z: half a turn round x */
        v[0] = placed[0];
        v[1] = -placed[1];
        v[2] = -placed[2];
        v[3] = turned[0];
        v[4] = -turned[1];
        v[5] = -turned[2];
        v[6] = uv[0];
        v[7] = uv[1];
    }
    if (!normal) flat_normals(out, triangles); /* flat: each triangle's own facing */
    return 1;
}

/* An image's bytes: in the binary, or a file beside the model. */
static unsigned char *image_bytes(const cgltf_image *image, const char *path, size_t *size, int *owned)
{
    *owned = 0;
    if (image->buffer_view) {
        *size = image->buffer_view->size;
        return (unsigned char *)cgltf_buffer_view_data(image->buffer_view);
    }
    if (image->uri && strncmp(image->uri, "data:", 5)) {
        char full[1024];
        const char *slash = strrchr(path, '/');
        size_t folder = slash ? (size_t)(slash - path + 1) : 0;
        unsigned char *bytes;
        long length;
        FILE *file;
        if (folder + strlen(image->uri) + 1 > sizeof(full)) return NULL;
        memcpy(full, path, folder);
        strcpy(full + folder, image->uri);
        cgltf_decode_uri(full + folder);
        file = fopen(full, "rb");
        if (!file) return NULL;
        fseek(file, 0, SEEK_END);
        length = ftell(file);
        fseek(file, 0, SEEK_SET);
        bytes = length > 0 ? malloc((size_t)length) : NULL;
        if (bytes && fread(bytes, 1, (size_t)length, file) != (size_t)length) {
            free(bytes);
            bytes = NULL;
        }
        fclose(file);
        *size = (size_t)length;
        *owned = bytes != NULL;
        return bytes;
    }
    return NULL;
}

static void load_images(MeshModel *model, const cgltf_data *data, const char *path)
{
    cgltf_size i;
    if (!data->images_count) return;
    model->images = calloc(data->images_count, sizeof(*model->images));
    if (!model->images) return;
    model->image_count = (int)data->images_count;
    for (i = 0; i < data->images_count; i++) {
        size_t size = 0;
        int owned, channels;
        unsigned char *bytes = image_bytes(&data->images[i], path, &size, &owned);
        if (bytes && size && size < 0x7fffffff) {
            model->images[i].rgba = stbi_load_from_memory(bytes, (int)size, &model->images[i].width,
                                                          &model->images[i].height, &channels, 4);
        }
        if (owned) free(bytes);
        if (!model->images[i].rgba) {
            fprintf(stderr, "memories-pc: mesh model %s: image %d not decoded (%s)\n", path, (int)i,
                    stbi_failure_reason() ? stbi_failure_reason() : "no data");
            model->images[i].width = model->images[i].height = 0;
        }
    }
}

/* --- clips ---------------------------------------------------------------- */

/* One node's translation, rotation or scale through a clip. */
typedef struct Channel {
    int node, path;        /* path: 0 translation, 1 rotation, 2 scale */
    int step;              /* held from key to key; else linear (a cubic spline's values, unbent) */
    int count;
    float *times, *values; /* values: 4 a key for a rotation, 3 else */
} Channel;

typedef struct Clip {
    char name[64];
    int length, strike; /* ms */
    Channel *channels;
    int channel_count;
    MeshEffect *effects;
    int effect_count;
} Clip;

/* A posable model: its nodes as they stand, the matrices a vertex can follow
 * (each skin's joints times their inverse binds, then each mesh's own node),
 * and per part, per vertex, where it stands before them and which follow it
 * (the parts' Building arrays, kept). */
struct MeshAnimation {
    int node_count;
    int *parent;            /* -1: a root */
    float (*rest)[10];      /* translation, rotation (x y z w), scale */
    float (*fixed)[16];     /* a node given by a matrix keeps it */
    unsigned char *is_fixed;
    int palette_count;
    int *palette_node;
    float (*palette_inverse)[16];
    float **source;         /* per part: x y z nx ny nz a vertex; a zero normal is the face's own */
    unsigned short **which; /* per part: four palette entries a vertex */
    float **weight;         /* per part: their four weights */
    int part_count;
    Clip *clips;
    int clip_count;
};

static void free_animation(MeshAnimation *anim)
{
    int i, k;
    if (!anim) return;
    for (i = 0; i < anim->part_count; i++) {
        if (anim->source) free(anim->source[i]);
        if (anim->which) free(anim->which[i]);
        if (anim->weight) free(anim->weight[i]);
    }
    for (i = 0; i < anim->clip_count; i++) {
        for (k = 0; k < anim->clips[i].channel_count; k++) {
            free(anim->clips[i].channels[k].times);
            free(anim->clips[i].channels[k].values);
        }
        free(anim->clips[i].channels);
        free(anim->clips[i].effects);
    }
    free(anim->clips);
    free(anim->source);
    free(anim->which);
    free(anim->weight);
    free(anim->palette_node);
    free(anim->palette_inverse);
    free(anim->parent);
    free(anim->rest);
    free(anim->fixed);
    free(anim->is_fixed);
    free(anim);
}

/* An accessor's floats, unpacked: NULL when there are none. */
static float *unpack(const cgltf_accessor *accessor, cgltf_size *count)
{
    cgltf_size floats = accessor ? cgltf_accessor_unpack_floats(accessor, NULL, 0) : 0;
    float *out = floats ? malloc(floats * sizeof(float)) : NULL;
    if (out) cgltf_accessor_unpack_floats(accessor, out, floats);
    *count = out ? floats : 0;
    return out;
}

/* An animation's extras as text, to be freed; NULL for none. They are this
 * port's own, written by its converter:
 *   {"strike": seconds,
 *    "effects": [{"kind": "glow" | "beam" | "burst", "node": n (-1 the target),
 *                 "offset": [x, y, z], "from": s, "to": s, "size": units,
 *                 "image": "name", "grid": [columns, rows], "color": [r, g, b, a]}]}
 * with times from the strike in real seconds. Read by looking for the keys,
 * not by a JSON parser: nothing else is written there. */
static char *extras_text(const cgltf_data *data, const cgltf_extras *extras)
{
    cgltf_size size;
    char *text;
    if (extras->data) {
        size = strlen(extras->data);
        text = malloc(size + 1);
        if (text) memcpy(text, extras->data, size + 1);
        return text;
    }
    size = extras->end_offset > extras->start_offset ? extras->end_offset - extras->start_offset : 0;
    if (!size || !data->json) return NULL;
    text = malloc(size + 1);
    if (!text) return NULL;
    memcpy(text, data->json + extras->start_offset, size);
    text[size] = 0;
    return text;
}

/* Just past the colon after "key" in [from, to), or NULL. */
static const char *json_field(const char *from, const char *to, const char *key)
{
    size_t length = strlen(key);
    const char *at;
    for (at = from; at + length + 2 < to; at++) {
        if (at[0] == '"' && !strncmp(at + 1, key, length) && at[length + 1] == '"') {
            at += length + 2;
            while (at < to && (*at == ' ' || *at == ':')) at++;
            return at < to ? at : NULL;
        }
    }
    return NULL;
}

static float json_number(const char *from, const char *to, const char *key, float otherwise)
{
    const char *at = json_field(from, to, key);
    return at ? strtof(at, NULL) : otherwise;
}

/* Up to `count` numbers of the array after "key"; how many were read. */
static int json_numbers(const char *from, const char *to, const char *key, float *out, int count)
{
    const char *at = json_field(from, to, key);
    char *end;
    int n = 0;
    if (!at || *at != '[') return 0;
    at++;
    while (n < count && at < to && *at != ']') {
        float value = strtof(at, &end);
        if (end == at) break;
        out[n++] = value;
        at = end;
        while (at < to && (*at == ',' || *at == ' ')) at++;
    }
    return n;
}

static void read_effects(Clip *clip, const char *text, const cgltf_data *data)
{
    const char *at = json_field(text, text + strlen(text), "effects"), *end = text + strlen(text);
    int room = 0;
    if (!at || *at != '[') return;
    while ((at = strchr(at, '{')) != NULL) {
        const char *close = strchr(at, '}'), *kind, *image;
        MeshEffect *effect, *more;
        float grid[2] = {1, 1};
        if (!close || close > end) break;
        if (clip->effect_count == room) {
            room = room ? room * 2 : 4;
            more = realloc(clip->effects, (size_t)room * sizeof(*more));
            if (!more) return;
            clip->effects = more;
        }
        effect = &clip->effects[clip->effect_count];
        memset(effect, 0, sizeof(*effect));
        kind = json_field(at, close, "kind");
        if (kind && !strncmp(kind, "\"beam\"", 6)) effect->kind = MESH_EFFECT_BEAM;
        else if (kind && !strncmp(kind, "\"burst\"", 7)) effect->kind = MESH_EFFECT_BURST;
        else if (kind && !strncmp(kind, "\"glow\"", 6)) effect->kind = MESH_EFFECT_GLOW;
        else {
            at = close;
            continue;
        }
        effect->node = (int)json_number(at, close, "node", -1.0f);
        if (effect->node >= (int)data->nodes_count) effect->node = -1;
        json_numbers(at, close, "offset", effect->offset, 3);
        effect->from = (int)(json_number(at, close, "from", 0.0f) * 1000.0f);
        effect->to = (int)(json_number(at, close, "to", 0.0f) * 1000.0f);
        effect->size = json_number(at, close, "size", 1.0f);
        json_numbers(at, close, "grid", grid, 2);
        effect->columns = grid[0] >= 1.0f ? (int)grid[0] : 1;
        effect->rows = grid[1] >= 1.0f ? (int)grid[1] : 1;
        effect->color[0] = effect->color[1] = effect->color[2] = effect->color[3] = 1.0f;
        json_numbers(at, close, "color", effect->color, 4);
        effect->image = -1;
        image = json_field(at, close, "image");
        if (image && *image == '"') {
            cgltf_size i;
            const char *name_end = strchr(image + 1, '"');
            for (i = 0; name_end && i < data->images_count; i++) {
                const char *name = data->images[i].name;
                if (name && strlen(name) == (size_t)(name_end - image - 1) && !strncmp(name, image + 1, strlen(name)))
                    effect->image = (int)i;
            }
        }
        if (effect->to > effect->from) clip->effect_count++;
        at = close;
    }
}

static int read_channel(Channel *out, const cgltf_data *data, const cgltf_animation_channel *channel)
{
    const cgltf_animation_sampler *sampler = channel->sampler;
    cgltf_size times, values, width, k;
    if (!sampler || !channel->target_node) return 0;
    if (channel->target_path == cgltf_animation_path_type_translation) out->path = 0;
    else if (channel->target_path == cgltf_animation_path_type_rotation) out->path = 1;
    else if (channel->target_path == cgltf_animation_path_type_scale) out->path = 2;
    else return 0;
    width = out->path == 1 ? 4 : 3;
    out->node = (int)(channel->target_node - data->nodes);
    out->step = sampler->interpolation == cgltf_interpolation_type_step;
    out->times = unpack(sampler->input, &times);
    out->values = unpack(sampler->output, &values);
    if (sampler->interpolation == cgltf_interpolation_type_cubic_spline && out->values && values == times * width * 3) {
        /* in-tangent, value, out-tangent a key: the values alone */
        for (k = 0; k < times; k++) memmove(out->values + k * width, out->values + (k * 3 + 1) * width, width * sizeof(float));
        values = times * width;
    }
    if (!out->times || !out->values || !times || values < times * width) {
        free(out->times);
        free(out->values);
        return 0;
    }
    out->count = (int)times;
    return 1;
}

static void read_clips(MeshAnimation *anim, const cgltf_data *data)
{
    cgltf_size a, c;
    anim->clips = calloc(data->animations_count, sizeof(*anim->clips));
    if (!anim->clips) return;
    for (a = 0; a < data->animations_count; a++) {
        const cgltf_animation *animation = &data->animations[a];
        Clip *clip = &anim->clips[anim->clip_count];
        float last = 0.0f, strike = -1.0f;
        char *extras;
        clip->channels = calloc(animation->channels_count ? animation->channels_count : 1, sizeof(*clip->channels));
        if (!clip->channels) continue;
        for (c = 0; c < animation->channels_count; c++) {
            Channel *channel = &clip->channels[clip->channel_count];
            if (!read_channel(channel, data, &animation->channels[c])) continue;
            if (channel->times[channel->count - 1] > last) last = channel->times[channel->count - 1];
            clip->channel_count++;
        }
        if (animation->name) snprintf(clip->name, sizeof(clip->name), "%s", animation->name);
        else snprintf(clip->name, sizeof(clip->name), "clip%d", (int)a);
        clip->length = (int)(last * 1000.0f + 0.5f);
        extras = extras_text(data, &animation->extras);
        if (extras) {
            strike = json_number(extras, extras + strlen(extras), "strike", -1.0f);
            read_effects(clip, extras, data);
            free(extras);
        }
        clip->strike = strike >= 0.0f ? (int)(strike * 1000.0f + 0.5f) : clip->length / 2;
        if (clip->strike > clip->length) clip->strike = clip->length;
        anim->clip_count++;
    }
}

/* The nodes as they stand, and the palette: each skin's joints from
 * skin_entry[skin], then each mesh node's own at own_entry[node]. */
static MeshAnimation *read_nodes(const cgltf_data *data, int *skin_entry, int *own_entry)
{
    MeshAnimation *anim = calloc(1, sizeof(*anim));
    cgltf_size n, s, j;
    int entries = 0;
    if (!anim) return NULL;
    for (s = 0; s < data->skins_count; s++) entries += (int)data->skins[s].joints_count;
    for (n = 0; n < data->nodes_count; n++) entries += data->nodes[n].mesh != NULL;
    anim->node_count = (int)data->nodes_count;
    anim->parent = malloc((data->nodes_count ? data->nodes_count : 1) * sizeof(*anim->parent));
    anim->rest = malloc((data->nodes_count ? data->nodes_count : 1) * sizeof(*anim->rest));
    anim->fixed = malloc((data->nodes_count ? data->nodes_count : 1) * sizeof(*anim->fixed));
    anim->is_fixed = calloc(data->nodes_count ? data->nodes_count : 1, 1);
    anim->palette_node = malloc((size_t)(entries ? entries : 1) * sizeof(*anim->palette_node));
    anim->palette_inverse = malloc((size_t)(entries ? entries : 1) * sizeof(*anim->palette_inverse));
    if (!anim->parent || !anim->rest || !anim->fixed || !anim->is_fixed || !anim->palette_node ||
        !anim->palette_inverse) {
        free_animation(anim);
        return NULL;
    }
    for (n = 0; n < data->nodes_count; n++) {
        const cgltf_node *node = &data->nodes[n];
        float *trs = anim->rest[n];
        anim->parent[n] = node->parent ? (int)(node->parent - data->nodes) : -1;
        if (node->has_matrix) {
            anim->is_fixed[n] = 1;
            cgltf_node_transform_local(node, anim->fixed[n]);
        }
        trs[0] = node->has_translation ? node->translation[0] : 0.0f;
        trs[1] = node->has_translation ? node->translation[1] : 0.0f;
        trs[2] = node->has_translation ? node->translation[2] : 0.0f;
        trs[3] = node->has_rotation ? node->rotation[0] : 0.0f;
        trs[4] = node->has_rotation ? node->rotation[1] : 0.0f;
        trs[5] = node->has_rotation ? node->rotation[2] : 0.0f;
        trs[6] = node->has_rotation ? node->rotation[3] : 1.0f;
        trs[7] = node->has_scale ? node->scale[0] : 1.0f;
        trs[8] = node->has_scale ? node->scale[1] : 1.0f;
        trs[9] = node->has_scale ? node->scale[2] : 1.0f;
    }
    for (s = 0; s < data->skins_count; s++) {
        const cgltf_skin *skin = &data->skins[s];
        skin_entry[s] = anim->palette_count;
        for (j = 0; j < skin->joints_count; j++) {
            float *inverse = anim->palette_inverse[anim->palette_count];
            memset(inverse, 0, sizeof(anim->palette_inverse[0]));
            inverse[0] = inverse[5] = inverse[10] = inverse[15] = 1.0f;
            if (skin->inverse_bind_matrices) cgltf_accessor_read_float(skin->inverse_bind_matrices, j, inverse, 16);
            anim->palette_node[anim->palette_count++] = skin->joints[j] ? (int)(skin->joints[j] - data->nodes) : 0;
        }
    }
    for (n = 0; n < data->nodes_count; n++) {
        float *inverse;
        own_entry[n] = -1;
        if (!data->nodes[n].mesh) continue;
        inverse = anim->palette_inverse[anim->palette_count];
        memset(inverse, 0, sizeof(anim->palette_inverse[0]));
        inverse[0] = inverse[5] = inverse[10] = inverse[15] = 1.0f;
        own_entry[n] = anim->palette_count;
        anim->palette_node[anim->palette_count++] = (int)n;
    }
    return anim;
}

static void sample(const Channel *channel, float t, float *out)
{
    int width = channel->path == 1 ? 4 : 3, i = 0, k;
    float u = 0.0f;
    const float *a, *b;
    if (t <= channel->times[0] || channel->count == 1) {
        memcpy(out, channel->values, (size_t)width * sizeof(float));
        return;
    }
    if (t >= channel->times[channel->count - 1]) {
        memcpy(out, channel->values + (size_t)(channel->count - 1) * width, (size_t)width * sizeof(float));
        return;
    }
    while (i + 2 < channel->count && channel->times[i + 1] <= t) i++;
    a = channel->values + (size_t)i * width;
    b = a + width;
    if (!channel->step && channel->times[i + 1] > channel->times[i])
        u = (t - channel->times[i]) / (channel->times[i + 1] - channel->times[i]);
    if (width == 4) {
        /* the shorter way round, then back to unit length */
        float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3], sign = dot < 0.0f ? -1.0f : 1.0f, length;
        for (k = 0; k < 4; k++) out[k] = a[k] + (sign * b[k] - a[k]) * u;
        length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2] + out[3] * out[3]);
        for (k = 0; k < 4 && length > 0.0f; k++) out[k] /= length;
    } else {
        for (k = 0; k < 3; k++) out[k] = a[k] + (b[k] - a[k]) * u;
    }
}

/* translation, rotation (x y z w), scale: the matrix T R S */
static void compose(const float *trs, float *m)
{
    float x = trs[3], y = trs[4], z = trs[5], w = trs[6];
    float r[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w),     2 * (x * z - y * w),
                  2 * (x * y - z * w),     1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                  2 * (x * z + y * w),     2 * (y * z - x * w),     1 - 2 * (x * x + y * y)};
    int column, k;
    for (column = 0; column < 3; column++) {
        for (k = 0; k < 3; k++) m[column * 4 + k] = r[column * 3 + k] * trs[7 + column];
        m[column * 4 + 3] = 0.0f;
    }
    m[12] = trs[0];
    m[13] = trs[1];
    m[14] = trs[2];
    m[15] = 1.0f;
}

static void world_of(const MeshAnimation *anim, float (*local)[16], float (*world)[16], unsigned char *done, int i)
{
    int parent = anim->parent[i];
    if (done[i]) return;
    done[i] = 1; /* a cycle, which a valid file has not, stops here */
    if (parent >= 0) {
        world_of(anim, local, world, done, parent);
        multiply(world[parent], local[i], world[i]);
    } else {
        memcpy(world[i], local[i], sizeof(world[i]));
    }
}

int MeshModel_Clip(const MeshModel *model, const char *name)
{
    int i;
    if (!model || !model->animation || !name) return -1;
    for (i = 0; i < model->animation->clip_count; i++) {
        if (!strcmp(model->animation->clips[i].name, name)) return i;
    }
    return -1;
}

int MeshModel_ClipLength(const MeshModel *model, int clip)
{
    if (!model || !model->animation || clip < 0 || clip >= model->animation->clip_count) return 0;
    return model->animation->clips[clip].length;
}

int MeshModel_ClipStrike(const MeshModel *model, int clip)
{
    if (!model || !model->animation || clip < 0 || clip >= model->animation->clip_count) return 0;
    return model->animation->clips[clip].strike;
}

/* Every node's world matrix with clip `clip` `ms` in: an array to free, or
 * NULL for no such clip (or no room). */
static float (*posed_nodes(const MeshAnimation *anim, int clip, int ms))[16]
{
    const Clip *c;
    float (*trs)[10], (*local)[16], (*world)[16];
    unsigned char *done;
    float t;
    int i, k, nodes;
    if (!anim || clip < 0 || clip >= anim->clip_count) return NULL;
    c = &anim->clips[clip];
    t = (float)(ms < 0 ? 0 : ms > c->length ? c->length : ms) / 1000.0f;
    nodes = anim->node_count ? anim->node_count : 1;
    trs = malloc((size_t)nodes * sizeof(*trs));
    local = malloc((size_t)nodes * sizeof(*local));
    world = malloc((size_t)nodes * sizeof(*world));
    done = calloc((size_t)nodes, 1);
    if (trs && local && world && done) {
        memcpy(trs, anim->rest, (size_t)anim->node_count * sizeof(*trs));
        for (k = 0; k < c->channel_count; k++) {
            const Channel *channel = &c->channels[k];
            sample(channel, t, trs[channel->node] + (channel->path == 0 ? 0 : channel->path == 1 ? 3 : 7));
        }
        for (i = 0; i < anim->node_count; i++) {
            if (anim->is_fixed[i]) memcpy(local[i], anim->fixed[i], sizeof(local[i]));
            else compose(trs[i], local[i]);
        }
        for (i = 0; i < anim->node_count; i++) world_of(anim, local, world, done, i);
    } else {
        free(world);
        world = NULL;
    }
    free(trs);
    free(local);
    free(done);
    return world;
}

int MeshModel_ClipEffects(const MeshModel *model, int clip, const MeshEffect **effects)
{
    if (!model || !model->animation || clip < 0 || clip >= model->animation->clip_count) return 0;
    if (effects) *effects = model->animation->clips[clip].effects;
    return model->animation->clips[clip].effect_count;
}

int MeshModel_NodePoint(const MeshModel *model, int clip, int ms, int node, const float offset[3], float out[3])
{
    const MeshAnimation *anim = model ? model->animation : NULL;
    float (*world)[16], placed[3];
    if (!anim || node < 0 || node >= anim->node_count || !(world = posed_nodes(anim, clip, ms))) return 0;
    transform_point(world[node], offset, placed);
    free(world);
    /* half a turn round x, as the parts */
    out[0] = placed[0];
    out[1] = -placed[1];
    out[2] = -placed[2];
    return 1;
}

int MeshModel_Pose(const MeshModel *model, int clip, int ms, float *const *out)
{
    const MeshAnimation *anim = model ? model->animation : NULL;
    float (*world)[16], (*palette)[16];
    int i, v, k;
    if (!anim || !out || !(world = posed_nodes(anim, clip, ms))) return 0;
    palette = malloc((size_t)(anim->palette_count ? anim->palette_count : 1) * sizeof(*palette));
    if (!palette) {
        free(world);
        return 0;
    }
    for (i = 0; i < anim->palette_count; i++) multiply(world[anim->palette_node[i]], anim->palette_inverse[i], palette[i]);
    for (i = 0; i < model->part_count && i < anim->part_count; i++) {
        const MeshPart *part = &model->parts[i];
        for (v = 0; v < part->count; v++) {
            const float *source = anim->source[i] + (size_t)v * 6;
            const unsigned short *which = anim->which[i] + (size_t)v * 4;
            const float *weight = anim->weight[i] + (size_t)v * 4;
            float m[16], total = 0.0f, placed[3], turned[3];
            float *o = out[i] + (size_t)v * MESH_VERTEX_FLOATS;
            memset(m, 0, sizeof(m));
            for (k = 0; k < 4; k++) {
                int e;
                if (weight[k] <= 0.0f || which[k] >= anim->palette_count) continue;
                for (e = 0; e < 16; e++) m[e] += weight[k] * palette[which[k]][e];
                total += weight[k];
            }
            if (total <= 0.0f) {
                memcpy(m, palette[which[0] < anim->palette_count ? which[0] : 0], sizeof(m));
            } else if (total != 1.0f) {
                for (k = 0; k < 16; k++) m[k] /= total;
            }
            transform_point(m, source, placed);
            transform_normal(m, source + 3, turned);
            /* half a turn round x, as the rest pose (add_primitive) */
            o[0] = placed[0];
            o[1] = -placed[1];
            o[2] = -placed[2];
            o[3] = turned[0];
            o[4] = -turned[1];
            o[5] = -turned[2];
            o[6] = part->vertices[(size_t)v * MESH_VERTEX_FLOATS + 6];
            o[7] = part->vertices[(size_t)v * MESH_VERTEX_FLOATS + 7];
            if (v % 3 == 2 && source[3] == 0.0f && source[4] == 0.0f && source[5] == 0.0f) {
                flat_normals(out[i] + (size_t)(v - 2) * MESH_VERTEX_FLOATS, 1); /* had none: the face's own */
            }
        }
    }
    free(world);
    free(palette);
    return 1;
}

/* --- loading -------------------------------------------------------------- */

MeshModel *MeshModel_Load(const char *path, char *error, int error_size)
{
    cgltf_options options;
    cgltf_data *data = NULL;
    cgltf_result result;
    MeshModel *model;
    MeshAnimation *anim;
    Building *parts = NULL;
    int part_count = 0, i, ok = 1, *skin_entry, *own_entry;
    cgltf_size n;
    memset(&options, 0, sizeof(options));
    if (error && error_size > 0) error[0] = 0;
    result = cgltf_parse_file(&options, path, &data);
    if (result == cgltf_result_success) result = cgltf_load_buffers(&options, data, path);
    if (result == cgltf_result_success) result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        if (error) snprintf(error, (size_t)error_size, "%s: not a usable glTF file (cgltf result %d)", path, (int)result);
        cgltf_free(data);
        return NULL;
    }
    skin_entry = calloc(data->skins_count ? data->skins_count : 1, sizeof(int));
    own_entry = calloc(data->nodes_count ? data->nodes_count : 1, sizeof(int));
    anim = skin_entry && own_entry ? read_nodes(data, skin_entry, own_entry) : NULL;
    ok = anim != NULL;
    for (n = 0; n < data->nodes_count && ok; n++) {
        const cgltf_node *node = &data->nodes[n];
        float world[16], *joints;
        cgltf_size p;
        int skin = node->skin ? skin_entry[node->skin - data->skins] : -1;
        if (!node->mesh) continue;
        cgltf_node_transform_world(node, world);
        joints = joint_matrices_of(node->skin);
        for (p = 0; p < node->mesh->primitives_count && ok; p++) {
            ok = add_primitive(&parts, &part_count, node, &node->mesh->primitives[p], world, joints, skin,
                               own_entry[n]);
        }
        free(joints);
    }
    free(skin_entry);
    free(own_entry);
    model = ok && part_count ? calloc(1, sizeof(*model)) : NULL;
    if (model) model->parts = calloc((size_t)part_count, sizeof(*model->parts));
    if (!model || !model->parts) {
        if (error) snprintf(error, (size_t)error_size, "%s: %s", path, ok ? "no triangles" : "out of memory");
        for (i = 0; i < part_count; i++) {
            free(parts[i].vertices);
            free(parts[i].source);
            free(parts[i].which);
            free(parts[i].weight);
        }
        free(parts);
        if (model) free(model->parts);
        free(model);
        free_animation(anim);
        cgltf_free(data);
        return NULL;
    }
    /* What posing needs, when the file has clips; without, the model stands. */
    if (data->animations_count) read_clips(anim, data);
    anim->source = anim->clip_count ? calloc((size_t)part_count, sizeof(*anim->source)) : NULL;
    anim->which = anim->source ? calloc((size_t)part_count, sizeof(*anim->which)) : NULL;
    anim->weight = anim->which ? calloc((size_t)part_count, sizeof(*anim->weight)) : NULL;
    if (anim->weight) {
        anim->part_count = part_count;
        for (i = 0; i < part_count; i++) {
            anim->source[i] = parts[i].source;
            anim->which[i] = parts[i].which;
            anim->weight[i] = parts[i].weight;
        }
        model->animation = anim;
    } else {
        for (i = 0; i < part_count; i++) {
            free(parts[i].source);
            free(parts[i].which);
            free(parts[i].weight);
        }
        free_animation(anim);
    }
    load_images(model, data, path);
    model->part_count = part_count;
    for (i = 0; i < 3; i++) {
        model->min[i] = INFINITY;
        model->max[i] = -INFINITY;
    }
    for (i = 0; i < part_count; i++) {
        const cgltf_material *material = parts[i].material;
        MeshPart *part = &model->parts[i];
        int v, k;
        part->vertices = parts[i].vertices;
        part->count = parts[i].count;
        part->color[0] = part->color[1] = part->color[2] = part->color[3] = 1.0f;
        part->image = -1;
        if (material && material->has_pbr_metallic_roughness) {
            const cgltf_texture *texture = material->pbr_metallic_roughness.base_color_texture.texture;
            memcpy(part->color, material->pbr_metallic_roughness.base_color_factor, sizeof(part->color));
            if (texture && texture->image) {
                int image = (int)(texture->image - data->images);
                if (image >= 0 && image < model->image_count && model->images[image].rgba) part->image = image;
            }
        }
        for (v = 0; v < part->count; v++) {
            const float *at = part->vertices + (size_t)v * MESH_VERTEX_FLOATS;
            for (k = 0; k < 3; k++) {
                if (at[k] < model->min[k]) model->min[k] = at[k];
                if (at[k] > model->max[k]) model->max[k] = at[k];
            }
        }
    }
    free(parts);
    cgltf_free(data);
    return model;
}

void MeshModel_Free(MeshModel *model)
{
    int i;
    if (!model) return;
    for (i = 0; i < model->part_count; i++) free(model->parts[i].vertices);
    for (i = 0; i < model->image_count; i++) stbi_image_free(model->images[i].rgba);
    free_animation(model->animation);
    free(model->parts);
    free(model->images);
    free(model);
}
