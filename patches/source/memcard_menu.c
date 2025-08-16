#include "memcard_menu.h"

#include "attr.h"
#include "font.h"

__attribute_data__ gameid_t card_game_ids[2][CARD_MAX_FILE];

__attribute_reloc__ char* (*get_card_info)(s32 chan, s32 fileNo);
__attribute_reloc__ void (*draw_card_info)(char unk);

__attribute_used__ char *patched_card_info(s32 chan, s32 fileNo) {
    if (card_game_ids[chan][fileNo].parts.gamecode[3] == 'J') switch_lang_jpn();
    else switch_lang_eng();

    return get_card_info(chan, fileNo);
}

__attribute_used__ void fix_card_info(char unk) {
    draw_card_info(unk);
    switch_lang_orig();
}
