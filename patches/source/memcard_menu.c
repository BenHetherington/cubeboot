#include "memcard_menu.h"

#include "attr.h"
#include "blob.h"
#include "element_alpha.h"
#include "font.h"

#include <ogc/gx.h>

#include <stddef.h>
#include <string.h>

typedef struct {
    u8 unknown0[4];
    bool is_copyable;
    bool is_movable;
    u8 unknown1[6];
} card_menu_entry_t;

typedef struct {
    u32 entry_index;
    u32 something;
} card_menu_grid_to_entry_index_t;

typedef struct {
    u8 selected_card_number;
    u8 unknown0[3];
    u8 x_position[2];
    u8 unknown1[2];
    u8 y_position[2];
    u8 unknown2[2];
    u8 y_scroll[2];
    // It seems like more variables follow, but this is all we need
} card_menu_state_t;

__attribute_data__ gameid_t card_game_ids[2][CARD_MAX_FILE];

__attribute_reloc__ char* (*get_card_info)(s32 chan, s32 fileNo);
__attribute_reloc__ void (*draw_card_info)(char unk);
__attribute_reloc__ void (*draw_move_copy_erase)(u32 card_number, const element_alpha_state_t *element_alpha, const sth0_blob_header_t *sth0_blob, const blob_header_t *glh0_blob, const GXColor *color);

__attribute_reloc__ card_menu_entry_t (*card_menu_entries)[2][CARD_MAX_FILE];
__attribute_reloc__ card_menu_grid_to_entry_index_t (*card_menu_grid_to_entry_indices)[2][CARD_MAX_FILE];

__attribute_reloc__ card_menu_state_t *card_menu_state;

__attribute_used__ char *patched_card_info(s32 chan, s32 fileNo) {
    if (card_game_ids[chan][fileNo].parts.gamecode[3] == 'J') switch_lang_jpn();
    else switch_lang_eng();

    return get_card_info(chan, fileNo);
}

__attribute_used__ void fix_card_info(char unk) {
    draw_card_info(unk);
    switch_lang_orig();
}

static card_menu_grid_to_entry_index_t *get_entry_index_for_slot(u32 card_number, u8 x_position, u8 y_position, u8 y_scroll) {
    u32 index = x_position + ((y_position + y_scroll) * 4);
    return &(*card_menu_grid_to_entry_indices)[card_number][index];
}

static card_menu_grid_to_entry_index_t *get_entry_index_for_selected_slot(u32 card_number) {
    return get_entry_index_for_slot(card_number, card_menu_state->x_position[card_number], card_menu_state->y_position[card_number], card_menu_state->y_scroll[card_number]);
}

typedef struct {
    sth0_blob_header_t header;
    sth0_blob_offset_t open_a_offset;
    sth0_blob_offset_t open_b_offset;
    sth0_blob_offset_t move_offset;
    sth0_blob_offset_t copy_offset;
    sth0_blob_offset_t erase_offset;
    sth0_blob_offset_t yes_offset;
    sth0_blob_offset_t no_offset;
    // 40 more offsets should follow these, but aren't needed for the move/copy/erase text
    char move[16];
    char copy[16];
    char erase[16];
    char yes[16];
    char no[16];
} move_copy_erase_text_blob_t;

__attribute_used__ void patch_draw_move_copy_erase(u32 card_number, const element_alpha_state_t *element_alpha, const sth0_blob_header_t *sth0_blob, const blob_header_t *glh0_blob, const GXColor *color) {
    u32 selected_slot = get_entry_index_for_selected_slot(card_number)->entry_index;
    card_menu_entry_t *card_menu_entry = &(*card_menu_entries)[card_number][selected_slot];
    bool is_selected_save_normally_movable = card_menu_entry->is_movable;
    bool is_selected_save_normally_copyable = card_menu_entry->is_copyable;

    move_copy_erase_text_blob_t patched_sth0_blob = (move_copy_erase_text_blob_t){
        .header = (sth0_blob_header_t){
            .magic = make_type('S', 'T', 'H', '0'),
            .unknown0 = sth0_blob->unknown0
        },
        .open_a_offset = (sth0_blob_offset_t){ 0, 0, 0 },
        .open_b_offset = (sth0_blob_offset_t){ 0, 0, 0 },
        .move_offset = (sth0_blob_offset_t){
            .glh0_offset = get_sth0_blob_offset(sth0_blob, 2)->glh0_offset,
            .unknown0 = get_sth0_blob_offset(sth0_blob, 2)->unknown0,
            offsetof(move_copy_erase_text_blob_t, move) - offsetof(move_copy_erase_text_blob_t, move_offset)
        },
        .copy_offset = (sth0_blob_offset_t){
            .glh0_offset = get_sth0_blob_offset(sth0_blob, 3)->glh0_offset,
            .unknown0 = get_sth0_blob_offset(sth0_blob, 3)->unknown0,
            offsetof(move_copy_erase_text_blob_t, copy) - offsetof(move_copy_erase_text_blob_t, copy_offset)
        },
        .erase_offset = (sth0_blob_offset_t){
            .glh0_offset = get_sth0_blob_offset(sth0_blob, 4)->glh0_offset,
            .unknown0 = get_sth0_blob_offset(sth0_blob, 4)->unknown0,
            offsetof(move_copy_erase_text_blob_t, erase) - offsetof(move_copy_erase_text_blob_t, erase_offset)
        },
        .yes_offset = (sth0_blob_offset_t){
            .glh0_offset = get_sth0_blob_offset(sth0_blob, 5)->glh0_offset,
            .unknown0 = get_sth0_blob_offset(sth0_blob, 5)->unknown0,
            offsetof(move_copy_erase_text_blob_t, yes) - offsetof(move_copy_erase_text_blob_t, yes_offset)
        },
        .no_offset = (sth0_blob_offset_t){
            .glh0_offset = get_sth0_blob_offset(sth0_blob, 6)->glh0_offset,
            .unknown0 = get_sth0_blob_offset(sth0_blob, 6)->unknown0,
            offsetof(move_copy_erase_text_blob_t, no) - offsetof(move_copy_erase_text_blob_t, no_offset)
        }
    };

    // The 'Force' text is not localized, and is English everywhere - ideally this would be displayed in the user's language
    const char *original_move_string = get_sth0_blob_string(sth0_blob, 2);
    strcpy(patched_sth0_blob.move, is_selected_save_normally_movable ? original_move_string : "Force Move");

    const char *original_copy_string = get_sth0_blob_string(sth0_blob, 3);
    strcpy(patched_sth0_blob.copy, is_selected_save_normally_copyable ? original_copy_string : "Force Copy");

    const char *original_erase_string = get_sth0_blob_string(sth0_blob, 4);
    strcpy(patched_sth0_blob.erase, original_erase_string);

    const char *original_yes_string = get_sth0_blob_string(sth0_blob, 5);
    strcpy(patched_sth0_blob.yes, original_yes_string);

    const char *original_no_string = get_sth0_blob_string(sth0_blob, 6);
    strcpy(patched_sth0_blob.no, original_no_string);

    draw_move_copy_erase(card_number, element_alpha, &patched_sth0_blob.header, glh0_blob, color);
}
