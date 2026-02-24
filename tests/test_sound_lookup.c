#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "sf33rd/Source/Game/sound/sound_ids.h"

// Define types needed for the test
typedef struct {
    s16 ptix;
    s16 bank;
    s16 port;
    u16 code;
} SoundRequestData;

// Prototype from sound_lookup.c
const SoundLookupEntry* Get_Sound_Lookup(SoundRequest id);

// Simulating sound processing logic for testing
u16 test_remake_logic(u16 code, SoundRequestData* rmcode) {
    if (code == 0) {
        rmcode->ptix = 0x7FFF;
        rmcode->bank = 0;
        rmcode->port = 0;
        rmcode->code = 0;
        return 0;
    }

    const SoundLookupEntry* lookup = Get_Sound_Lookup((SoundRequest)code);
    if (lookup) {
        rmcode->ptix = lookup->ptix;
        rmcode->bank = lookup->bank;
        rmcode->port = lookup->port;
        rmcode->code = lookup->engine_code;
        return 0;
    }
    return 1;
}

void test_remake_integration() {
    SoundRequestData rmcode;
    
    // Test SND_MENU_CURSOR (96)
    assert(test_remake_logic(96, &rmcode) == 0);
    assert(rmcode.code == 0x0060);
    assert(rmcode.ptix == 0x0000);
    
    // Test SND_MENU_SELECT (98)
    assert(test_remake_logic(98, &rmcode) == 0);
    assert(rmcode.code == 0x0061);
    assert(rmcode.ptix == 0x0000);

    // Test SND_BGM_CHARACTER_SELECT (57)
    assert(test_remake_logic(57, &rmcode) == 0);
    assert(rmcode.code == 0x0039);
    assert(rmcode.ptix == 0x007F);
    assert(rmcode.bank == 4);

    // Test SND_NONE (0)
    assert(test_remake_logic(0, &rmcode) == 0);
    assert(rmcode.ptix == 0x7FFF);

    printf("test_remake_integration passed\n");
}

int main() {
    test_remake_integration();
    printf("All engine integration tests passed!\n");
    return 0;
}