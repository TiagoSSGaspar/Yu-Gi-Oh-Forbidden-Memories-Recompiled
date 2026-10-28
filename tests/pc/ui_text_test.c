/* The port's own words in the player's language (src/pc/platform/ui_text.h):
 * languages/ui/<language>.json beside the program, the English wherever
 * the catalog has nothing or is broken. */
#define _POSIX_C_SOURCE 200809L
#include "pc/platform/ui_text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "pc/compat/posix.h"
#include "scratch.h"

static char root[1024];

static void write_text(const char *relative, const char *text)
{
    char path[1100];
    FILE *file;
    snprintf(path, sizeof(path), "%s/%s", root, relative);
    assert((file = fopen(path, "wb")));
    fputs(text, file);
    fclose(file);
}

static void make_dir(const char *relative)
{
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s", root, relative);
    assert(!mkdir(path, 0700));
}

int main(void)
{
    char tag[16];
    const char *english = "Save state";
    assert(scratch_dir(root, sizeof(root), "memories-ui-text"));
    make_dir("languages");
    make_dir("languages/ui");
    write_text("languages/ui/pt-BR.json",
               "{\"Save state\":\"Salvar estado\",\"Slot %d\":\"Espa\xC3\xA7o %d\",\"Empty\":\"\",\"Number\":3}");
    write_text("languages/ui/pt.json", "{\"Save state\":\"Gravar estado\"}");
    write_text("languages/ui/de.json", "{\"Save state\":");
    write_text("languages/ui/fr.json", "[\"Save state\"]");
    assert(!setenv("MEMORIES_PROGRAM_DIR", root, 1));
    assert(!unsetenv("MEMORIES_UI_LANGUAGE"));
    assert(!setenv("LC_ALL", "C", 1));

    /* The tags SDL and the locale give. */
    Language_Tag(tag, sizeof(tag), "pt_BR.UTF-8");
    assert(!strcmp(tag, "pt-BR"));
    Language_Tag(tag, sizeof(tag), "zh_hant");
    assert(!strcmp(tag, "zh-Hant"));
    Language_Tag(tag, sizeof(tag), "C.UTF-8");
    assert(!strcmp(tag, ""));

    /* No language: the English itself, the same pointer. */
    assert(UI(english) == english);
    assert(UI(NULL) == NULL);

    UiText_SetLanguage("pt-BR");
    assert(!strcmp(UI("Save state"), "Salvar estado"));
    assert(!strcmp(UI("Slot %d"), "Espa\xC3\xA7o %d"));
    assert(!strcmp(UI("Not in the catalog"), "Not in the catalog"));
    assert(!strcmp(UI("Empty"), "Empty"));     /* an empty translation is none */
    assert(!strcmp(UI("Number"), "Number"));   /* so is one that is no string */
    assert(!strcmp(UI(""), ""));

    /* The language alone when the country has no file. */
    UiText_SetLanguage("pt-PT");
    assert(!strcmp(UI("Save state"), "Gravar estado"));

    /* A catalog that does not parse, or is no object: English. */
    UiText_SetLanguage("de-DE");
    assert(!strcmp(UI("Save state"), "Save state"));
    UiText_SetLanguage("fr");
    assert(!strcmp(UI("Save state"), "Save state"));

    /* The built-in letters have no accents: English while they are used. */
    UiText_SetLanguage("pt-BR");
    UiText_SetPlain(1);
    assert(!strcmp(UI("Save state"), "Save state"));
    UiText_SetPlain(0);
    assert(!strcmp(UI("Save state"), "Salvar estado"));

    /* The environment wins over the system; "en" is English. */
    assert(!setenv("MEMORIES_UI_LANGUAGE", "en", 1));
    UiText_SetLanguage("pt-BR");
    assert(!strcmp(UI("Save state"), "Save state"));
    assert(!setenv("MEMORIES_UI_LANGUAGE", "pt", 1));
    UiText_SetLanguage("en-US");
    assert(!strcmp(UI("Save state"), "Gravar estado"));
    assert(!unsetenv("MEMORIES_UI_LANGUAGE"));

    /* No system language: the locale's. */
    assert(!setenv("LC_ALL", "pt_BR.UTF-8", 1));
    UiText_SetLanguage(NULL);
    assert(!strcmp(UI("Save state"), "Salvar estado"));

    puts("ui text: ok");
    return 0;
}
