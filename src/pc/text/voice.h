#ifndef MEMORIES_PC_TEXT_VOICE_H
#define MEMORIES_PC_TEXT_VOICE_H
/* Dialogue voices: a mod's "audio" "voice" entries give a page of dialogue
 * a file, by where the page starts in the disc's dialogue bank
 * (notes/modding.md, "Voices"; replace.h plays them). The text box's own
 * code (src/game, #ifdef MEMORIES_PC) says where each page starts and when
 * it goes; the first letter the page draws starts its voice, so a page of
 * nothing but codes says nothing. Only text box 0 speaks: the one story
 * scenes and duel dialogue use. Text read from anywhere but the disc's
 * dialogue bank (a translation's, a mod's) has no voice. */

/* A text starts in `box`, or its page was confirmed away: the next page
 * starts at `text`. The voice playing stops. */
void TextVoice_PageAt(int box, const unsigned char *text);
/* `box` drew a letter: the first since TextVoice_PageAt starts the page's
 * voice. `skipping` (Square held, the game's fast-forward): the text is
 * being skipped, and so is its voice: the one playing stops and the page
 * says nothing. */
void TextVoice_Letter(int box, int skipping);
/* `box` closed: its voice stops. */
void TextVoice_Close(int box);
#endif
