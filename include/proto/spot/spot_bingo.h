/* Functions defined in src/spot/spot_bingo.c. */
#ifndef PROTO_SPOT_SPOT_BINGO_H
#define PROTO_SPOT_SPOT_BINGO_H

#include "structs.h"

int spot_bingo_screen(s8);
int spot_bingo_mark(s8);
void spot_bingo_draw_card(int, int);
int spot_bingo_card_erase(void);

#endif
