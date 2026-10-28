/* The port's own words in the player's language (ui_text.h). */
#include "pc/compat/fs.h"
#include "ui_text.h"
#include "paths.h"
#include "pc/mods/json.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void Language_Tag(char *out, size_t size, const char *text)
{
    size_t length = 0, part = 0;
    if (!size) return;
    out[0] = '\0';
    if (!text || !strcmp(text, "C") || !strncmp(text, "C.", 2) || !strcmp(text, "POSIX")) return;
    for (; *text && *text != '.' && *text != '@' && length + 1 < size; text++) {
        if (*text == '_' || *text == '-') {
            if (!length || part) break;   /* the language and its country (or script), no more */
            part = length + 1;
            out[length++] = '-';
        } else if (isalnum((unsigned char)*text)) {
            out[length++] = (char)tolower((unsigned char)*text);
        } else break;
    }
    if (length && out[length - 1] == '-') length--;
    out[length] = '\0';
    if (part && length - part == 4) out[part] = (char)toupper((unsigned char)out[part]);   /* a script: Hant */
    else if (part) for (size_t i = part; i < length; i++) out[i] = (char)toupper((unsigned char)out[i]);
}

void Language_Pick(char *out, size_t size, const char *variable, const char *system)
{
    static const char *const locale[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    const char *named = variable ? getenv(variable) : NULL;
    if (named) { Language_Tag(out, size, named); return; }
    if (system && *system) { Language_Tag(out, size, system); return; }
    if (size) out[0] = '\0';
    for (size_t i = 0; i < sizeof(locale) / sizeof(locale[0]); i++) {
        const char *value = getenv(locale[i]);
        if (value && *value) { Language_Tag(out, size, value); return; }   /* the first set wins, C too */
    }
}

/* The catalog read, its strings found by a table of their keys' hashes.
 * Never freed: UI's strings live until exit, and an earlier language's
 * stay readable after another is read. */
static char system_language[16], read_language[16];
static int loaded, plain;
static const JsonValue **table;
static size_t table_mask;

static uint32_t hash(const char *text)
{
    uint32_t value = 2166136261u;
    for (; *text; text++) value = (value ^ (unsigned char)*text) * 16777619u;
    return value;
}

static void read_catalog(void)
{
    char tag[16], relative[64], path[1024], error[128];
    JsonDocument *document = NULL;
    const JsonValue *root, *entry;
    size_t size = 1;
    Language_Pick(tag, sizeof(tag), "MEMORIES_UI_LANGUAGE", system_language);
    memcpy(read_language, tag, sizeof(read_language));
    loaded = 1;
    table = NULL;
    table_mask = 0;
    for (int whole = 1; whole >= 0 && *tag && !document; whole--) {
        int length = (int)(whole ? strlen(tag) : strcspn(tag, "-"));
        if (!whole && tag[length] == '\0') break;
        snprintf(relative, sizeof(relative), "languages/ui/%.*s.json", length, tag);
        if (Paths_Program(path, sizeof(path), relative) || access(path, R_OK)) {
            snprintf(path, sizeof(path), "%s", relative);
            if (access(path, R_OK)) continue;
        }
        if (!(document = Json_ParseFile(path, error, sizeof(error)))) {
            fprintf(stderr, "memories-pc: %s: %s; the menus stay English\n", path, error);
            return;
        }
    }
    if (!document) return;   /* English, or a language with no words for the port */
    root = Json_Root(document);
    if (Json_TypeOf(root) != JSON_OBJECT) {
        fprintf(stderr, "memories-pc: %s is not an object; the menus stay English\n", path);
        Json_Free(document);
        return;
    }
    while (size < (size_t)Json_Count(root) * 2) size <<= 1;
    if (!(table = calloc(size, sizeof(*table)))) return;
    table_mask = size - 1;
    for (entry = Json_At(root, 0); entry; entry = Json_Next(entry)) {
        size_t slot = hash(Json_Name(entry)) & table_mask;
        if (Json_TypeOf(entry) != JSON_STRING || !*Json_String(entry, "")) continue;   /* none: the English */
        while (table[slot]) slot = (slot + 1) & table_mask;
        table[slot] = entry;
    }
}

const char *UI(const char *english)
{
    size_t slot;
    if (!english || !*english || plain) return english;
    if (!loaded) read_catalog();
    if (!table) return english;
    for (slot = hash(english) & table_mask; table[slot]; slot = (slot + 1) & table_mask)
        if (!strcmp(Json_Name(table[slot]), english)) return Json_String(table[slot], english);
    return english;
}

void UiText_SetLanguage(const char *system_tag)
{
    char now[16];
    Language_Tag(system_language, sizeof(system_language), system_tag);
    Language_Pick(now, sizeof(now), "MEMORIES_UI_LANGUAGE", system_language);
    if (!loaded || strcmp(now, read_language)) read_catalog();
}

void UiText_SetPlain(int value)
{
    plain = value;
}
