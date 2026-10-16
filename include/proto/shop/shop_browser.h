/* Declarations of src/shop/shop_browser.c: its types, variables and functions. */
#ifndef PROTO_SHOP_SHOP_BROWSER_H
#define PROTO_SHOP_SHOP_BROWSER_H

#include "structs.h"

struct ItemBank {
    u16 slot_size;
    u8 first_sector;
    u8 slot_count;
    const u8 *builtin_list;
    u8 builtin_count;
};

extern u8 shop_discount_mode;

extern const u8 D_255C30C[];
extern const struct ItemBank item_bank_tbl[];
extern const u32 D_255C4D8;
extern const u32 D_255C4F4;
extern const struct BankListEntry shop_tbl_daily_tools[];
extern const struct BankListEntry shop_tbl_fashion[];
extern const u8 D_255C4B8[];
extern const u8 D_255C4C8[];
extern const u8 D_255C4D0[];

u32 item_price_field(struct TamagoFile *);
u32 item_price(struct TamagoFile *);
u16 item_price_by_index(int);
int shop_buy_confirm(struct TamagoFile *);
void shop_set_discount_mode(int);
int shop_get_discount_mode(void);
int item_bank_next_occupied(int, int, int);
int item_bank_count(int, int);
int item_bank_browser(int, int, int, int *);
int ui_small_menu(int, u8, u8);
int shop_browser(u8 *, int, int, int *, void (*)(void), int);
int shop_fashion(void);
int shop_play_tools(void);

#endif
