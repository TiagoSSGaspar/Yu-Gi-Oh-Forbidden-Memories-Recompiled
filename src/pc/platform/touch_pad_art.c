/* The on-screen controller's pictures (touch_pad_art.h). */
#include "touch_pad_art.h"
#include "touch_pad.h"
#include "pc/text/overlay_text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The look: a dark, see-through body with a thin light edge and the
 * PlayStation's own colors on its symbols; held, the body lightens and the
 * edge brightens, as if lit from under the finger. */
#define BODY 0x141418u
#define BODY_ALPHA 0.59f
#define BODY_HELD 0x5a5a64u
#define BODY_HELD_ALPHA 0.78f
#define EDGE 0xffffffu
#define EDGE_ALPHA 0.35f
#define EDGE_HELD_ALPHA 0.80f
#define INK 0xffffffu
#define INK_ALPHA 0.90f

static const uint32_t symbol_color[TOUCH_BUTTONS] = {
    [TOUCH_TRIANGLE] = 0x40c896u, [TOUCH_CIRCLE] = 0xf0505au, [TOUCH_CROSS] = 0x6e96ffu, [TOUCH_SQUARE] = 0xeb78c8u,
};
static const char *const label[TOUCH_BUTTONS] = {
    [TOUCH_L1] = "L1", [TOUCH_L2] = "L2", [TOUCH_R1] = "R1", [TOUCH_R2] = "R2",
    [TOUCH_SELECT] = "SELECT", [TOUCH_START] = "START", [TOUCH_MENU] = "MENU",
};

/* A button's picture at its size, let go (0) and held (1): straight-alpha
 * colors, drawn again only when the layout changes its size. */
typedef struct {
    int w, h;
    float *alpha;
    uint32_t *color;
} Tile;
static Tile tiles[TOUCH_BUTTONS][2];

static void free_tile(Tile *tile)
{
    free(tile->alpha);
    free(tile->color);
    memset(tile, 0, sizeof(*tile));
}

void TouchPadArt_Reset(void)
{
    for (int i = 0; i < TOUCH_BUTTONS; i++) {
        free_tile(&tiles[i][0]);
        free_tile(&tiles[i][1]);
    }
}

/* One pixel's share of a shape whose signed distance there is d (negative
 * inside): a one-pixel ramp across the edge. */
static float cover(float d)
{
    d = 0.5f - d;
    return d <= 0 ? 0 : d >= 1 ? 1 : d;
}

/* Paints `color` at `alpha` over what the tile holds at x, y. */
static void over(Tile *tile, int x, int y, uint32_t color, float alpha)
{
    float *a = &tile->alpha[y * tile->w + x], out;
    uint32_t *c = &tile->color[y * tile->w + x];
    if (alpha <= 0) return;
    out = alpha + *a * (1 - alpha);
    if (out <= 0) return;
    {
        float keep = *a * (1 - alpha) / out, put = alpha / out;
        unsigned r = (unsigned)((*c >> 16 & 255) * keep + (color >> 16 & 255) * put + 0.5f);
        unsigned g = (unsigned)((*c >> 8 & 255) * keep + (color >> 8 & 255) * put + 0.5f);
        unsigned b = (unsigned)((*c & 255) * keep + (color & 255) * put + 0.5f);
        *c = r << 16 | g << 8 | b;
    }
    *a = out;
}

/* Signed distances, in pixels, from the middle of pixel x, y. */
static float round_box(float x, float y, float cx, float cy, float hw, float hh, float r)
{
    float qx = fabsf(x - cx) - (hw - r), qy = fabsf(y - cy) - (hh - r);
    float outside = hypotf(qx > 0 ? qx : 0, qy > 0 ? qy : 0), inside = qx > qy ? qx : qy;
    return outside + (inside < 0 ? inside : 0) - r;
}
static float segment(float x, float y, float ax, float ay, float bx, float by)
{
    float px = x - ax, py = y - ay, dx = bx - ax, dy = by - ay, t = (px * dx + py * dy) / (dx * dx + dy * dy);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return hypotf(px - dx * t, py - dy * t);
}
/* A triangle's signed distance: the nearest edge, negative inside. */
static float triangle(float x, float y, const float *p)
{
    float d = segment(x, y, p[0], p[1], p[2], p[3]), e = segment(x, y, p[2], p[3], p[4], p[5]);
    float f = segment(x, y, p[4], p[5], p[0], p[1]);
    float c0 = (p[2] - p[0]) * (y - p[1]) - (p[3] - p[1]) * (x - p[0]);
    float c1 = (p[4] - p[2]) * (y - p[3]) - (p[5] - p[3]) * (x - p[2]);
    float c2 = (p[0] - p[4]) * (y - p[5]) - (p[1] - p[5]) * (x - p[4]);
    int inside = (c0 >= 0 && c1 >= 0 && c2 >= 0) || (c0 <= 0 && c1 <= 0 && c2 <= 0);
    d = d < e ? d : e;
    d = d < f ? d : f;
    return inside ? -d : d;
}

/* The body and its edge: a round button, or a box with round corners. */
static void body(Tile *tile, int round, float cx, float cy, float hw, float hh, float r, int held, float line)
{
    for (int y = 0; y < tile->h; y++)
        for (int x = 0; x < tile->w; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float d = round ? hypotf(px - cx, py - cy) - hw : round_box(px, py, cx, cy, hw, hh, r);
            over(tile, x, y, held ? BODY_HELD : BODY, cover(d) * (held ? BODY_HELD_ALPHA : BODY_ALPHA));
            over(tile, x, y, EDGE, cover(fabsf(d + line / 2) - line / 2) * (held ? EDGE_HELD_ALPHA : EDGE_ALPHA));
        }
}

/* A face button's symbol, in strokes `line` wide, `r` from the middle. */
static void symbol(Tile *tile, TouchButton button, float cx, float cy, float r, float line)
{
    const float half = line / 2;
    for (int y = 0; y < tile->h; y++)
        for (int x = 0; x < tile->w; x++) {
            float px = x + 0.5f, py = y + 0.5f, d;
            if (button == TOUCH_CIRCLE) {
                d = fabsf(hypotf(px - cx, py - cy) - r) - half;
            } else if (button == TOUCH_SQUARE) {
                d = fabsf(round_box(px, py, cx, cy, r * 0.82f, r * 0.82f, line * 0.4f)) - half;
            } else if (button == TOUCH_CROSS) {
                float k = r * 0.78f, a = segment(px, py, cx - k, cy - k, cx + k, cy + k);
                float b = segment(px, py, cx - k, cy + k, cx + k, cy - k);
                d = (a < b ? a : b) - half;
            } else {
                float p[6] = {cx, cy - r, cx + r * 0.95f, cy + r * 0.68f, cx - r * 0.95f, cy + r * 0.68f};
                float a = segment(px, py, p[0], p[1], p[2], p[3]), b = segment(px, py, p[2], p[3], p[4], p[5]);
                float c = segment(px, py, p[4], p[5], p[0], p[1]);
                d = (a < b ? (a < c ? a : c) : (b < c ? b : c)) - half;
            }
            over(tile, x, y, symbol_color[button], cover(d));
        }
}

/* A D-pad button's arrow, pointing out from the pad's middle. */
static void arrow(Tile *tile, TouchButton button, float cx, float cy, float r)
{
    float p[6];
    float ux = button == TOUCH_LEFT ? -1.f : button == TOUCH_RIGHT ? 1.f : 0.f;
    float uy = button == TOUCH_UP ? -1.f : button == TOUCH_DOWN ? 1.f : 0.f;
    p[0] = cx + ux * r;
    p[1] = cy + uy * r;
    p[2] = cx - ux * r * 0.6f + uy * r;
    p[3] = cy - uy * r * 0.6f + ux * r;
    p[4] = cx - ux * r * 0.6f - uy * r;
    p[5] = cy - uy * r * 0.6f - ux * r;
    for (int y = 0; y < tile->h; y++)
        for (int x = 0; x < tile->w; x++)
            over(tile, x, y, INK, cover(triangle(x + 0.5f, y + 0.5f, p)) * INK_ALPHA);
}

/* The pill a labelled button is: as tall as the shorter side allows, its
 * text set in the menu's face (overlay_text.h) when drawn. */
static float pill_height(TouchButton button, int w, int h)
{
    float tall = button == TOUCH_START ? h * 0.62f /* as tall as SELECT */
                 : button == TOUCH_L1 || button == TOUCH_L2 || button == TOUCH_R1 || button == TOUCH_R2 ? h * 0.62f
                                                                                                         : h;
    return tall < w ? tall : w;
}

static int make_tile(Tile *tile, TouchButton button, int w, int h, int held)
{
    float line = w > h ? h / 28.f : w / 28.f, cx = w / 2.f, cy = h / 2.f;
    if (line < 1.5f) line = 1.5f;
    free_tile(tile);
    tile->alpha = calloc((size_t)w * h, sizeof(*tile->alpha));
    tile->color = calloc((size_t)w * h, sizeof(*tile->color));
    if (!tile->alpha || !tile->color) {
        free_tile(tile);
        return 0;
    }
    tile->w = w;
    tile->h = h;
    if (button >= TOUCH_TRIANGLE && button <= TOUCH_SQUARE) {
        float r = (w < h ? w : h) / 2.f - line;
        body(tile, 1, cx, cy, r, r, 0, held, line);
        symbol(tile, button, cx, cy, r * 0.42f, r * 0.11f > 2 ? r * 0.11f : 2);
    } else if (button <= TOUCH_RIGHT) {
        float half = (w < h ? w : h) / 2.f - line * 1.5f;
        body(tile, 0, cx, cy, half, half, half * 0.32f, held, line);
        arrow(tile, button, cx, cy, half * 0.36f);
    } else {
        float ph = pill_height(button, w, h) - line * 2;
        body(tile, 0, cx, cy, w / 2.f - line, ph / 2, ph / 2, held, line);
    }
    return 1;
}

static void grow(int *x0, int *y0, int *x1, int *y1, int x, int y, int w, int h)
{
    if (x < *x0) *x0 = x;
    if (y < *y0) *y0 = y;
    if (x + w > *x1) *x1 = x + w;
    if (y + h > *y1) *y1 = y + h;
}

void TouchPadArt_Draw(MenuCanvas *canvas, int *x, int *y, int *w, int *h)
{
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30), bx, by, bw, bh;
    *x = *y = *w = *h = 0;
    if (!TouchPad_Shown()) return;
    for (int i = 0; i < TOUCH_BUTTONS; i++) {
        int held = TouchPad_Held((TouchButton)i) != 0;
        Tile *tile = &tiles[i][held];
        if (!TouchPad_Rect((TouchButton)i, &bx, &by, &bw, &bh)) return;
        if (bw <= 0 || bh <= 0) continue;
        grow(&x0, &y0, &x1, &y1, bx, by, bw, bh);
        if ((tile->w != bw || tile->h != bh) && !make_tile(tile, (TouchButton)i, bw, bh, held)) continue;
        for (int ty = 0; ty < bh; ty++) {
            if (by + ty < 0 || by + ty >= canvas->height) continue;
            for (int tx = 0; tx < bw; tx++) {
                float a = tile->alpha[ty * bw + tx];
                if (a <= 0 || bx + tx < 0 || bx + tx >= canvas->width) continue;
                OverlayText_Blend(canvas, bx + tx, by + ty, tile->color[ty * bw + tx], (unsigned)(a * 255 + 0.5f));
            }
        }
        if (label[i]) {
            int pixels = (int)(pill_height((TouchButton)i, bw, bh) * (i >= TOUCH_SELECT ? 0.46f : 0.44f));
            int text_w = OverlayText_Width(label[i], pixels);
            OverlayText_Draw(canvas, bx + (bw - text_w) / 2, by + bh / 2, bx + bw, label[i], pixels, INK);
        }
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > canvas->width) x1 = canvas->width;
    if (y1 > canvas->height) y1 = canvas->height;
    if (x1 <= x0 || y1 <= y0) return;
    *x = x0;
    *y = y0;
    *w = x1 - x0;
    *h = y1 - y0;
}
