/* Declarations of src/item/item_record_browser.c: its types, variables and functions. */
#ifndef PROTO_ITEM_ITEM_RECORD_BROWSER_H
#define PROTO_ITEM_ITEM_RECORD_BROWSER_H

#include "structs.h"

extern const char D_255F2B4[];
extern const u8 D_255F2BC[];
extern const u8 D_255F2C4[];
extern const u8 D_255F2CC[];

int item_special_area_count(void);
int item_bank_next_record(int, int, int);
int item_record_browser(int, int, u8 *, int, int, int);
int item_special_area_browser(void);

#endif
