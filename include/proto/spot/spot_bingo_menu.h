/* Declarations of src/spot/spot_bingo_menu.c: its types, variables and functions. */
#ifndef PROTO_SPOT_SPOT_BINGO_MENU_H
#define PROTO_SPOT_SPOT_BINGO_MENU_H

#include "structs.h"

struct LabelPairB {
    u32 first;
    u32 second;
};

/* touch bingo plays today (max 3, cleared at midnight) */
/* the bingo spot's number (record byte 9) left by spot_reader, -1 when none */
extern s8 spot_bingo_number;

extern const struct LabelPairB spot_bingo_gashapon_menu_labels;
extern const u8 D_255CFD8[];
extern const u8 D_255CFEC[];
extern const u8 D_255D004[];
extern const u8 D_255D014[];

void spot_bingo_gashapon_menu(void);

#endif
