#include <gctypes.h>
#include <ogc/system.h>
#include <ogc/dsp.h>

#include "attr.h"
#include "ipl.h"
#include "memcard_menu.h"
#include "reloc.h"
#include "picolibc.h"

#include "memcard.h"

// from https://github.com/Prakxo/ac-decomp/blob/b9554ef0cc3d8474047148882e898191f6e7bbb2/include/dolphin/private/card.h#L76
#define CARD_FILENAME_MAX 32
typedef struct CARDDir {
    // total size: 0x40
    u8 gameName[4];                 // offset 0x0, size 0x4
    u8 company[2];                  // offset 0x4, size 0x2
    u8 _padding0;                   // offset 0x6, size 0x1
    u8 bannerFormat;                // offset 0x7, size 0x1
    u8 fileName[CARD_FILENAME_MAX]; // offset 0x8, size 0x20
    u32 time;                       // offset 0x28, size 0x4
    u32 iconAddr;                   // offset 0x2C, size 0x4
    u16 iconFormat;                 // offset 0x30, size 0x2
    u16 iconSpeed;                  // offset 0x32, size 0x2
    u8 permission;                  // offset 0x34, size 0x1
    u8 copyTimes;                   // offset 0x35, size 0x1
    u16 startBlock;                 // offset 0x36, size 0x2
    u16 length;                     // offset 0x38, size 0x2
    u8 _padding1[2];                // offset 0x3A, size 0x2
    u32 commentAddr;                // offset 0x3C, size 0x4
} CARDDir;

// Based on the version in libogc2
typedef struct {
    s32 chn;
    s32 filenum;
    s32 offset;
    s32 len;
    u16 iblock;
} card_file;

typedef struct {
    u32 serial[0x08];
    u16 device_id;
    u16	size;
    u16 encoding;
    u8 padding[0x1d6];
    u16 chksum1;
    u16 chksum2;
} card_header;

// On NTSC 1.1, NTSC 1.2-001, NTSC 1.2-101, PAL 1.0, PAL 1.1, and PAL 1.2, this is 264 bytes, and the pointer to the header is 128 bytes in
typedef struct {
    u8 unknown0[128];
    card_header *header;
    u8 unknown1[132];
} card_block_standard;

// On NTSC 1.0 only, this is 176 bytes large, and the pointer to the header is 40 bytes in
typedef struct {
    u8 unknown0[40];
    card_header *header;
    u8 unknown1[132];
} card_block_ntsc10;

// Note that both of these use a different layout from the struct used by libogc and GCMM
typedef union {
    card_block_standard standard;
    card_block_ntsc10 ntsc10;
} card_block;

__attribute_reloc__ s32 (*__CARDGetStatusEx)(s32 chan, s32 fileNo, CARDDir* dirent);
__attribute_reloc__ s32 (*__CARDGetControlBlock)(s32 chan, card_block **card);
__attribute_reloc__ s32 (*__CARDPutControlBlock)(card_block *card, s32 ret);
__attribute_reloc__ s32 (*read_save_chunk)(card_file *file, u8 *read_buffer, u32 chunk_size, u32 chunk_offset);

__attribute_used__ s32 save_card_status(s32 chan, s32 fileNo, CARDDir* dirent) {
    s32 ret = __CARDGetStatusEx(chan, fileNo, dirent);
    if (ret == 0) {
        gameid_t *id = &card_game_ids[chan][fileNo];
        memcpy(id, &dirent->gameName[0], sizeof(gameid_t));
    }

    return ret;
}

typedef enum {
    save_patch_none,
    save_patch_fzero,
    save_patch_pso1_2,
    save_patch_pso3,
} save_patch_t;

save_patch_t patch_type_for_filename(const char *filename) {
    if (strcasecmp(filename, "f_zero.dat") == 0) {
        return save_patch_fzero;
    }
    if (strcasecmp(filename, "PSO_SYSTEM") == 0) {
        return save_patch_pso1_2;
    }
    if (strcasecmp(filename, "PSO3_SYSTEM") == 0) {
        return save_patch_pso3;
    }
    return save_patch_none;
}

// Based on GCMM
#define CARD_ERROR_UNLOCKED			1           /* card being unlocked or already unlocked. */
#define CARD_ERROR_READY            0           /* card is ready. */
#define CARD_ERROR_BUSY            -1           /* card is busy. */
#define CARD_ERROR_WRONGDEVICE     -2           /* wrong device connected in slot */
#define CARD_ERROR_NOCARD          -3           /* no memory card in slot */
#define CARD_ERROR_NOFILE          -4           /* specified file not found */
#define CARD_ERROR_IOERROR         -5           /* internal EXI I/O error */
#define CARD_ERROR_BROKEN          -6           /* directory structure or file entry broken */
#define CARD_ERROR_EXIST           -7           /* file allready exists with the specified parameters */
#define CARD_ERROR_NOENT           -8           /* found no empty block to create the file */
#define CARD_ERROR_INSSPACE        -9           /* not enough space to write file to memory card */
#define CARD_ERROR_NOPERM          -10          /* not enough permissions to operate on the file */
#define CARD_ERROR_LIMIT           -11          /* card size limit reached */
#define CARD_ERROR_NAMETOOLONG     -12          /* filename too long */
#define CARD_ERROR_ENCODING        -13          /* font encoding PAL/SJIS mismatch*/
#define CARD_ERROR_CANCELED        -14          /* card operation canceled */
#define CARD_ERROR_FATAL_ERROR     -128         /* fatal error, non recoverable */

static card_header *get_card_header(card_block *card) {
    switch (get_ipl_revision()) {
        case IPL_NTSC_10_001:
        case IPL_NTSC_10_002:
            return card->ntsc10.header;

        default:
            return card->standard.header;
    }
}

// Based on GCMM
static s32 get_card_serial_number(s32 channel, u32 *serial1, u32 *serial2) {
    if (channel < 0 || channel > 1) {
        return CARD_ERROR_FATAL_ERROR;
    }

    card_block *card = NULL;
    s32 ret = __CARDGetControlBlock(channel, &card);
    if (ret < 0) {
        return ret;
    }

    card_header *header = get_card_header(card);
    *serial1 = header->serial[0] ^ header->serial[2] ^ header->serial[4] ^ header->serial[6];
    *serial2 = header->serial[1] ^ header->serial[3] ^ header->serial[5] ^ header->serial[7];

    return __CARDPutControlBlock(card, ret);
}

// Based on GCMM
static s32 patch_fzero_save(u8 *buffer, u32 chunk_size, u32 chunk_offset, u32 serial1, u32 serial2) {
    if (chunk_size < 0x8000 || chunk_offset > 0) {
        return CARD_ERROR_NOPERM;
    }

    *(u16*)&buffer[0x2066] = serial1 >> 16;
    *(u16*)&buffer[0x7580] = serial2 >> 16;
    *(u16*)&buffer[0x2060] = serial1 & 0xFFFF;
    *(u16*)&buffer[0x2200] = serial2 & 0xFFFF;

    // calc 16-bit checksum
    u16 checksum = 0xFFFF;
    for (u32 i = 0x02; i < 0x8000; i++) {
        checksum ^= (buffer[i] & 0xFF);

        for (u32 j = 8; j > 0; j--) {
            if (checksum & 1) {
                checksum = (checksum >> 1) ^ 0x8408;
            } else {
                checksum >>= 1;
            }
        }
    }

    // set new checksum
    *(u16*)&buffer[0x00] = ~checksum;

    return CARD_ERROR_READY;
}

// Based on GCMM
static s32 patch_pso_save(u8* buffer, u32 chunk_size, u32 chunk_offset, u32 serial1, u32 serial2, bool is_pso3) {
    u32 pso3Offset = is_pso3 ? 0x10 : 0;
    if (chunk_size < 0x2165 + pso3Offset || chunk_offset > 0) {
        return CARD_ERROR_NOPERM;
    }

    // set new serial numbers
    *(u32*)&buffer[0x2158] = serial1;
    *(u32*)&buffer[0x215C] = serial2;

    // generate crc32 LUT
    u32 crc32LUT[256];
    for (u32 i = 0; i < 256; i++) {
        u32 checksum = i;

        for (u32 j = 8; j > 0; j--) {
            if (checksum & 1) {
                checksum = (checksum >> 1) ^ 0xEDB88320;
            } else {
                checksum >>= 1;
            }
        }

        crc32LUT[i] = checksum;
    }

    // PSO initial crc32 value
    u32 checksum = 0xDEBB20E3;

    // calc 32-bit checksum
    for (u32 i = 0x204C; i < 0x2164 + pso3Offset; i++) {
        checksum = ((checksum >> 8) & 0xFFFFFF) ^ crc32LUT[(checksum ^ buffer[i]) & 0xFF];
    }

    // set new checksum
    *(u32*)&buffer[0x2048] = checksum ^ 0xFFFFFFFF;

    return CARD_ERROR_READY;
}

__attribute_used__ s32 read_and_patch_save_chunk(card_file *file, u8 *read_buffer, u32 chunk_size, u32 chunk_offset) {
    CARDDir source_dir_entry;
    s32 ret = __CARDGetStatusEx(file->chn, file->filenum, &source_dir_entry);
    if (ret < 0) {
        return ret;
    }

    save_patch_t patch_type = patch_type_for_filename((char *)&source_dir_entry.fileName);

    ret = read_save_chunk(file, read_buffer, chunk_size, chunk_offset);
    if (ret < 0) {
        return ret;
    }

    // Get the serial number of the destination card, if we need it
    u32 serial1;
    u32 serial2;
    if (patch_type != save_patch_none) {
        u32 destination_memory_card_channel = file->chn == 0 ? 1 : 0;
        ret = get_card_serial_number(destination_memory_card_channel, &serial1, &serial2);
        if (ret < 0) {
            return ret;
        }
    }

    // Patch the save data accordingly
    switch (patch_type) {
        case save_patch_fzero:
            ret = patch_fzero_save(read_buffer, chunk_size, chunk_offset, serial1, serial2);
            break;

        case save_patch_pso1_2:
            ret = patch_pso_save(read_buffer, chunk_size, chunk_offset, serial1, serial2, /*is_pso3*/false);
            break;

        case save_patch_pso3:
            ret = patch_pso_save(read_buffer, chunk_size, chunk_offset, serial1, serial2, /*is_pso3*/true);
            break;

        case save_patch_none:
        default:
            break;
    }
    return ret;
}
