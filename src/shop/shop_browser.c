/*
 * The item-bank browsers and the shops: the purchase confirmation, the discount (sale) mode,
 * browsing the occupied slots of a bank with item cards, the small two-entry menu, the fashion
 * shop (clothes / accessories) and the play-tools shop.
 *
 * Code 241585C..2416230, .rodata from 255C2C0.
 */
#include "proto/shop/shop_browser.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/shop/shop_reform.h"
#include "proto/diary/diary_notebook.h"
#include "proto/anim/anim_scripts.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"

/*
 * Price field of item record a (big-endian at +0x74 for downloads, +0x64 / +0x6A for
 * built-in records).
 */
u32 item_price_field(struct TamagoFile *a)
{
    u8 *p;
    u8 *q;

    if (a != 0) {
        if (a->x000 != -1) {
            p = (u8 *)a;
            switch (a->x04e) {
            case 1:
                if (p[0x4f] == 4)
                    a = (struct TamagoFile *)(p[0x6d] | p[0x6c] << 8 | p[0x6b] << 16 | p[0x6b] << 24);
                else
                    a = (struct TamagoFile *)(p[0x67] | p[0x66] << 8 | p[0x65] << 16 | p[0x64] << 24);
                break;
            case 0x81:
            case 0x94:
                q = (u8 *)a;
                a = (struct TamagoFile *)(q[0x77] | q[0x76] << 8 | q[0x75] << 16 | q[0x74] << 24);
                break;
            }
            return (u32)a;
        }
    }
    return 0;
}

/*
 * Price of item record a, halved or changed by the shop discount mode shop_discount_mode.
 */
u32 item_price(struct TamagoFile *a)
{
    u8 *p;

    if (a != 0) {
        if (a->x000 != -1) {
            p = (u8 *)a;
            switch (a->x04e) {
            case 1:
                if (p[0x4f] == 4)
                    a = (struct TamagoFile *)(p[0x6d] | p[0x6c] << 8 | p[0x6b] << 16 | p[0x6b] << 24);
                else
                    a = (struct TamagoFile *)(p[0x67] | p[0x66] << 8 | p[0x65] << 16 | p[0x64] << 24);
                break;
            case 0x81:
            case 0x94:
                p = (u8 *)a;
                a = (struct TamagoFile *)(p[0x77] | p[0x76] << 8 | p[0x75] << 16 | p[0x74] << 24);
                break;
            }
            switch (shop_discount_mode) {
            case 1:
                a = (struct TamagoFile *)((u32)a * 7 / 10);
                break;
            case 2:
                a = (struct TamagoFile *)((u32)a >> 1);
                break;
            }
            return (u32)a;
        }
    }
    return 0;
}

/*
 * Price of item number a.
 */
u16 item_price_by_index(int a)
{
    return item_price(item_record_by_index(a));
}

/*
 * F36 = the item-bank browsers and shops. Item banks are described by item_bank_tbl[k] (layout
 * at the data block below); item_bank_has(k, i, f) = slot i occupied (f = include the
 * built-in "shop stock" items listed after the download slots). Shop tables (shop_tbl_daily_tools =
 * にちようひん daily goods / あそびどうぐ play tools, shop_tbl_fashion = ようふく clothes /
 * アクセサリー accessories) are rows of {label, bank k, filter f} (struct BankListEntry). shop_discount_mode = shop badge /
 * discount mode (0 normal, 1 / 2 = sale icons drawn by item_bank_browser), set with
 * shop_set_discount_mode.
 *
 * shop_buy_confirm: "おかいあげ? はい/いいえ" (buy it? yes/no, D_255C4B8 / D_255C4C8 /
 * D_255C4D0) for item it: price item_price, affordable pet_can_pay, stored with
 * item_add_to_inventory and paid (pet_spend_points). Returns 1 when bought.
 */
int shop_buy_confirm(struct TamagoFile *it)
{
    u8 sel = 1;
    u8 a;
    u8 w;

    ui_new_screen();
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    a = gfx_object_create(0x18, 0x20, 0x38, 0, 0, 0);
    gfx_draw_text_centered(a, 0x20, 0x30, D_255C4B8, 0, 9);
    a = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x40, 0x34, (int)D_255C4C8, 0, 8);
    a = gfx_object_create(0x18, 0x20, 0x30, 0, 0, 0);
    gfx_draw_text_line(a, 0x18, 0x50, 0x34, (int)D_255C4D0, 0, 8);
    w = gfx_object_create(6, 0x50, 0x30, 0, 0, D_2104714);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses) {
            btn_a_presses = 0;
            sel = (u8)(sel + 1);
            if (sel > 1)
                sel = 0;
            gfx_object_move_dirty(w, 6, sel * 16 + 0x40);
            func_00009A6();
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            u32 n;
            btn_b_presses = 0;
            if (sel == 0) {
                n = item_price(it);
                if (pet_can_pay(n)) {
                    if (it != 0) {
                        u8 c;
                        snd_wait_end();
                        c = item_add_to_inventory(it);
                        btn_b_presses = sel;
                        if (c) {
                            pet_spend_points(item_price(it));
                            goto one;
                        }
                        if (ui_timeout)
                            ui_timeout = 10;
                        return 0;
                    }
                one:
                    return 1;
                }
            }
            ui_timeout = 10;
            return 0;
        }
        if (btn_c_presses) {
            ui_timeout = 10;
            btn_c_presses = 0;
            return 0;
        }
        sys_idle_tick();
    } while (ui_timeout);
    return 0;
}

/*
 * Set the shop discount mode shop_discount_mode (0 normal, 2 = sale).
 */
void shop_set_discount_mode(int a)
{
    shop_discount_mode = a;
}

/*
 * Shop discount mode.
 */
int shop_get_discount_mode(void)
{
    return shop_discount_mode;
}

/*
 * Next occupied slot after i in bank k (wrapping; f = include the built-in stock items
 * after the download slots).
 */
int item_bank_next_occupied(int k, int i, int f)
{
    do {
        i++;
        if (f) {
            if (i >= item_bank_tbl[k].slot_count + item_bank_tbl[k].builtin_count)
                i = 0;
        } else {
            if (i >= item_bank_tbl[k].slot_count)
                i = 0;
        }
    } while (item_bank_has(k, i, f) == 0);
    return i;
}

/*
 * Number of occupied slots in bank k (f = include the built-in stock items).
 */
int item_bank_count(int k, int f)
{
    int n, c, i;

    if (f)
        n = item_bank_tbl[k].slot_count + item_bank_tbl[k].builtin_count;
    else
        n = item_bank_tbl[k].slot_count;
    c = 0;
    for (i = 0; i < n; i++)
        if (item_bank_has(k, i, f))
            c++;
    return c;
}

/*
 * Item browser for bank k: shows item *p (item_draw_card draws the item card in style g,
 * with price?; a mark 0x214DFE4 on the download slots below x003, the shop badge, the
 * arrow when more than one item). A = next, B = returns the item record, C / timeout = 0.
 * *p < 0 starts at the first occupied slot; *p = current index.
 */
int item_bank_browser(int k, int f, int g, int *p)
{
    struct TamagoFile *it;
    u32 old;
    u32 i, j;

    btn_b_presses = 0;
    btn_c_presses = 0;
    if (*p < 0)
        *p = item_bank_next_occupied(k, item_bank_tbl[k].slot_count + item_bank_tbl[k].builtin_count, f);
    old = (u8)*p;
    i = 0;
    j = 0;
again:
    if (i > j)
        goto zero;
    it = item_bank_record(k, *p);
    item_draw_card(it, g);
    if (*p < item_bank_tbl[k].slot_count)
        gfx_object_create(0x69, 0x21, 0x60, 0, 0, D_214DFE4);
    switch (shop_discount_mode) {
    case 1:
        gfx_object_create(7, 0x2a, 0x70, 0, 0, D_208D488);
        break;
    case 2:
        gfx_object_create(7, 0x2a, 0x70, 0, 0, D_2194F18);
        break;
    }
    if (item_bank_count(k, f) > 1) {
        if (*p < item_bank_next_occupied(k, *p, f))
            gfx_object_create(0x72, 0x58, 0x30, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x30, 1, 0, D_21059A4);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
wait:
    if (btn_a_presses != 0) {
        *p = item_bank_next_occupied(k, *p, f);
        goto again;
    }
    if (btn_b_presses != 0)
        return (int)it;
    if (btn_c_presses != 0)
        goto zero;
    sys_idle_tick();
    if (ui_timeout != 0)
        goto wait;
zero:
    return 0;
}

/*
 * Small menu of up to two entries from table t (12-byte rows, label first; n entries,
 * cursor sel): returns the entry + 1, 0 on C, -1 on timeout.
 */
int ui_small_menu(int t, u8 n, u8 sel)
{
    u8 c;
    int i;
    int *e;
    u8 cur;

    ui_new_screen();
    cur = sel;
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    for (i = 0; i <= 1; i++) {
        e = (int *)(i * 12 + t);
        if (*e == 0)
            break;
        gfx_draw_text_line(gfx_object_create(0x18, 0x20, i * 4 + 0x30, 0, 0, 0), 0x18, i * 16 + 0x20, i * 4 + 0x30, *e, 0, 8);
    }
    c = gfx_object_create(6, cur * 16 + 0x22, 0x20, 0, 0, D_2104714);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            cur++;
            if (cur >= n)
                cur = 0;
            gfx_object_move_dirty(c, 6, cur * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0)
            return cur + 1;
        if (btn_c_presses != 0)
            return 0;
        sys_idle_tick();
    } while (ui_timeout != 0);
    return -1;
}

/*
 * Shop browser over row a of shop table tbl: browse the bank (item_bank_browser); for a
 * download slot the three-way "おかいあげ?" (buy / no / けす delete - the delete erases
 * the slot through the scratch-sector rewrite after "ほんとうに?"), for stock items the
 * yes/no box; buying = affordable check, store, pay, save, callback cb (the purchase
 * animation), activity counter pet_count_activity(1). Returns 1 bought / 2 nothing (left with
 * B), 3 / 4 the same after a browse cancel, 5 / 6 after a timeout; b = keep browsing after
 * a purchase.
 */
#define T ((struct BankListEntry *)tbl)
int shop_browser(u8 *tbl, int a, int f, int *p, void (*cb)(void), int b)
{
    u8 k = a;
    u8 g = b;
    int done = 0;
    int one = 1;
    struct TamagoFile *it;
    u32 sel;
    u32 sel2;
    int x2;
    u32 x0;
    int r;

    ui_new_screen();
again:
    it = (struct TamagoFile *)item_bank_browser(T[k].bank, T[k].flags, f, p);
    if (it) {
        if (*p < item_bank_tbl[T[k].bank].slot_count) {
            sel = 1;
            switch ((u32)ui_use_no_delete((int)D_255C4B8, 0, &sel)) {
            case 1:
                switch (sel) {
                case 0:
                    pet_count_activity(1);
                    break;
                case 1:
                    goto again;
                case 2:
                    if (ui_confirm_really()) {
                        x2 = item_bank_tbl[T[k].bank].first_sector;
                        x0 = item_bank_tbl[T[k].bank].slot_size;
                        btn_b_presses = 0;
                        btn_c_presses = 0;
                        snd_wait_end();
                        flash_backup_sector(x2 + (x0 * *p >> 16));
                        flash_restore_sector_except(x2 + (x0 * *p >> 16), it, x0);
                        *p = item_bank_next_occupied(T[k].bank, *p, T[k].flags);
                        snd_wait_end();
                        save_game();
                        goto again;
                    }
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    if (ui_timeout)
                        goto again;
                    r = 6;
                    if (done)
                        r = 5;
                    return r;
                }
                break;
            case 2:
                goto again;
            case 3:
                r = 6;
                if (done)
                    r = 5;
                return r;
            }
        } else {
            sel2 = 1;
            switch (ui_yes_no(D_255C4B8, D_255C4C8, D_255C4D0, 0x10, (int *)&sel2)) {
            case 1:
                switch (sel2) {
                case 0:
                    pet_count_activity(1);
                    break;
                case 1:
                    goto again;
                }
                break;
            case 2:
                goto again;
            case 3:
                r = 6;
                if (done)
                    r = 5;
                return r;
            }
        }
        {
            u32 n = item_price(it);
            if (pet_can_pay(n)) {
                btn_a_presses = one;
                btn_b_presses = one;
                btn_c_presses = one;
                snd_wait_end();
                if (item_add_to_inventory(it)) {
                    pet_spend_points(item_price(it));
                    save_game();
                    done = 1;
                    cb();
                    if (g)
                        goto again;
                    goto out;
                }
                if (ui_timeout) {
                    ui_timeout = 10;
                    goto again;
                }
                goto out;
            }
            ui_timeout = 10;
            goto again;
        }
    } else {
        if (ui_timeout == 0) {
            r = 6;
            if (done)
                r = 5;
            return r;
        }
        r = 4;
        if (done)
            r = 3;
        return r;
    }
out:
    return done ? 1 : 2;
}

/*
 * item_bank_tbl (in this data block): the item banks / download categories, 12-byte entries
 * {u16 x000 slot size, u8 x002 first sector (1-based), u8 x003 slot count, u32 x004 table
 * of the built-in item numbers shown after the slots (the shop stock), u8 x008 number of
 * built-in items}: 0/1 2 KB x 20, 2 16 KB x 5, 3..6 8 KB, 7 games 32 KB x 2, 8 town
 * places/apps 32 KB x 4, 9 letters 1 KB x 40, 10 pictures 10 KB x 6.
 * Filled by item_store_in_bank from received TAMAGO files (download pack categories, see
 * notes/findings/tamago.md): 0 meals, 1 snacks, 2 clothes, 3 accessories, 4 goods / toys,
 * 5 goods of sub-kind 2, 6 rooms, 7 downloadable games, 8 downloadable outings, 15
 * seasonal decorations (ids x801..x949), 16 coupons (x601..x749), 17 bingo card, 18
 * gashapon; 11..14 and 19..21 only list built-in items.
 */
const u8 D_255C2C4[] = {
    0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
};
const u8 D_255C2D8[] = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
    0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
};
const u8 D_255C2EC[] = { 0x3e, 0x3f, 0x40, 0x41 };
const u8 D_255C2F0[] = { 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a };
const u8 D_255C2F8[] = { 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x24 };
const u8 D_255C308[] = { 0x00 };
const u8 D_255C30C[] = { 0x2f, 0x3a, 0x35, 0x3b, 0x3c, 0x3d, 0x36, 0x37, 0x38, 0x39 };
const u8 D_255C318[] = { 0x30 };
const u8 D_255C31C[] = { 0x31 };
const u8 D_255C320[] = { 0x32 };
const u8 D_255C324[] = { 0x33 };
const u8 D_255C328[] = { 0x44, 0x42, 0x43 };
const u8 D_255C32C[] = { 0x00, 0x01, 0x02 };
const u8 D_255C330[] = { 0x22, 0x23, 0x25, 0x26, 0x27, 0x28 };
const struct ItemBank item_bank_tbl[23] = {
    { 0x800, FLASH_SEC_MEALS, 20, D_255C2C4, 20 }, /* 0 */
    { 0x800, FLASH_SEC_SNACKS, 20, D_255C2D8, 20 }, /* 1 */
    { 0x4000, FLASH_SEC_CLOTHES, 5, D_255C2EC, 4 }, /* 2 */
    { 0x2000, FLASH_SEC_ACCESSORIES, 10, D_255C2F0, 8 }, /* 3 */
    { 0x2000, FLASH_SEC_TOYS, 15, D_255C2F8, 14 }, /* 4 */
    { 0x2000, FLASH_SEC_BANK5, 5, D_255C308, 1 }, /* 5 */
    { 0x2000, FLASH_SEC_ROOMS, 10, D_255C30C, 10 }, /* 6 */
    { 0x8000, FLASH_SEC_GAMES, 2, 0, 0 }, /* 7 */
    { 0x8000, FLASH_SEC_OUTINGS, 4, 0, 0 }, /* 8 */
    { 0x400, FLASH_SEC_LETTERS, 40, 0, 0 }, /* 9 */
    { 0x2800, FLASH_SEC_PHOTOS, 6, 0, 0 }, /* 10 */
    { 0x2000, FLASH_SEC_ROOMS, 0, D_255C318, 1 }, /* 11 */
    { 0x2000, FLASH_SEC_ROOMS, 0, D_255C31C, 1 }, /* 12 */
    { 0x2000, FLASH_SEC_ROOMS, 0, D_255C320, 1 }, /* 13 */
    { 0x2000, FLASH_SEC_ROOMS, 0, D_255C324, 1 }, /* 14 */
    { 0x2000, FLASH_SEC_DECORATIONS, 4, 0, 0 }, /* 15 */
    { 0x2000, FLASH_SEC_LETTERS, 1, 0, 0 }, /* 16 */
    { 0x8000, FLASH_SEC_BINGO, 1, 0, 0 }, /* 17 */
    { 0x8000, FLASH_SEC_GASHAPON, 1, 0, 0 }, /* 18 */
    { 0, 0, 0, D_255C328, 3 }, /* 19 */
    { 0, 0, 0, D_255C32C, 3 }, /* 20 */
    { 0, 0, 0, D_255C330, 6 }, /* 21 */
    { 0, 0, 0, 0, 0 }, /* 22 */
};
const u8 D_255C44C[] = _("にちようひん");
const u8 D_255C45C[] = _("あそびどうぐ");
const struct BankListEntry shop_tbl_daily_tools[] = {
    { D_255C44C, 5, 0x29 },
    { D_255C45C, 4, 0x2a },
};
const u8 D_255C484[] = _("ようふく");
const u8 D_255C490[] = _("アクセサリー");
const struct BankListEntry shop_tbl_fashion[] = {
    { D_255C484, 2, 0x33 },
    { D_255C490, 3, 0x34 },
};
const u8 D_255C4B8[] = _("おかいあげ?");
const u8 D_255C4C8[] = _("はい");
const u8 D_255C4D0[] = _("いいえ");

/*
 * Two-tab shop: ようふく clothes / アクセサリー accessories (table shop_tbl_fashion), sale
 * badge when flag 0x18 is set, purchase animation D_24656B0. Returns 1 when something was
 * bought.
 */
int shop_fashion(void)
{
    int r = 0;
    u8 sel = 0;
    int k;
    int v;

    if (pet_get_unlock_flag(0x18) != 0)
        shop_set_discount_mode(2);
again:
    k = ui_small_menu((int)shop_tbl_fashion, 2, sel);
    switch (k) {
    case -1:
    case 0:
        goto out;
    case 1:
    case 2:
        break;
    default:
        goto out;
    }
    sel = k - 1;
    v = -1;
    switch (shop_browser((u8 *)shop_tbl_fashion, sel, 1, &v, (void (*)(void))D_24656B0, 1)) {
    case 0:
    case 4:
        goto again;
    case 3:
        r = 1;
        goto again;
    case 1:
    case 5:
        r = 1;
        goto out;
    case 2:
    case 6:
        goto out;
    default:
        goto again;
    }
out:
    shop_set_discount_mode(0);
    return r;
}

/*
 * Single-tab shop: あそびどうぐ play tools (row 1 of shop_tbl_daily_tools, bank 4): sale badge on flag 0x15, purchase animation
 * anim_toy_shop. Returns 1 when something was bought.
 */
int shop_play_tools(void)
{
    int r = 0;
    int k;
    int v;

    if (pet_get_unlock_flag(0x15) != r)
        shop_set_discount_mode(2);
    for (;;) {
        k = 1;
        v = -1;
        switch (shop_browser((u8 *)shop_tbl_daily_tools, k, k, &v, anim_toy_shop, k)) {
        case 0:
            continue;
        case 1:
        case 3:
        case 5:
            r = 1;
        case 2:
        case 4:
        case 6:
            shop_set_discount_mode(0);
            return r;
        }
    }
}

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 shop_discount_mode DSTRAM;
