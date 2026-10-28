#define _POSIX_C_SOURCE 200809L
#include "pc/platform/controls_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pc/compat/posix.h"
#include "scratch.h"
static void wait_ms(long ms)
{
    struct timespec t = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&t, NULL);
}
int main(void)
{
    char dir[SCRATCH_MAX], path[SCRATCH_MAX + 64];
    assert(scratch_dir(dir, sizeof(dir), "memories-stuck"));
    snprintf(path, sizeof(path), "%s/controls.txt", dir);
    setenv("MEMORIES_CONTROLS", path, 1);
    ControllerDevice *d = ControlsRuntime_Device(0);
    d->connected = 1;
    strcpy(d->identity, "test:stuck");
    d->threshold = 1.0f / 3;
    /* The GameSir G3s on macOS: both stick axes read full left/up at rest. */
    d->snapshot.axis[CTRL_AXIS_LEFT_X] = -1.0f;
    d->snapshot.axis[CTRL_AXIS_LEFT_Y] = -1.0f;
    ControlsRuntime_Update();
    assert(ControlsRuntime_Blocked());
    ControlsRuntime_Key(CTRL_KEY_X, 1);
    ControlsRuntime_Update();
    assert(!ControlsRuntime_Keyboard()); /* still inside the grace time */
    ControlsRuntime_Key(CTRL_KEY_X, 0);
    ControlsRuntime_Update();
    wait_ms(600);
    ControlsRuntime_Update();
    assert(!ControlsRuntime_Blocked());
    assert(!ControlsRuntime_Pad(0)); /* the stuck stick reads as rest */
    ControlsRuntime_Key(CTRL_KEY_X, 1);
    ControlsRuntime_Update();
    assert(ControlsRuntime_Keyboard() == CTRL_DEST_CROSS);
    ControlsRuntime_Key(CTRL_KEY_X, 0);
    ControlsRuntime_Update();
    /* Back at rest once, the axis counts again. */
    d->snapshot.axis[CTRL_AXIS_LEFT_X] = 0;
    d->snapshot.axis[CTRL_AXIS_LEFT_Y] = 0;
    ControlsRuntime_Update();
    d->snapshot.axis[CTRL_AXIS_LEFT_X] = -1.0f;
    ControlsRuntime_Update();
    assert(ControlsRuntime_Pad(0) == CTRL_DEST_LEFT);
    d->snapshot.axis[CTRL_AXIS_LEFT_X] = 0;
    ControlsRuntime_Update();
    assert(!ControlsRuntime_Blocked());
    /* A held button still holds the gate, however long. */
    ControlsRuntime_Gate();
    d->snapshot.buttons_down = 1u << (CTRL_BTN_SOUTH - 1);
    ControlsRuntime_Update();
    wait_ms(600);
    ControlsRuntime_Update();
    assert(ControlsRuntime_Blocked());
    d->snapshot.buttons_down = 0;
    ControlsRuntime_Update();
    assert(!ControlsRuntime_Blocked());
    /* A stick that moves while held is a press, not a stuck reading. */
    ControlsRuntime_Gate();
    d->snapshot.axis[CTRL_AXIS_LEFT_X] = -0.6f;
    ControlsRuntime_Update();
    d->snapshot.axis[CTRL_AXIS_LEFT_X] = -0.9f;
    wait_ms(600);
    ControlsRuntime_Update();
    assert(ControlsRuntime_Blocked());
    d->snapshot.axis[CTRL_AXIS_LEFT_X] = 0;
    ControlsRuntime_Update();
    assert(!ControlsRuntime_Blocked());
    /* A trigger held at full from the start (a descriptor read the wrong way). */
    ControlsRuntime_Gate();
    d->snapshot.trigger[CTRL_TRIGGER_LEFT] = 1.0f;
    ControlsRuntime_Update();
    wait_ms(600);
    ControlsRuntime_Update();
    assert(!ControlsRuntime_Blocked());
    assert(!ControlsRuntime_Pad(0));
    puts("controls stuck: ok");
    return 0;
}
