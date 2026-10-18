/*
 * The 'special' items: the bingo card viewer, the coupon viewer and its erase, and which of the
 * two is missing.
 *
 * Code 25136DA..2513DD8, .rodata from 255D298.
 */
#include "proto/item/item_special.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/spot/spot_bingo.h"
#include "flash.h"
#include "libc.h"

extern const u8 D_255D29C[];
extern const u8 D_255D2A8[];
/*
 * F49 = the "スペシャル" items: the BINGO CARD (the 0x8000-byte app-data item at 0x2750000
 * = sector 0x7D; F45 plays it, F63 spot_bingo_screen draws it) and the COUPON (sector 0x7C =
 * 0x2740000). item_special_menu = the special items menu (おどうぐ tools, スペシャル
 * special, ビンゴカード bingo card -> spot_bingo_card_viewer, クーポン coupon -> item_coupon_viewer; けす?
 * delete).
 */
void item_special_menu(void)
{
    const u8 *items[4] = { D_255D29C, D_255D2A8 };
    u8 bingo[] = "\x04\xbe\x04\xa4\x04\xb2\x04\x7c\x04\x51\x04\xbc";
    u8 coupon[] = "\x04\x7e\x04\x51\x04\xc6\x04\xa4";
    int v;
    u8 img, page, cur, old;
    u8 n = 4, per = 4;
    u8 i, idx, gray, color, f;
    int r;
    const u8 **ip;
    const struct Inventory *t;

    cur = 0;
    page = 0;
    old = 0;
    ip = items;
    ip[2] = bingo;
    ip[3] = coupon;
again:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    if (n > per) {
        f = 0;
        if (page >= (n - 1) / per)
            f = 1;
        gfx_object_create(0x72, 0x58, 0x10, f, 0, 0x21059a4);
    }
    for (i = 0; i < per; i++) {
        idx = page * per + i;
        if (idx >= n)
            break;
        switch (idx) {
        case 0:
            t = &item_inv_daily;
            if (item_inv_usable_count(t) != 0)
                gray = 0;
            else if (item_inv_usable_count(t + 1) != 0)
                gray = 0;
            else
                gray = 3;
            break;
        case 1:
            if (item_special_count() != 0)
                gray = 0;
            else
                gray = 3;
            break;
        default:
            gray = 0;
            if (items[idx] == bingo)
                gray = item_special_missing(1) ? 3 : 0;
            if (items[idx] == coupon)
                gray = item_special_missing(0) ? 3 : 0;
            break;
        }
        color = gray != 0 ? 0x24 : 0x20;
        img = gfx_object_create(0, 0, 0, 0, gray, 0);
        gfx_draw_text_line(img, 0x18, i * 16 + 0x20, color, (int)items[idx], 0, 8);
    }
    img = gfx_object_create(6, (u8)(cur % per) * 16 + 0x22, 0x30, 0, 0, 0x2104714);
    func_00009A6();
    btn_clear();
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            cur = cur < n - 1 ? (u8)(cur + 1) : 0;
            page = cur / per;
            if (old != page) {
                old = page;
                goto again;
            }
            gfx_object_move_dirty(img, 6, (u8)(cur % per) * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            switch (cur) {
            case 0:
                if (item_inv_usable_count(&item_inv_daily) == 0
                    && item_inv_usable_count(&item_inv_daily + 1) == 0) {
                    snd_error_beep();
                    btn_b_presses = 0;
                    ui_timeout = 10;
                    continue;
                }
                v = -1;
                f = save.pet.discipline_flags & 1;
                if (item_inv_picker_b(&item_inv_daily, 1, &v) < 0
                    && ui_timeout != 0)
                    goto again;
                if (f == 0 && (save.pet.discipline_flags & 1) != 0)
                    ui_timeout = 0;
                goto out;
            case 1:
                if (item_special_count() == 0) {
                    snd_error_beep();
                    btn_b_presses = 0;
                    ui_timeout = 10;
                    continue;
                }
                if (item_special_browser() < 0 && ui_timeout != 0)
                    goto again;
                goto out;
            default:
                if (items[cur] == bingo) {
                    if (item_special_missing(1)) {
                        snd_error_beep();
                        btn_b_presses = 0;
                        ui_timeout = 10;
                        continue;
                    }
                    r = spot_bingo_card_viewer();
                } else {
                    if (items[cur] != coupon)
                        goto out;
                    if (item_special_missing(0)) {
                        snd_error_beep();
                        btn_b_presses = 0;
                        ui_timeout = 10;
                        continue;
                    }
                    r = item_coupon_viewer();
                }
                if (r >= 0)
                    goto again;
                goto out;
            }
        }
        if (btn_c_presses)
            break;
        sys_idle_tick();
        if (ui_timeout == 0)
            break;
    }
out:
    btn_clear();
}

/*
 * Bingo card viewer (ビンゴカード entry): spot_bingo_screen draws the card, B -> "けす? はい/
 * いいえ" -> spot_bingo_card_erase erases it (sector 0x7D), pet.bingo_board = 0, save. 1 deleted, 0 kept,
 * -1 timeout.
 */
int spot_bingo_card_viewer(void)
{
    int r = 0;

again:
    spot_bingo_screen(-1);
    btn_clear();
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            r = item_special_delete_confirm(10);
            if (r > 0) {
                snd_wait_end();
                spot_bingo_card_erase();
                save.pet.bingo_board = 0;
                save_game();
                break;
            }
            if (r == 0)
                goto again;
            break;
        }
        if (btn_c_presses != 0)
            break;
        if (ui_timeout == 0)
            break;
        sys_idle_tick();
    }
    btn_clear();
    if (r == 0)
        r = ui_timeout == 0 ? -1 : 0;
    return r;
}

/*
 * Coupon viewer (クーポン entry): shows the main image of the item in sector 0x7C for up to
 * 60 s (display off on timeout when nothing is pending), B -> "けす?" -> item_coupon_erase erases
 * sector 0x7C.
 */
int item_coupon_viewer(void)
{
    u8 buf[0x3c];
    int r = 0;

again:
    ui_new_screen();
    memcpy(buf, anim_pic_slots, 0x3c);
    item_load_pictures(anim_pic_slots[0], (struct TamagoFile *)flash_sector_addr(FLASH_SEC_LETTERS), 0xf);
    gfx_object_create(0, 0x1c, 0, 0, 0, anim_pic_slots[0][0]);
    memcpy(anim_pic_slots, buf, 0x3c);
    func_00009A6();
    btn_clear();
    ui_timeout = 0x3c;
    for (;;) {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            ui_timeout = 0x3c;
        }
        if (btn_b_presses != 0) {
            r = item_special_delete_confirm(10);
            if (r > 0) {
                snd_wait_end();
                item_coupon_erase();
                break;
            }
            if (r == 0)
                goto again;
            break;
        }
        if (btn_c_presses != 0)
            break;
        if (ui_timeout == 0) {
            if ((s8)save.pet.calls >= 0 || pet_sleep_state() == 3) {
                if ((save.pet.calls & 0x70) == 0) {
                    sys_display_state = 2;
                    sys_set_backlight(0);
                    sys_apply_backlight();
                }
            }
            break;
        }
        sys_idle_seconds = 0;
        sys_idle_tick();
    }
    btn_clear();
    if (r == 0)
        r = ui_timeout == 0 ? -1 : 0;
    return r;
}

/*
 * Coupon/special item use or delete dialog.
 */
int item_special_delete_confirm(u8 a)
{
    u32 t[3];
    int r = 0;
    int sel = 1;
    u8 k;
    int c;

    memcpy(t, item_special_delete_texts, 0xc);
    ui_new_screen();
    gfx_object_create(r, 0x1c, r, r, r, 0x2104794);
    k = gfx_object_create(6, 0x52, 0x10, r, r, 0x2104714);
    c = gfx_object_create(r, r, r, r, r, r);
    gfx_draw_text_centered(c, 0x20, 0x20, (u8 *)t[0], r, 9);
    c = gfx_object_create(r, r, r, r, r, r);
    gfx_draw_text_line(c, 0x18, 0x40, 0x20, t[1], r, 2);
    c = gfx_object_create(r, r, r, r, r, r);
    gfx_draw_text_line(c, 0x18, 0x50, 0x20, t[2], r, 3);
    func_00009A6();
    btn_clear();
    ui_timeout = a;
    do {
        if (btn_a_presses != 0) {
            sel = !sel;
            gfx_object_move_dirty(k, 6, sel * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = a;
        }
        if (btn_b_presses != 0) {
            r = 0;
            if (sel != r)
                goto out;
            goto call;
        }
        if (btn_c_presses != 0)
            goto out;
        sys_idle_tick();
    } while (ui_timeout != 0);
out:
    btn_clear();
    switch (r) {
    case 0:
        if (ui_timeout != 0)
            r = 0;
        else
            r = -1;
        break;
    }
    goto ret;
call:
    r = ui_confirm_really();
    goto out;
ret:
    return r;
}

/*
 * Delete the stamp/coupon sector 0x7C, then save.
 */
int item_coupon_erase(void)
{
    int v0;

    sys_irq_disable();
    v0 = func_0000A54(FLASH_BASE, FLASH_SEC_LETTERS, FLASH_SEC_LETTERS);
    sys_irq_enable();
    snd_wait_end();
    save_game();
    return v0;
}

/*
 * Special item k missing? 0 = no coupon in sector 0x7C (first word erased), 1 = no valid
 * Bingo card at 0x2750000 (spot_bingo_card_kind validates it). Used to grey the menu entries.
 */
u8 item_special_missing(u8 k)
{
    u32 *p;
    u8 r;
    u8 f;

    switch (k) {
    case 0:
        p = (u32 *)flash_sector_addr(FLASH_SEC_LETTERS);
        break;
    case 1:
        p = (u32 *)spot_bingo_card_slot;
        f = 0;
        if (spot_bingo_card_kind((struct TamagoFile *)p) == -1)
            f = 1;
        return f;
    }
    r = 0;
    if (*p == 0xffffffff)
        r = 1;
    return r;
}

const u8 D_255D29C[] = _("おどうぐ");
const u8 D_255D2A8[] = _("スペシャル");
const u8 *const D_255D2B4[4] = { D_255D29C, D_255D2A8, 0, 0 };
const u8 D_255D2C4[] = _("ビンゴカード");
const u8 D_255D2D4[] = _("クーポン");
const u8 D_255D2E0[] = _("けす?");
const u8 D_255D2E8[] = _("はい");
const u8 D_255D2F0[] = _("いいえ");
const u8 *const item_special_delete_texts[3] = { D_255D2E0, D_255D2E8, D_255D2F0 };
