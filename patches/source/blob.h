#pragma once

#include <gctypes.h>

typedef struct {
    u32 magic;
    u32 textures_offset;
    u32 text_offset;
    u32 borders_offset;
    u16 texture_count;
    u16 text_count;
    u16 border_count;
    u16 unk0;
} blob_header_t;

typedef struct {
    u32 magic;
    u16 x_position;
    u16 y_position;
    u16 width;
    u16 height;
    u8 unk0;
    u8 texture_index;
    u8 unk1;
    u8 unk2;
} blob_texture_element_t;

typedef struct {
    u32 magic;
    u32 unknown0; // Is this ever read?
} sth0_blob_header_t;

typedef struct {
    u16 glh0_offset; // Used as: `glh0_header + (glh0_offset * 36)` - keep this intact when patching sth0 blobs
    u16 unknown0; // Is this ever read?
    u32 offset;
} sth0_blob_offset_t;


const sth0_blob_offset_t *get_sth0_blob_offset(const sth0_blob_header_t *sth0_blob, u32 index);
const char *get_sth0_blob_string(const sth0_blob_header_t *sth0_blob, u32 index);
