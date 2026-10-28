#include "gamepad_mapping.h"
#include <stdio.h>
#include <string.h>

static const char *const BUTTONS[] = {"a",        "b",          "x",     "y",       "leftshoulder", "rightshoulder",
                                      "back",     "start",      "leftstick", "rightstick", "guide", "misc1",
                                      "paddle1",  "paddle2",    "paddle3", "paddle4", "touchpad"};
int GamepadMapping_Fallback(char *out, size_t size, const char *guid, const char *name, int buttons, int hats,
                            int axes)
{
    size_t n = 0;
    int ok = 1;
#define PUT(...)                                                                                             \
    do {                                                                                                     \
        int w = ok && n < size ? snprintf(out + n, size - n, __VA_ARGS__) : -1;                              \
        if (w < 0 || (size_t)w >= size - n)                                                                  \
            ok = 0;                                                                                          \
        else                                                                                                 \
            n += (size_t)w;                                                                                  \
    } while (0)
    if (size)
        out[0] = 0;
    if (buttons <= 0 && hats <= 0)
        return 0;
    PUT("%s,", guid);
    /* A comma would end the name field early. */
    for (const char *c = name && *name ? name : "Controller"; *c; c++)
        PUT("%c", *c == ',' ? ' ' : *c);
    PUT(GAMEPAD_MAPPING_GENERIC ",");
    for (int b = 0; b < buttons && b < (int)(sizeof(BUTTONS) / sizeof(BUTTONS[0])); b++)
        PUT("%s:b%d,", BUTTONS[b], b);
    if (hats > 0)
        PUT("dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,");
    if (axes >= 2)
        PUT("leftx:a0,lefty:a1,");
    if (axes >= 4)
        PUT("rightx:a2,righty:a3,");
#undef PUT
    if (!ok && size)
        out[0] = 0;
    return ok;
}
