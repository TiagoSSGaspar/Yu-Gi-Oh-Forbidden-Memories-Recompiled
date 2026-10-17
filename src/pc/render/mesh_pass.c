/* glTF models drawn by the OpenGL picture: see mesh_pass.h.
 *
 * The models (mesh_model.c) are read on the game's side, when a mod loads
 * one, and made GL buffers and textures the first time the replay draws
 * them. A submission keeps what drawing needs and the GTE's projection as
 * it was, in a ring the marker's handle indexes: frames recorded ahead of a
 * replay (a raised game speed) each keep their own. A handle the ring has
 * gone round since is not drawn. A submission also keeps its pose, a clip
 * and how far in; an animated model's buffers are written again whenever
 * the submission drawn has another pose than the last (mesh_model.c poses
 * it on the CPU).
 *
 * Drawing reproduces RTPS: the model's vertex v, turned by `yaw`, scaled by
 * the fit and put at the feet, is c = R v + T through the submitted matrix,
 * and lands on the screen at the GTE's offset plus H c / c.z, plus the GPU's
 * drawing offset, times the picture's scale. That is emitted with w = c.z,
 * so GL interpolates in perspective and the depth is the model's own: a
 * depth buffer, cleared under the drawing area, is attached to the picture
 * for the draw. The rest of the scene keeps the ordering table's order. */
#include "pc/compat/fs.h"
#include "mesh_pass.h"
#include "mesh_model.h"
#include "gl_picture.h"
#include "pc/compat/gte.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODELS 32
#define RING 512 /* submissions: ten monsters a frame, fifty frames ahead */

typedef struct Model {
    char path[512];
    MeshModel *mesh;     /* NULL: could not be read */
    int fitted;
    float scale;         /* the model's units to its polygons' */
    float anchor[3];     /* the feet, in the polygons' units */
    float feet[3];       /* the middle of the model's lowest face, its own units */
    float turn[2];       /* cos, sin of its yaw */
    /* GL: made at the first draw */
    GLuint *buffers;
    GLuint *textures;
    int uploaded;
    /* An animated model's buffers hold one pose at a time: which (clip -1
     * the rest pose), and the posed vertices it was written from. */
    int shown_clip, shown_ms;
    float **posed;
} Model;

typedef struct Submission {
    uint32_t handle; /* 0: none */
    int model;
    int clip, ms; /* the pose: clip -1 the rest pose */
    int strike_ms; /* its effects' time, MESH_PASS_NO_STRIKE for none */
    int aimed;
    float target[3]; /* where they aim, in the camera's space */
    float rotation[9]; /* column-major, for the shader */
    float translation[3];
    float h, ofx, ofy;
    float opacity;
} Submission;

static Model models[MODELS];
static int model_count;
static Submission ring[RING];
static uint32_t handles;

/* --- the game's side ----------------------------------------------------- */
int MeshPass_Load(const char *path)
{
    char copy[sizeof(models[0].path)], error[256];
    size_t length;
    int i;
    if (!path) return 0;
    /* A mod's buffer: read into one of ours before the loader sees it. */
    for (length = 0; length + 1 < sizeof(copy) && path[length]; length++) copy[length] = path[length];
    copy[length] = 0;
    for (i = 0; i < model_count; i++) {
        if (!strcmp(models[i].path, copy)) return models[i].mesh ? i + 1 : 0;
    }
    if (model_count == MODELS) {
        fprintf(stderr, "memories-pc: mesh pass: more than %d models, %s left out\n", MODELS, copy);
        return 0;
    }
    memset(&models[model_count], 0, sizeof(models[model_count]));
    strcpy(models[model_count].path, copy);
    models[model_count].mesh = MeshModel_Load(copy, error, sizeof(error));
    if (!models[model_count].mesh) {
        fprintf(stderr, "memories-pc: mesh pass: %s\n", error);
        model_count++;
        return 0;
    }
    fprintf(stderr, "memories-pc: mesh pass: %s, %d parts\n", copy, models[model_count].mesh->part_count);
    return ++model_count;
}

static Model *model_at(int id)
{
    return id > 0 && id <= model_count && models[id - 1].mesh ? &models[id - 1] : NULL;
}

static float control_fixed(unsigned index) { return (float)(int32_t)Memories_GteReadControl(index) / 65536.0f; }
static float control_h(void) { return (float)(int16_t)Memories_GteReadControl(26); }

/* How many lines the model covers at `ls` and `scale`, placed as drawn. */
static float projected_height(const Model *model, const MeshPassMatrix *ls, float scale)
{
    float h = control_h(), top = INFINITY, bottom = -INFINITY;
    int i, v, r;
    for (i = 0; i < model->mesh->part_count; i++) {
        const MeshPart *part = &model->mesh->parts[i];
        for (v = 0; v < part->count; v++) {
            const float *at = part->vertices + (size_t)v * MESH_VERTEX_FLOATS;
            float d[3] = {at[0] - model->feet[0], at[1] - model->feet[1], at[2] - model->feet[2]};
            float p[3], c[3], y;
            p[0] = model->anchor[0] + scale * (model->turn[0] * d[0] + model->turn[1] * d[2]);
            p[1] = model->anchor[1] + scale * d[1];
            p[2] = model->anchor[2] + scale * (-model->turn[1] * d[0] + model->turn[0] * d[2]);
            for (r = 0; r < 3; r++) {
                c[r] = (ls->m[r][0] * p[0] + ls->m[r][1] * p[1] + ls->m[r][2] * p[2]) / 4096.0f + (float)ls->t[r];
            }
            if (c[2] <= 0.0f) return 0.0f; /* behind the eye: no answer */
            y = h * c[1] / c[2];
            if (y < top) top = y;
            if (y > bottom) bottom = y;
        }
    }
    return bottom > top ? bottom - top : 0.0f;
}

int MeshPass_Fit(int id, const MeshPassMatrix *ls, int height_px, int feet_x, int feet_y, int feet_z, int yaw,
                 int scale_percent)
{
    Model *model = model_at(id);
    MeshPassMatrix copy;
    float scale = 1.0f, angle;
    int i;
    if (!model || !ls || height_px <= 0) return 0;
    memcpy(&copy, ls, sizeof(copy));
    model->feet[0] = (model->mesh->min[0] + model->mesh->max[0]) / 2.0f;
    model->feet[1] = model->mesh->max[1]; /* y grows downwards */
    model->feet[2] = (model->mesh->min[2] + model->mesh->max[2]) / 2.0f;
    model->anchor[0] = (float)feet_x;
    model->anchor[1] = (float)feet_y;
    model->anchor[2] = (float)feet_z;
    angle = (float)yaw * 6.28318530718f / 4096.0f;
    model->turn[0] = cosf(angle);
    model->turn[1] = sinf(angle);
    /* Near enough linear: a few rounds of scaling by the ratio settle it. */
    for (i = 0; i < 6; i++) {
        float height = projected_height(model, &copy, scale);
        if (height <= 0.0f) return 0;
        scale *= (float)height_px / height;
    }
    model->scale = scale * (float)(scale_percent > 0 ? scale_percent : 100) / 100.0f;
    model->fitted = 1;
    return 1;
}

static int failed; /* the GL side could not be made: models are never drawn */

int MeshPass_Ready(void)
{
    return !failed && GlPicture_Scale() >= 2;
}

uint32_t MeshPass_Submit(int id, const MeshPassMatrix *ls, int fade)
{
    return MeshPass_SubmitPosed(id, ls, fade, -1, 0);
}

uint32_t MeshPass_SubmitPosed(int id, const MeshPassMatrix *ls, int fade, int clip, int ms)
{
    MeshPassPose pose;
    memset(&pose, 0, sizeof(pose));
    pose.clip = clip;
    pose.ms = ms;
    pose.strike_ms = MESH_PASS_NO_STRIKE;
    return MeshPass_SubmitPose(id, ls, fade, &pose);
}

uint32_t MeshPass_SubmitPose(int id, const MeshPassMatrix *ls, int fade, const MeshPassPose *pose)
{
    Model *model = model_at(id);
    Submission *at;
    uint32_t handle;
    int r, c;
    if (!model || !model->fitted || !ls || !pose || !MeshPass_Ready()) return 0;
    if (++handles > 0xffffu) handles = 1; /* 0 is no handle */
    handle = handles;
    at = &ring[handle % RING];
    at->handle = handle;
    at->model = id;
    at->clip = MeshModel_ClipLength(model->mesh, pose->clip) > 0 ? pose->clip : -1;
    at->ms = at->clip >= 0 ? pose->ms : 0;
    at->strike_ms = at->clip >= 0 ? pose->strike_ms : MESH_PASS_NO_STRIKE;
    at->aimed = pose->aimed;
    for (r = 0; r < 3; r++) at->target[r] = (float)pose->target[r];
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) at->rotation[c * 3 + r] = (float)ls->m[r][c] / 4096.0f;
        at->translation[r] = (float)ls->t[r];
    }
    at->h = control_h();
    at->ofx = control_fixed(24);
    at->ofy = control_fixed(25);
    at->opacity = 1.0f - (float)(fade < 0 ? 0 : fade > 255 ? 255 : fade) / 255.0f;
    return (MESH_PASS_MAGIC << 16) | handle;
}

int MeshPass_Clip(int id, const char *name)
{
    Model *model = model_at(id);
    char copy[64];
    size_t length;
    if (!model || !name) return -1;
    /* A mod's buffer, as MeshPass_Load's path. */
    for (length = 0; length + 1 < sizeof(copy) && name[length]; length++) copy[length] = name[length];
    copy[length] = 0;
    return MeshModel_Clip(model->mesh, copy);
}

int MeshPass_ClipEffects(int id, int clip)
{
    Model *model = model_at(id);
    return model ? MeshModel_ClipEffects(model->mesh, clip, NULL) : 0;
}

int MeshPass_ClipLength(int id, int clip)
{
    Model *model = model_at(id);
    return model ? MeshModel_ClipLength(model->mesh, clip) : 0;
}

int MeshPass_ClipStrike(int id, int clip)
{
    Model *model = model_at(id);
    return model ? MeshModel_ClipStrike(model->mesh, clip) : 0;
}

int MeshPass_IsMarker(uint32_t word)
{
    return (word >> 16) == MESH_PASS_MAGIC && (word & 0xffffu) != 0;
}

/* --- the replay's side: GL ----------------------------------------------- */
#define GL_FUNCTIONS(X) \
    X(PFNGLCREATESHADERPROC, CreateShader) \
    X(PFNGLSHADERSOURCEPROC, ShaderSource) \
    X(PFNGLCOMPILESHADERPROC, CompileShader) \
    X(PFNGLGETSHADERIVPROC, GetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, CreateProgram) \
    X(PFNGLATTACHSHADERPROC, AttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, BindAttribLocation) \
    X(PFNGLLINKPROGRAMPROC, LinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, GetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) \
    X(PFNGLDELETESHADERPROC, DeleteShader) \
    X(PFNGLUSEPROGRAMPROC, UseProgram) \
    X(PFNGLDELETEPROGRAMPROC, DeleteProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) \
    X(PFNGLUNIFORM1IPROC, Uniform1i) \
    X(PFNGLUNIFORM1FPROC, Uniform1f) \
    X(PFNGLUNIFORM2FPROC, Uniform2f) \
    X(PFNGLUNIFORM3FPROC, Uniform3f) \
    X(PFNGLUNIFORM4FPROC, Uniform4f) \
    X(PFNGLUNIFORMMATRIX3FVPROC, UniformMatrix3fv) \
    X(PFNGLGENBUFFERSPROC, GenBuffers) \
    X(PFNGLDELETEBUFFERSPROC, DeleteBuffers) \
    X(PFNGLBINDBUFFERPROC, BindBuffer) \
    X(PFNGLBUFFERDATAPROC, BufferData) \
    X(PFNGLBUFFERSUBDATAPROC, BufferSubData) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray) \
    X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, DisableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer) \
    X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, FramebufferRenderbuffer) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, CheckFramebufferStatus) \
    X(PFNGLGENRENDERBUFFERSPROC, GenRenderbuffers) \
    X(PFNGLBINDRENDERBUFFERPROC, BindRenderbuffer) \
    X(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, RenderbufferStorageMultisample) \
    X(PFNGLDELETERENDERBUFFERSPROC, DeleteRenderbuffers) \
    X(PFNGLACTIVETEXTUREPROC, ActiveTexture) \
    X(PFNGLGENERATEMIPMAPPROC, GenerateMipmap) \
    X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray) \
    X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays)

#define DECLARE(type, name) static type gl_##name;
GL_FUNCTIONS(DECLARE)
#undef DECLARE

static GLuint program, vertex_array, depth, effect_buffer;
static int depth_width, depth_height, depth_samples;
static GLint u_rotation, u_translation, u_place, u_feet, u_turn, u_screen, u_h, u_picture, u_scale, u_image,
    u_textured, u_color, u_opacity, u_effect;

static const char *vertex_source =
#ifdef __APPLE__
    "#version 150\n"
#else
    "#version 130\n"
#endif
    "in vec3 position;\n"
    "in vec3 normal;\n"
    "in vec2 texcoord;\n"
    "uniform mat3 rotation;\n"
    "uniform vec3 translation;\n"
    "uniform vec4 place;\n"   /* the feet in the polygons' units, then the scale */
    "uniform vec3 feet;\n"    /* the model's own */
    "uniform vec2 turn;\n"
    "uniform vec2 screen;\n"  /* the GTE's offset plus the GPU's */
    "uniform float h;\n"
    "uniform vec2 picture;\n"
    "uniform float scale;\n"
    "out vec2 uv;\n"
    "out float light;\n"
    "void main() {\n"
    "    vec3 d = position - feet;\n"
    "    vec3 p = place.xyz + place.w * vec3(turn.x * d.x + turn.y * d.z, d.y, -turn.y * d.x + turn.x * d.z);\n"
    "    vec3 c = rotation * p + translation;\n"
    /* the screen position times c.z, so that w = c.z divides it back */
    "    vec2 s = screen * c.z + h * c.xy;\n"
    "    const float near = 1.0, far = 65536.0;\n"
    "    gl_Position = vec4(s * scale * 2.0 / picture - c.z,\n"
    "                       c.z * (far + near) / (far - near) - 2.0 * far * near / (far - near), c.z);\n"
    "    vec3 n = rotation * vec3(turn.x * normal.x + turn.y * normal.z, normal.y,\n"
    "                             -turn.y * normal.x + turn.x * normal.z);\n"
    /* one light, from above and in front, whichever side a face shows */
    "    light = abs(dot(normalize(n), normalize(vec3(-0.35, -0.75, -0.55))));\n"
    "    uv = texcoord;\n"
    "}\n";

static const char *fragment_source =
#ifdef __APPLE__
    "#version 150\n"
#else
    "#version 130\n"
#endif
    "in vec2 uv;\n"
    "in float light;\n"
    "uniform sampler2D image;\n"
    "uniform int textured;\n"
    "uniform vec4 color;\n"
    "uniform float opacity;\n"
    "uniform int effect;\n"
    "out vec4 fragment;\n"
    "void main() {\n"
    "    vec4 texel = textured != 0 ? texture(image, uv) : vec4(1.0);\n"
    /* an effect adds its light to what is there */
    "    if (effect != 0) {\n"
    "        vec4 glow = texel * color;\n"
    "        fragment = vec4(glow.rgb * glow.a * opacity, 1.0);\n"
    "        return;\n"
    "    }\n"
    /* glTF's colour factors are linear and its textures sRGB: lit in
     * linear, then back to sRGB as the picture is */
    "    vec3 linear = pow(texel.rgb, vec3(2.2)) * color.rgb * (0.35 + 0.85 * light);\n"
    "    vec3 rgb = pow(min(linear, vec3(1.0)), vec3(1.0 / 2.2));\n"
    /* as the picture blends: colour pre-multiplied, alpha what is left of the destination */
    "    fragment = vec4(rgb * opacity, 1.0 - opacity);\n"
    "}\n";

static GLuint compile(GLenum kind, const char *source)
{
    GLuint shader = gl_CreateShader(kind);
    GLint ok = 0;
    gl_ShaderSource(shader, 1, &source, NULL);
    gl_CompileShader(shader);
    gl_GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        gl_GetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "memories-pc: mesh pass: %s shader: %s\n", kind == GL_VERTEX_SHADER ? "vertex" : "fragment",
                log);
        gl_DeleteShader(shader);
        return 0;
    }
    return shader;
}

/* The functions, the program and the vertex array, once a context. */
static int start(void)
{
    GLuint vs, fs;
    GLint ok = 0;
    if (program) return 1;
    if (failed) return 0;
#define LOAD(type, name) \
    if (!(gl_##name = (type)SDL_GL_GetProcAddress("gl" #name))) { \
        fprintf(stderr, "memories-pc: mesh pass: no gl" #name "\n"); \
        failed = 1; \
        return 0; \
    }
    GL_FUNCTIONS(LOAD)
#undef LOAD
    vs = compile(GL_VERTEX_SHADER, vertex_source);
    fs = vs ? compile(GL_FRAGMENT_SHADER, fragment_source) : 0;
    if (!fs) {
        if (vs) gl_DeleteShader(vs);
        failed = 1;
        return 0;
    }
    program = gl_CreateProgram();
    gl_AttachShader(program, vs);
    gl_AttachShader(program, fs);
    gl_BindAttribLocation(program, 0, "position");
    gl_BindAttribLocation(program, 1, "normal");
    gl_BindAttribLocation(program, 2, "texcoord");
    gl_LinkProgram(program);
    gl_DeleteShader(vs);
    gl_DeleteShader(fs);
    gl_GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        gl_GetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "memories-pc: mesh pass: program: %s\n", log);
        gl_DeleteProgram(program);
        program = 0;
        failed = 1;
        return 0;
    }
    u_rotation = gl_GetUniformLocation(program, "rotation");
    u_translation = gl_GetUniformLocation(program, "translation");
    u_place = gl_GetUniformLocation(program, "place");
    u_feet = gl_GetUniformLocation(program, "feet");
    u_turn = gl_GetUniformLocation(program, "turn");
    u_screen = gl_GetUniformLocation(program, "screen");
    u_h = gl_GetUniformLocation(program, "h");
    u_picture = gl_GetUniformLocation(program, "picture");
    u_scale = gl_GetUniformLocation(program, "scale");
    u_image = gl_GetUniformLocation(program, "image");
    u_textured = gl_GetUniformLocation(program, "textured");
    u_color = gl_GetUniformLocation(program, "color");
    u_opacity = gl_GetUniformLocation(program, "opacity");
    u_effect = gl_GetUniformLocation(program, "effect");
    gl_GenVertexArrays(1, &vertex_array);
    gl_GenBuffers(1, &effect_buffer);
    return 1;
}

static void upload(Model *model)
{
    const MeshModel *mesh = model->mesh;
    int i;
    model->buffers = calloc((size_t)mesh->part_count, sizeof(GLuint));
    model->textures = calloc((size_t)(mesh->image_count ? mesh->image_count : 1), sizeof(GLuint));
    if (!model->buffers || !model->textures) {
        free(model->buffers);
        free(model->textures);
        model->buffers = model->textures = NULL;
        return;
    }
    gl_GenBuffers(mesh->part_count, model->buffers);
    for (i = 0; i < mesh->part_count; i++) {
        gl_BindBuffer(GL_ARRAY_BUFFER, model->buffers[i]);
        gl_BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)mesh->parts[i].count * MESH_VERTEX_FLOATS * sizeof(float)),
                      mesh->parts[i].vertices, mesh->animation ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
    gl_BindBuffer(GL_ARRAY_BUFFER, 0);
    model->shown_clip = -1;
    model->shown_ms = 0;
    gl_ActiveTexture(GL_TEXTURE0);
    for (i = 0; i < mesh->image_count; i++) {
        if (!mesh->images[i].rgba) continue;
        glGenTextures(1, &model->textures[i]);
        glBindTexture(GL_TEXTURE_2D, model->textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mesh->images[i].width, mesh->images[i].height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, mesh->images[i].rgba);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        gl_GenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    model->uploaded = 1;
}

/* Puts a submission's pose in the model's buffers when they hold another:
 * its clip as far in as it was, or the rest pose. */
static void show(Model *model, int clip, int ms)
{
    const MeshModel *mesh = model->mesh;
    int i, posed = 0;
    if (clip == model->shown_clip && (clip < 0 || ms == model->shown_ms)) return;
    if (clip >= 0 && !model->posed) {
        int whole = (model->posed = calloc((size_t)mesh->part_count, sizeof(*model->posed))) != NULL;
        for (i = 0; whole && i < mesh->part_count; i++) {
            model->posed[i] = malloc((size_t)mesh->parts[i].count * MESH_VERTEX_FLOATS * sizeof(float));
            whole = model->posed[i] != NULL;
        }
        if (!whole && model->posed) {
            for (i = 0; i < mesh->part_count; i++) free(model->posed[i]);
            free(model->posed);
            model->posed = NULL; /* no room: it stands */
        }
    }
    if (clip >= 0 && model->posed) posed = MeshModel_Pose(mesh, clip, ms, model->posed);
    for (i = 0; i < mesh->part_count; i++) {
        gl_BindBuffer(GL_ARRAY_BUFFER, model->buffers[i]);
        gl_BufferSubData(GL_ARRAY_BUFFER, 0,
                         (GLsizeiptr)((size_t)mesh->parts[i].count * MESH_VERTEX_FLOATS * sizeof(float)),
                         posed ? model->posed[i] : mesh->parts[i].vertices);
    }
    gl_BindBuffer(GL_ARRAY_BUFFER, 0);
    model->shown_clip = posed ? clip : -1;
    model->shown_ms = posed ? ms : 0;
}

/* --- effects ---------------------------------------------------------------- */

/* The vertex shader's placing of a point of the model's own in the camera's
 * space, and back: an effect is shaped where the camera sees it and handed
 * to the shader as the model's own points. */
static void to_camera(const Model *model, const Submission *at, const float *v, float *c)
{
    float d[3] = {v[0] - model->feet[0], v[1] - model->feet[1], v[2] - model->feet[2]}, p[3];
    int r;
    p[0] = model->anchor[0] + model->scale * (model->turn[0] * d[0] + model->turn[1] * d[2]);
    p[1] = model->anchor[1] + model->scale * d[1];
    p[2] = model->anchor[2] + model->scale * (-model->turn[1] * d[0] + model->turn[0] * d[2]);
    for (r = 0; r < 3; r++) {
        c[r] = at->rotation[r] * p[0] + at->rotation[3 + r] * p[1] + at->rotation[6 + r] * p[2] + at->translation[r];
    }
}

static int to_model(const Model *model, const Submission *at, const float *c, float *v)
{
    const float *m = at->rotation; /* column-major: m[column * 3 + row] */
    float m00 = m[0], m10 = m[1], m20 = m[2], m01 = m[3], m11 = m[4], m21 = m[5], m02 = m[6], m12 = m[7], m22 = m[8];
    float q[3] = {c[0] - at->translation[0], c[1] - at->translation[1], c[2] - at->translation[2]}, p[3], e[3];
    float det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);
    int r;
    if (fabsf(det) < 1e-12f || model->scale == 0.0f) return 0;
    p[0] = ((m11 * m22 - m12 * m21) * q[0] - (m01 * m22 - m02 * m21) * q[1] + (m01 * m12 - m02 * m11) * q[2]) / det;
    p[1] = (-(m10 * m22 - m12 * m20) * q[0] + (m00 * m22 - m02 * m20) * q[1] - (m00 * m12 - m02 * m10) * q[2]) / det;
    p[2] = ((m10 * m21 - m11 * m20) * q[0] - (m00 * m21 - m01 * m20) * q[1] + (m00 * m11 - m01 * m10) * q[2]) / det;
    for (r = 0; r < 3; r++) e[r] = (p[r] - model->anchor[r]) / model->scale;
    v[0] = model->turn[0] * e[0] - model->turn[1] * e[2] + model->feet[0];
    v[1] = e[1] + model->feet[1];
    v[2] = model->turn[1] * e[0] + model->turn[0] * e[2] + model->feet[2];
    return 1;
}

/* Two triangles from four corners in the camera's space (round the quad),
 * textured from (u0, v0) at the first to (u1, v1) at the third. */
static int quad(const Model *model, const Submission *at, float corner[4][3], float u0, float v0, float u1, float v1,
                float *out)
{
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    int i;
    for (i = 0; i < 6; i++) {
        float *o = out + i * MESH_VERTEX_FLOATS;
        if (!to_model(model, at, corner[order[i]], o)) return 0;
        o[3] = 0.0f;
        o[4] = -1.0f;
        o[5] = 0.0f;
        o[6] = uv[order[i]][0];
        o[7] = uv[order[i]][1];
    }
    return 1;
}

/* A square facing the camera, `half` across from its middle. */
static void facing(float corner[4][3], const float *middle, float half)
{
    int i;
    for (i = 0; i < 4; i++) {
        corner[i][0] = middle[0] + (i == 1 || i == 2 ? half : -half);
        corner[i][1] = middle[1] + (i >= 2 ? half : -half);
        corner[i][2] = middle[2];
    }
}

/* The clip's effects that show at the submission's time, drawn after the
 * model with its depth to hide behind but none of their own. */
static void draw_effects(Model *model, const Submission *at)
{
    const MeshEffect *effects;
    float camera_unit, out[6 * MESH_VERTEX_FLOATS];
    int count = at->clip >= 0 && at->strike_ms != MESH_PASS_NO_STRIKE
                    ? MeshModel_ClipEffects(model->mesh, at->clip, &effects)
                    : 0;
    int i;
    if (!count) return;
    /* the camera's units to one of the model's own */
    camera_unit = model->scale * sqrtf(at->rotation[0] * at->rotation[0] + at->rotation[1] * at->rotation[1] +
                                       at->rotation[2] * at->rotation[2]);
    glDepthMask(GL_FALSE);
    gl_Uniform1i(u_effect, 1);
    gl_BindBuffer(GL_ARRAY_BUFFER, effect_buffer);
    gl_VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, MESH_VERTEX_FLOATS * sizeof(float), (const void *)0);
    gl_VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, MESH_VERTEX_FLOATS * sizeof(float), (const void *)(3 * sizeof(float)));
    gl_VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, MESH_VERTEX_FLOATS * sizeof(float), (const void *)(6 * sizeof(float)));
    for (i = 0; i < count; i++) {
        const MeshEffect *effect = &effects[i];
        float u, strength, from[3], to[3], corner[4][3], point[3], size;
        GLuint texture = 0;
        int made = 0;
        if (at->strike_ms < effect->from || at->strike_ms >= effect->to) continue;
        u = (float)(at->strike_ms - effect->from) / (float)(effect->to - effect->from);
        /* in quickly, out more slowly */
        strength = (u < 0.15f ? u / 0.15f : 1.0f) * (u > 0.75f ? (1.0f - u) / 0.25f : 1.0f);
        size = effect->size * camera_unit;
        if (effect->node >= 0) {
            if (!MeshModel_NodePoint(model->mesh, at->clip, at->ms, effect->node, effect->offset, point)) continue;
            to_camera(model, at, point, from);
        }
        if (effect->kind == MESH_EFFECT_GLOW && effect->node >= 0) {
            facing(corner, from, size * (0.6f + 0.4f * u) / 2.0f);
            made = quad(model, at, corner, 0.0f, 0.0f, 1.0f, 1.0f, out);
        } else if (effect->kind == MESH_EFFECT_BURST && at->aimed) {
            int frames = effect->columns * effect->rows, frame = (int)(u * (float)frames);
            float cw = 1.0f / (float)effect->columns, ch = 1.0f / (float)effect->rows;
            if (frame >= frames) frame = frames - 1;
            facing(corner, at->target, size * (0.5f + 0.5f * u) / 2.0f);
            made = quad(model, at, corner, (float)(frame % effect->columns) * cw, (float)(frame / effect->columns) * ch,
                        (float)(frame % effect->columns + 1) * cw, (float)(frame / effect->columns + 1) * ch, out);
        } else if (effect->kind == MESH_EFFECT_BEAM && effect->node >= 0 && at->aimed) {
            /* a band from the node to the target, turned to face the eye */
            float axis[3], side[3], length, across, k;
            int r;
            for (r = 0; r < 3; r++) to[r] = at->target[r], axis[r] = to[r] - from[r];
            side[0] = axis[1] * from[2] - axis[2] * from[1];
            side[1] = axis[2] * from[0] - axis[0] * from[2];
            side[2] = axis[0] * from[1] - axis[1] * from[0];
            across = sqrtf(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
            length = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
            if (across <= 0.0f || length <= 0.0f || size <= 0.0f) continue;
            k = size * (0.5f + 0.5f * strength) / 2.0f / across;
            for (r = 0; r < 3; r++) {
                corner[0][r] = from[r] - side[r] * k;
                corner[1][r] = to[r] - side[r] * k;
                corner[2][r] = to[r] + side[r] * k;
                corner[3][r] = from[r] + side[r] * k;
            }
            /* the streaks run along it, towards the target */
            made = quad(model, at, corner, -(float)at->strike_ms / 250.0f, 0.0f,
                        length / size - (float)at->strike_ms / 250.0f, 1.0f, out);
        }
        if (!made) continue;
        if (effect->image >= 0 && effect->image < model->mesh->image_count) texture = model->textures[effect->image];
        glBindTexture(GL_TEXTURE_2D, texture);
        gl_Uniform1i(u_textured, texture != 0);
        gl_Uniform4f(u_color, effect->color[0], effect->color[1], effect->color[2], effect->color[3] * strength);
        gl_BufferData(GL_ARRAY_BUFFER, sizeof(out), out, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    gl_Uniform1i(u_effect, 0);
    glDepthMask(GL_TRUE);
}

/* A depth buffer the size of the target, with its samples. */
static int depth_for(const MeshPassTarget *target)
{
    if (depth && depth_width == target->width && depth_height == target->height && depth_samples == target->samples)
        return 1;
    if (depth) gl_DeleteRenderbuffers(1, &depth);
    gl_GenRenderbuffers(1, &depth);
    gl_BindRenderbuffer(GL_RENDERBUFFER, depth);
    while (glGetError() != GL_NO_ERROR) continue;
    gl_RenderbufferStorageMultisample(GL_RENDERBUFFER, target->samples, GL_DEPTH_COMPONENT24, target->width,
                                      target->height);
    gl_BindRenderbuffer(GL_RENDERBUFFER, 0);
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "memories-pc: mesh pass: no room for a %dx%d depth buffer\n", target->width, target->height);
        gl_DeleteRenderbuffers(1, &depth);
        depth = 0;
        return 0;
    }
    depth_width = target->width;
    depth_height = target->height;
    depth_samples = target->samples;
    return 1;
}

void MeshPass_Draw(uint32_t word, const MeshPassTarget *target)
{
    const Submission *at = &ring[(word & 0xffffu) % RING];
    Model *model;
    int i, x, y, w, h;
    if (!MeshPass_IsMarker(word) || at->handle != (word & 0xffffu) || !(model = model_at(at->model))) return;
    if (!start() || !depth_for(target)) return;
    if (!model->uploaded) upload(model);
    if (!model->buffers) return;
    show(model, at->clip, at->ms);

    gl_BindFramebuffer(GL_FRAMEBUFFER, target->fbo);
    gl_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (gl_CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "memories-pc: mesh pass: the picture takes no depth buffer; models are off\n");
        gl_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        failed = 1;
        return;
    }
    glViewport(0, 0, target->width, target->height);
    x = target->clip_x1 < 0 ? 0 : target->clip_x1;
    y = target->clip_y1 < 0 ? 0 : target->clip_y1;
    w = target->clip_x2 - x + 1;
    h = target->clip_y2 - y + 1;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x * target->scale, y * target->scale, (w > 0 ? w : 0) * target->scale, (h > 0 ? h : 0) * target->scale);
    glDepthMask(GL_TRUE);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); /* the picture's alpha is its own */

    gl_UseProgram(program);
    gl_BindVertexArray(vertex_array);
    gl_UniformMatrix3fv(u_rotation, 1, GL_FALSE, at->rotation);
    gl_Uniform3f(u_translation, at->translation[0], at->translation[1], at->translation[2]);
    gl_Uniform4f(u_place, model->anchor[0], model->anchor[1], model->anchor[2], model->scale);
    gl_Uniform3f(u_feet, model->feet[0], model->feet[1], model->feet[2]);
    gl_Uniform2f(u_turn, model->turn[0], model->turn[1]);
    gl_Uniform2f(u_screen, at->ofx + (float)target->offset_x, at->ofy + (float)target->offset_y);
    gl_Uniform1f(u_h, at->h);
    gl_Uniform2f(u_picture, (float)target->width, (float)target->height);
    gl_Uniform1f(u_scale, (float)target->scale);
    gl_Uniform1i(u_image, 0);
    gl_Uniform1f(u_opacity, at->opacity);
    gl_ActiveTexture(GL_TEXTURE0);
    gl_EnableVertexAttribArray(0);
    gl_EnableVertexAttribArray(1);
    gl_EnableVertexAttribArray(2);
    for (i = 0; i < model->mesh->part_count; i++) {
        const MeshPart *part = &model->mesh->parts[i];
        GLuint texture = part->image >= 0 ? model->textures[part->image] : 0;
        gl_BindBuffer(GL_ARRAY_BUFFER, model->buffers[i]);
        gl_VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, MESH_VERTEX_FLOATS * sizeof(float), (const void *)0);
        gl_VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, MESH_VERTEX_FLOATS * sizeof(float),
                               (const void *)(3 * sizeof(float)));
        gl_VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, MESH_VERTEX_FLOATS * sizeof(float),
                               (const void *)(6 * sizeof(float)));
        glBindTexture(GL_TEXTURE_2D, texture);
        gl_Uniform1i(u_textured, texture != 0);
        gl_Uniform4f(u_color, part->color[0], part->color[1], part->color[2], part->color[3]);
        glDrawArrays(GL_TRIANGLES, 0, part->count);
    }
    draw_effects(model, at);
    gl_DisableVertexAttribArray(0);
    gl_DisableVertexAttribArray(1);
    gl_DisableVertexAttribArray(2);
    gl_BindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    gl_BindVertexArray(0);
    gl_UseProgram(0);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    gl_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
}

static void forget(void)
{
    int i;
    for (i = 0; i < model_count; i++) {
        free(models[i].buffers);
        free(models[i].textures);
        models[i].buffers = models[i].textures = NULL;
        models[i].uploaded = 0;
    }
    program = vertex_array = depth = effect_buffer = 0;
    depth_width = depth_height = depth_samples = 0;
}

void MeshPass_GlLost(void)
{
    forget(); /* they went with the context */
}

void MeshPass_GlStop(void)
{
    int i;
    if (program) {
        for (i = 0; i < model_count; i++) {
            if (models[i].buffers) gl_DeleteBuffers(models[i].mesh->part_count, models[i].buffers);
            if (models[i].textures) glDeleteTextures(models[i].mesh->image_count, models[i].textures);
        }
        if (depth) gl_DeleteRenderbuffers(1, &depth);
        if (effect_buffer) gl_DeleteBuffers(1, &effect_buffer);
        gl_DeleteVertexArrays(1, &vertex_array);
        gl_DeleteProgram(program);
    }
    forget();
}
