#include "blob.h"

const sth0_blob_offset_t *get_sth0_blob_offset(const sth0_blob_header_t *sth0_blob, u32 index) {
    const void *end_of_blob_header = &sth0_blob[1];
    const sth0_blob_offset_t *offsets = end_of_blob_header;
    return &offsets[index];
}

const char *get_sth0_blob_string(const sth0_blob_header_t *sth0_blob, u32 index) {
    const sth0_blob_offset_t *found_text_offset = get_sth0_blob_offset(sth0_blob, index);
    return (const char *)found_text_offset + found_text_offset->offset;
}
