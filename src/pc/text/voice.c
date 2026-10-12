/* Dialogue voices: the text box's pages to replace.h's voice channel
 * (voice.h). Game thread only. */
#include "voice.h"
#include "text.h"
#include "pc/audio/replace.h"
#include "pc/debug/log.h"

#define VOICE_BOX 0
#define NO_PAGE (-2)   /* no page waiting for its first letter */

static int waiting = NO_PAGE;   /* the waiting page's place, or -1: not the disc's dialogue */

void TextVoice_PageAt(int box, const unsigned char *text)
{
    if (box != VOICE_BOX) return;
    AudioReplace_VoiceStop();
    waiting = Text_DialogPlace(text);
}

void TextVoice_Letter(int box, int skipping)
{
    int place = waiting;
    if (box != VOICE_BOX) return;
    if (skipping) {
        waiting = NO_PAGE;
        AudioReplace_VoiceStop();
        return;
    }
    if (place == NO_PAGE) return;
    waiting = NO_PAGE;
    if (place < 0) return;
    /* Every page once, voiced or not: the trace is how a voice pack finds
     * the places of the pages it wants (tmp listing: text_listing.py). */
    LOG(LOG_MODS, "voice: page 0x%04X shows", place);
    AudioReplace_VoicePage(place);
}

void TextVoice_Close(int box)
{
    if (box != VOICE_BOX) return;
    waiting = NO_PAGE;
    AudioReplace_VoiceStop();
}
