#ifndef MEMORIES_PC_UI_TEXT_H
#define MEMORIES_PC_UI_TEXT_H
/* The port's own words in the player's language (notes/pc-build.md, "The
 * port's words in the player's language"): the menu bar, the Mods and
 * Controls windows, the notices and prompts the port draws. The English is
 * the key; languages/ui/<language>.json, beside the program (or in the
 * working folder), maps it to the player's language, and an English the file
 * does not have stays English. The game's own text is not here (Game >
 * Language, src/pc/text/language.h), nor what the log says. */
#include <stddef.h>

/* `english` in the player's language, or `english` itself. A format keeps
 * its conversions in the same order (tools/pc/check_ui_text.py). The string
 * returned lives until exit. */
const char *UI(const char *english);

/* The system's language as SDL names it ("pt-BR"), or NULL for none; read
 * again only when the language it gives differs from the one read. */
void UiText_SetLanguage(const char *system_tag);

/* The menus drawn in the built-in 5x7 letters, which have no accents: keep
 * them English rather than "?" for every accented letter. */
void UiText_SetPlain(int plain);

/* "pt_BR.UTF-8", "pt-br" or "pt" as "pt-BR" or "pt", a script as "zh-Hant";
 * "C" and "POSIX" as none (empty). */
void Language_Tag(char *out, size_t size, const char *text);

/* The language to show: the environment's `variable` when it is set ("en"
 * for English), else `system` (a tag), else the first of LC_ALL,
 * LC_MESSAGES and LANG that is set. Empty for none. */
void Language_Pick(char *out, size_t size, const char *variable, const char *system);

#endif
