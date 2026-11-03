/* A number wider than its field, and a card's name longer than its F8 07
 * limit, drawn in the field's width (src/pc/text/number_width.c). */
#include "pc/text/number_width.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static int steps(int channel, int digits, int advance, int *each)
{
    int i, total = 0;
    for (i = 0; i < digits; i++) {
        each[i] = advance - NumberWidth_Take(channel);
        total += each[i];
    }
    return total;
}

int main(void)
{
    int each[8];
    /* Nothing squeezed: every glyph steps its advance. */
    assert(NumberWidth_Take(0) == 0 && NumberWidth_Take(3) == 0);
    /* Five digits in four cells of 8: 6, 6, 6, 7, 7, the field's 32 in all,
     * and the glyph after them steps its own 8. */
    NumberWidth_Squeeze(1, 5, 4, 8);
    assert(steps(1, 5, 8, each) == 32);
    assert(each[0] == 6 && each[1] == 6 && each[2] == 6 && each[3] == 7 && each[4] == 7);
    assert(NumberWidth_Take(1) == 0);
    /* Eight in six cells of 16 (the Password screen's starchips). */
    NumberWidth_Squeeze(2, 8, 6, 16);
    assert(steps(2, 8, 16, each) == 96 && each[0] == 12 && each[7] == 12);
    /* Channels apart; a new box forgets what was left. */
    NumberWidth_Squeeze(0, 7, 6, 8);
    assert(NumberWidth_Take(3) == 0);
    NumberWidth_Reset(0);
    assert(NumberWidth_Take(0) == 0);
    /* A number that fits, and channels out of range, change nothing. */
    NumberWidth_Squeeze(0, 4, 4, 8);
    NumberWidth_Squeeze(9, 5, 4, 8);
    NumberWidth_Squeeze(-1, 5, 4, 8);
    assert(NumberWidth_Take(0) == 0 && NumberWidth_Take(9) == 0 && NumberWidth_Take(-1) == 0);
    {   /* Names: a letter a byte, F1-F5 and a byte one letter (accented),
         * ending at FF or any other code. */
        static const unsigned char fits[] = {0x10, 0x11, 0x12, 0xFF};
        unsigned char longer[40];
        int i, total = 0, each_step;
        assert(NumberWidth_SqueezeName(0, fits, 24, 8) == 24);
        assert(NumberWidth_Take(0) == 0);                       /* fits: nothing squeezed */
        /* 28 letters, two of them accented, in the duel bar's 24 cells of 8. */
        for (i = 0; i < 26; i++) longer[i] = 0x20;
        longer[2] = 0xF1; longer[3] = 0x00;                     /* code 0x100: a letter, not an end */
        longer[10] = 0xF2; longer[11] = 0x40;
        for (i = 26; i < 30; i++) longer[i] = 0x21;
        longer[30] = 0xFF;
        assert(NumberWidth_SqueezeName(1, longer, 24, 8) == 28);
        for (i = 0; i < 28; i++) {
            each_step = 8 - NumberWidth_Take(1);
            assert(each_step >= NAME_MIN_STEP && each_step <= 8);
            total += each_step;
        }
        assert(total == 192 && NumberWidth_Take(1) == 0);     /* the field's 192 pixels */
        /* Too long even at NAME_MIN_STEP: as many letters as fit at it. */
        for (i = 0; i < 39; i++) longer[i] = 0x20;
        longer[39] = 0xFF;
        assert(NumberWidth_SqueezeName(2, longer, 24, 8) == 32);
        for (i = 0, total = 0; i < 32; i++) total += 8 - NumberWidth_Take(2);
        assert(total == 192);
        /* A code ends the name; no limit, no text or a cell too small: as it was. */
        longer[5] = 0xF8;
        assert(NumberWidth_SqueezeName(3, longer, 4, 8) == 5);
        NumberWidth_Reset(3);
        assert(NumberWidth_SqueezeName(3, longer, 0, 8) == 0 && NumberWidth_SqueezeName(3, NULL, 4, 8) == 4);
        assert(NumberWidth_SqueezeName(3, longer, 4, NAME_MIN_STEP) == 4 && NumberWidth_Take(3) == 0);
    }
    puts("number_width: ok");
    return 0;
}
