/* A mod's own words in the player's language (lang/<language>.json): the
 * name, description and settings the window shows, the English wherever
 * the file has nothing or gets something wrong. */
#define main original_mods_test
#include "pc/compat/fs.h"
#include "mods_test.c"
#undef main
#include <dirent.h>

int main(void)
{
    char path[1024];
    int words, plain, broken, odd, listed;
    assert(scratch_dir(root, sizeof(root), "memories-language"));
    make_dir("mods");
    make_dir("mods/words");
    write_text("mods/words/mod.json",
               "{\"id\":\"words\",\"name\":\"Words\",\"description\":\"In English.\",\"settings\":["
               "{\"key\":\"speed\",\"label\":\"Speed\",\"description\":\"How fast.\",\"suffix\":\" frames\","
               "\"min\":1,\"max\":10,\"default\":5},"
               "{\"key\":\"mode\",\"label\":\"Mode\",\"type\":\"choice\",\"choices\":[\"Off\",\"Some\",\"All\"]},"
               "{\"key\":\"kept\",\"label\":\"Kept\",\"type\":\"choice\",\"choices\":[\"One\",\"Two\"]}]}");
    make_dir("mods/words/lang");
    write_text("mods/words/lang/pt-BR.json",
               "{\"name\":\"Palavras\",\"description\":\"Em portugu\xC3\xAAs.\",\"settings\":{"
               "\"speed\":{\"label\":\"Velocidade\",\"suffix\":\" quadros\"},"
               "\"mode\":{\"choices\":[\"Desligado\",\"Alguns\",\"Todos\"]},"
               "\"kept\":{\"label\":\"Mantido\",\"choices\":[\"Um\"]}}}");
    write_text("mods/words/lang/pt.json", "{\"name\":\"Palavras (pt)\"}");
    write_text("mods/words/lang/zh-Hant.json", "{\"name\":\"Hant\"}");
    /* Only the language, no country: pt-BR finds it. */
    make_dir("mods/plain");
    write_text("mods/plain/mod.json", "{\"id\":\"plain\",\"name\":\"Plain\",\"description\":\"English only.\"}");
    make_dir("mods/plain/lang");
    write_text("mods/plain/lang/pt.json", "{\"name\":\"\",\"description\":\"S\xC3\xB3 portugu\xC3\xAAs.\"}");
    make_dir("mods/broken");
    write_text("mods/broken/mod.json", "{\"id\":\"broken\",\"name\":\"Broken\"}");
    make_dir("mods/broken/lang");
    write_text("mods/broken/lang/pt-BR.json", "{\"name\":");
    make_dir("mods/odd");
    write_text("mods/odd/mod.json", "{\"id\":\"odd\",\"name\":\"Odd\"}");
    make_dir("mods/odd/lang");
    write_text("mods/odd/lang/pt-BR.json", "{\"nome\":\"x\",\"settings\":{\"nothing\":{\"label\":\"y\"}}}");
    /* Settings as a list, not keyed: a warning, not a crash. */
    make_dir("mods/listed");
    write_text("mods/listed/mod.json", "{\"id\":\"listed\",\"name\":\"Listed\"}");
    make_dir("mods/listed/lang");
    write_text("mods/listed/lang/pt-BR.json", "{\"name\":\"Listado\",\"settings\":[{\"label\":\"x\"}]}");

    snprintf(path, sizeof(path), "%s/mods", root);
    assert(!setenv("MEMORIES_MODS_DIR", path, 1));
    snprintf(path, sizeof(path), "%s/settings.txt", root);
    assert(!setenv("MEMORIES_SETTINGS", path, 1));
    assert(!setenv("MEMORIES_USER_DIR", root, 1));
    assert(!unsetenv("MEMORIES_MODS_LANGUAGE"));
    assert(!setenv("LC_ALL", "C", 1));
    assert(!unsetenv("LC_MESSAGES") && !unsetenv("LANG"));
    Settings_Load();
    Mods_Load();
    words = find("words");
    plain = find("plain");
    broken = find("broken");
    odd = find("odd");
    listed = find("listed");
    assert(words >= 0 && plain >= 0 && broken >= 0 && odd >= 0 && listed >= 0);

    /* No language: the manifest's words. */
    assert(!strcmp(Mods_Name(words), "Words"));
    assert(!strcmp(Mods_Metadata(words, "description"), "In English."));
    assert(!strcmp(Mods_OptionText(words, 0, "label", "?"), "Speed"));
    assert(!strcmp(Mods_OptionChoice(words, 1, 2, "?"), "All"));

    /* The system's language, as SDL names it. */
    Mods_SetLanguage("pt-BR");
    assert(!strcmp(Mods_Name(words), "Palavras"));
    assert(!strcmp(Mods_Metadata(words, "description"), "Em portugu\xC3\xAAs."));
    assert(!strcmp(Mods_Metadata(words, "version"), ""));
    assert(!strcmp(Mods_Id(words), "words"));
    assert(!strcmp(Mods_OptionText(words, 0, "label", "?"), "Velocidade"));
    assert(!strcmp(Mods_OptionText(words, 0, "suffix", ""), " quadros"));
    assert(!strcmp(Mods_OptionText(words, 0, "description", ""), "How fast."));   /* not translated */
    assert(!strcmp(Mods_OptionText(words, 1, "label", "?"), "Mode"));
    assert(!strcmp(Mods_OptionChoice(words, 1, 0, "?"), "Desligado"));
    assert(!strcmp(Mods_OptionChoice(words, 1, 2, "?"), "Todos"));
    assert(!strcmp(Mods_OptionChoice(words, 1, 3, "?"), "?"));
    /* One choice for two: the label is used, the choices stay English. */
    assert(!strcmp(Mods_OptionText(words, 2, "label", "?"), "Mantido"));
    assert(!strcmp(Mods_OptionChoice(words, 2, 1, "?"), "Two"));
    assert(strstr(Mods_Status(words), "\"kept\" has 1 choices"));
    assert(!strcmp(Mods_OptionText(words, 9, "label", "?"), "?"));
    /* The language without its country. */
    assert(!strcmp(Mods_Name(plain), "Plain"));
    assert(!strcmp(Mods_Metadata(plain, "description"), "S\xC3\xB3 portugu\xC3\xAAs."));
    /* A file that does not parse, and one with typos: warned, English. */
    assert(!strcmp(Mods_Name(broken), "Broken"));
    assert(strstr(Mods_Status(broken), "lang/pt-BR.json"));
    assert(!strcmp(Mods_Name(odd), "Odd"));
    assert(strstr(Mods_Status(odd), "unknown key \"nome\""));
    assert(strstr(Mods_Status(odd), "no setting \"nothing\""));
    assert(!strcmp(Mods_Name(listed), "Listado"));
    assert(strstr(Mods_Status(listed), "\"settings\" is not an object"));

    /* The same language again reads nothing again: no warning twice. */
    {
        size_t length = strlen(Mods_Status(odd));
        Mods_SetLanguage("pt-BR");
        assert(strlen(Mods_Status(odd)) == length);
    }
    /* The locale's spelling of it, and the environment winning. */
    Mods_SetLanguage("pt_BR.UTF-8");
    assert(!strcmp(Mods_Name(words), "Palavras"));
    Mods_SetLanguage("pt");
    assert(!strcmp(Mods_Name(words), "Palavras (pt)"));
    Mods_SetLanguage("zh_hant");   /* a script, not a country */
    assert(!strcmp(Mods_Name(words), "Hant"));
    Mods_SetLanguage("en-US");
    assert(!strcmp(Mods_Name(words), "Words"));
    assert(!setenv("MEMORIES_MODS_LANGUAGE", "pt-br", 1));
    Mods_SetLanguage("en-US");
    Mods_SetLanguage("de");
    assert(!strcmp(Mods_Name(words), "Palavras"));
    assert(!setenv("MEMORIES_MODS_LANGUAGE", "en", 1));
    Mods_SetLanguage("pt-BR");
    assert(!strcmp(Mods_Name(words), "Words"));   /* no lang/en.json: the manifest's, whatever the system says */
#ifndef _WIN32
    /* Windows has no empty variable: setting one to "" removes it. */
    assert(!setenv("MEMORIES_MODS_LANGUAGE", "", 1));
    Mods_SetLanguage("pt-BR");
    assert(!strcmp(Mods_Name(words), "Words"));   /* "" is English too */
#endif
    assert(!unsetenv("MEMORIES_MODS_LANGUAGE"));
    /* With no system language, the locale's environment. */
    assert(!setenv("LC_ALL", "pt_BR.UTF-8", 1));
    Mods_SetLanguage(NULL);
    assert(!strcmp(Mods_Name(words), "Palavras"));
    /* The first variable set wins, C as well. */
    assert(!setenv("LC_ALL", "C", 1) && !setenv("LANG", "pt_BR.UTF-8", 1));
    Mods_SetLanguage(NULL);
    assert(!strcmp(Mods_Name(words), "Words"));

#ifdef MODS_SOURCE_DIR
    /* The mods the release ships: every lang file reads without a warning. */
    {
        DIR *folder = opendir(MODS_SOURCE_DIR);
        struct dirent *item;
        int checked = 0, later;
        assert(folder);
        assert(!setenv("MEMORIES_MODS_LANGUAGE", "pt-BR", 1));
        while ((item = readdir(folder))) {
            char directory[1024], file[1100];
            int mod;
            if (item->d_name[0] == '.') continue;
            snprintf(directory, sizeof(directory), "%s/%s", MODS_SOURCE_DIR, item->d_name);
            snprintf(file, sizeof(file), "%s/lang/pt-BR.json", directory);
            if (access(file, R_OK)) continue;
            mod = Mods_Discover(directory, &later);
            assert(mod >= 0);
            if (strstr(Mods_Status(mod), "lang/")) {
                fprintf(stderr, "%s: %s\n", item->d_name, Mods_Status(mod));
                assert(!"a shipped lang file has a warning");
            }
            assert(strcmp(Mods_Name(mod), "") && Mods_Metadata(mod, "description")[0]);
            checked++;
        }
        closedir(folder);
        assert(checked > 0);
    }
#endif

    puts("mods language: ok");
    return 0;
}
