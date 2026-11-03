#ifndef MEMORIES_PC_TEXT_NUMBER_WIDTH_H
#define MEMORIES_PC_TEXT_NUMBER_WIDTH_H
/* A number a string prints in a field of four digits or more ({f8 03 ...})
 * that has more digits than the field: ATK, DEF or LP past 9999, or
 * starchips past 999999, which a mod's "limits" allow (pc/cards/tables.h).
 * func_80038148 keeps all the digits, and what the string lays out round the
 * field must not move: the card's bar puts its type and stars right after
 * ATK and DEF, and the Password screen's starchip box ends where six digits
 * end. So the digits are drawn closer together, in the field's own width: 5
 * in 4 cells of 8 step 6, 6, 6, 7, 7 (the digits are 5 pixels wide). A
 * number that fits never gets here, so nothing the disc prints changes.
 *
 * Kept by text channel (index_57, the four text boxes); no game headers. */

/* The next `digits` glyphs of `channel` are a number of that many digits in
 * a field of `field` cells `advance` pixels wide. */
void NumberWidth_Squeeze(int channel, int digits, int field, int advance);
/* A text box starts on `channel`: nothing of an earlier number is left. */
void NumberWidth_Reset(int channel);
/* How many pixels less than its advance the glyph `channel` has just drawn
 * steps: 0 unless it is one of a squeezed number's. */
int NumberWidth_Take(int channel);

/* A card's name ({f8 00 20}) longer than the letters its string's F8 07
 * lets it have (24 on the duel's bar, 16 in the card lists): the retail
 * game cut it there, which a translation's longer names suffer from far
 * more than the English (notes/translation.md). Its letters are drawn
 * closer instead, in the same `limit` cells of `cell` pixels, down to
 * NAME_MIN_STEP pixels a letter (the letters are 7 wide, outline and all);
 * a name longer even than that is cut after the letters that fit. Returns
 * how many letters to draw, the limit F8 07 should now hold; `limit` when
 * the name fits as it is. `text` is the name's encoded text: a letter a
 * byte below 0xF0, or F1-F5 and a low byte (glyphs.h). */
#define NAME_MIN_STEP 6
int NumberWidth_SqueezeName(int channel, const unsigned char *text, int limit, int cell);

#endif
