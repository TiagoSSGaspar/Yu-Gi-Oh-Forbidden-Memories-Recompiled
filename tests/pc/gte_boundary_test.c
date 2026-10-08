#include "pc/compat/gte.h"
#include "pc/guest/translated_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tools/pc/test_gte_boundaries.py. Built as an ordinary native unit, the
 * software GTE still takes guest addresses where a caller hands it one (lwc2
 * and swc2, from translated game code), and a seeded run of commands gives the
 * same registers as the instrumented translation. */
int main(void)
{
    static const uint8_t word[4] = {0x34, 0x12, 0xcd, 0xab};
    MemoriesMemory *memory = calloc(1, sizeof(*memory));
    unsigned bank, i, size = 0, random = 0x2b1f3c5du;
    uint64_t hash = 1469598103934665603ULL;
    assert(memory && !GuestRuntime_Bind(memory));
    for (bank = 0; bank < 3; bank++) {
        uint32_t address = (uint32_t[]){0x00020000u, 0x80020000u, 0xa0020000u}[bank];
        uint8_t *ram = Memories_Resolve(memory, address, 8, 4);
        assert(ram);
        memset(ram, 0, 8);
        memcpy(ram, word, sizeof(word));
        Memories_GteReset();
        Memories_GteLoad(0, (const void *)(uintptr_t)address); /* VXY0 */
        assert(Memories_GteReadData(0) == 0xabcd1234u);
        Memories_GteStore(0, (void *)(uintptr_t)(address + 4));
        assert(!memcmp(ram + 4, word, sizeof(word)));
        Memories_GteStoreWord(0x01020304u, (void *)(uintptr_t)address);
        assert(ram[0] == 4 && ram[1] == 3 && ram[2] == 2 && ram[3] == 1);
    }
    assert(Gte_StateData(&size) && size);
    /* Every register through random command words, implemented or not. */
    Memories_GteReset();
    for (i = 0; i < 20000; i++) {
        unsigned index;
        random = random * 1664525u + 1013904223u;
        index = random >> 27;
        if (i % 3 == 0) Memories_GteWriteControl(index, random * 2654435761u);
        else Memories_GteWriteData(index, random * 2246822519u);
        random = random * 1664525u + 1013904223u;
        Memories_GteCommand(random >> 7);
        for (index = 0; index < 32; index++) {
            hash = (hash ^ Memories_GteReadData(index)) * 1099511628211ULL;
            hash = (hash ^ Memories_GteReadControl(index)) * 1099511628211ULL;
        }
    }
    printf("Seeded GTE register hash: %016llx\n", (unsigned long long)hash);
    GuestRuntime_Reset();
    free(memory);
    puts("GTE boundary: lwc2/swc2 through three guest aliases passed");
    return 0;
}
