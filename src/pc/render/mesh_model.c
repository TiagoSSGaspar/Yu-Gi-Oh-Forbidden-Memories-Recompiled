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

/* A vertex array that grows as triangles are added to its material. */
typedef struct Building {
    const cgltf_material *material;
    float *vertices;
    int count, room;
} Building;

static float *room_for(Building *part, int vertices)
{
    if (part->count + vertices > part->room) {
        int room = part->room ? part->room * 2 : 3072;
        float *more;
        while (room < part->count + vertices) room *= 2;
        more = realloc(part->vertices, (size_t)room * MESH_VERTEX_FLOATS * sizeof(float));
        if (!more) return NULL;
        part->vertices = more;
        part->room = room;
    }
    part->count += vertices;
    return part->vertices + (size_t)(part->count - vertices) * MESH_VERTEX_FLOATS;
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

static int add_primitive(Building **parts, int *part_count, const cgltf_node *node, const cgltf_primitive *primitive,
                         const float *node_world, const float *joint_matrices)
{
    const cgltf_accessor *position = NULL, *normal = NULL, *texcoord = NULL, *joints = NULL, *weights = NULL;
    cgltf_size i, triangles;
    Building *part;
    float *out;
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
    if (!part || !(out = room_for(part, (int)triangles * 3))) return 0;
    for (i = 0; i < triangles * 3; i++) {
        cgltf_size index = primitive->indices ? cgltf_accessor_read_index(primitive->indices, i) : i;
        float p[3] = {0, 0, 0}, n[3] = {0, 1, 0}, uv[2] = {0, 0}, m[16], placed[3], turned[3];
        float *v = out + i * MESH_VERTEX_FLOATS;
        cgltf_accessor_read_float(position, index, p, 3);
        if (normal) cgltf_accessor_read_float(normal, index, n, 3);
        if (texcoord) cgltf_accessor_read_float(texcoord, index, uv, 2);
        vertex_matrix(node, joint_matrices, joints, weights, index, node_world, m);
        transform_point(m, p, placed);
        transform_normal(m, n, turned);
        /* glTF's y up, z towards the viewer; the console's y down, z away */
        v[0] = placed[0];
        v[1] = -placed[1];
        v[2] = -placed[2];
        v[3] = turned[0];
        v[4] = -turned[1];
        v[5] = -turned[2];
        v[6] = uv[0];
        v[7] = uv[1];
    }
    if (!normal) {
        /* flat: each triangle's own facing */
        for (i = 0; i < triangles; i++) {
            float *a = out + i * 3 * MESH_VERTEX_FLOATS, *b = a + MESH_VERTEX_FLOATS, *c = b + MESH_VERTEX_FLOATS;
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

MeshModel *MeshModel_Load(const char *path, char *error, int error_size)
{
    cgltf_options options;
    cgltf_data *data = NULL;
    cgltf_result result;
    MeshModel *model;
    Building *parts = NULL;
    int part_count = 0, i, ok = 1;
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
    for (n = 0; n < data->nodes_count && ok; n++) {
        const cgltf_node *node = &data->nodes[n];
        float world[16], *joints;
        cgltf_size p;
        if (!node->mesh) continue;
        cgltf_node_transform_world(node, world);
        joints = joint_matrices_of(node->skin);
        for (p = 0; p < node->mesh->primitives_count && ok; p++) {
            ok = add_primitive(&parts, &part_count, node, &node->mesh->primitives[p], world, joints);
        }
        free(joints);
    }
    model = ok && part_count ? calloc(1, sizeof(*model)) : NULL;
    if (model) model->parts = calloc((size_t)part_count, sizeof(*model->parts));
    if (!model || !model->parts) {
        if (error) snprintf(error, (size_t)error_size, "%s: %s", path, ok ? "no triangles" : "out of memory");
        for (i = 0; i < part_count; i++) free(parts[i].vertices);
        free(parts);
        free(model);
        cgltf_free(data);
        return NULL;
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
    free(model->parts);
    free(model->images);
    free(model);
}
