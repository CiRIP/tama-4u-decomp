/* Declarations of src/item/item_special.c: its types, variables and functions. */
#ifndef PROTO_ITEM_ITEM_SPECIAL_H
#define PROTO_ITEM_ITEM_SPECIAL_H

#include "structs.h"

extern const u8 *const item_special_delete_texts[];

void item_special_menu(void);
int spot_bingo_card_viewer(void);
int item_coupon_viewer(void);
int item_special_delete_confirm(u8);
int item_coupon_erase(void);
u8 item_special_missing(u8);

#endif
