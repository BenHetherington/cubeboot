#pragma once

#include <gctypes.h>

#define CARD_MAX_FILE 127

typedef union {
    struct {
        u8 gamecode[4];
        u8 company[2];
    } parts;
    u8 blob[6];
} gameid_t;

extern gameid_t card_game_ids[2][CARD_MAX_FILE];
