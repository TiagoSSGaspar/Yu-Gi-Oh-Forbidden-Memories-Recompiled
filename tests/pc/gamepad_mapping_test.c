#include "pc/platform/gamepad_mapping.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    char out[1024];
    assert(GamepadMapping_Fallback(out, sizeof(out), "0300bdfbac0500003d03000004010000", "Pad, USB", 16, 1, 6));
    assert(!strcmp(out, "0300bdfbac0500003d03000004010000,Pad  USB (generic),"
                        "a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,"
                        "leftstick:b8,rightstick:b9,guide:b10,misc1:b11,paddle1:b12,paddle2:b13,"
                        "paddle3:b14,paddle4:b15,"
                        "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,"
                        "leftx:a0,lefty:a1,rightx:a2,righty:a3,"));
    assert(GamepadMapping_Fallback(out, sizeof(out), "03000000000000000000000000000000", "Two", 2, 0, 1));
    assert(!strcmp(out, "03000000000000000000000000000000,Two (generic),a:b0,b:b1,"));
    assert(GamepadMapping_Fallback(out, sizeof(out), "03000000000000000000000000000000", NULL, 0, 1, 0));
    assert(!strcmp(out, "03000000000000000000000000000000,Controller (generic),"
                        "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,"));
    assert(!GamepadMapping_Fallback(out, sizeof(out), "03000000000000000000000000000000", "Pedal", 0, 0, 3));
    assert(!out[0]);
    assert(!GamepadMapping_Fallback(out, 16, "0300bdfbac0500003d03000004010000", "Pad", 4, 1, 2));
    assert(!out[0]);
    puts("gamepad mapping: ok");
    return 0;
}
