#ifndef MEMORIES_GAMEPAD_MAPPING_H
#define MEMORIES_GAMEPAD_MAPPING_H
#include <stddef.h>
/* An SDL gamepad mapping for a joystick SDL has none for: its buttons in
 * order as the gamepad's (south, east, west, north, shoulders, back, start,
 * sticks, guide, misc, paddles), hat 0 as the d-pad and its first axes as
 * the sticks. The names may land on the wrong buttons; the Controls window
 * rebinds them. 0 (and out empty) with no button and no hat, or no room. */
/* What the mapping's name ends with: what tells a pad on it (sdl.c). */
#define GAMEPAD_MAPPING_GENERIC " (generic)"
int GamepadMapping_Fallback(char *out, size_t size, const char *guid, const char *name, int buttons, int hats,
                            int axes);
#endif
