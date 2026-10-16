/*
 * Browsing the special-area TAMAGO records (decorations) with delete.
 *
 * Code 25193BE..25196A4, .rodata from 255F2B0.
 */
#include "proto/item/item_record_browser.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/room/room_main.h"
#include "proto/shop/shop_browser.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * Number of records with the magic D_255F2B4 in the special area (sector 0x62).
 */
int item_special_area_count(void)
{
    char buf[12];
    u16 sz;
    int count = 0;
    u8 a;
    u8 n;
    u8 *base;
    u8 *p;
    u32 i;
    const struct ItemBank *bk;

    memset(buf, 0, 10);
    strcpy(buf, D_255F2B4);
    bk = &item_bank_tbl[15];
    a = bk->first_sector;
    sz = bk->slot_size;
    n = bk->slot_count;
    base = flash_sector_addr(a);
    for (i = 0; i < n; i++) {
        p = base + sz * i;
        if (*(u16 *)p != 0xffff && strncmp((char *)p, buf, strlen(buf)) == 0)
            count++;
    }
    return count;
}

/*
 * Next stored record after b in download category a (c: only categories 0x81/0x94).
 */
int item_bank_next_record(int a, int b, int c)
{
    u8 *p;

    for (;;) {
        b++;
        if (b >= item_bank_tbl[a].slot_count)
            b = 0;
        p = item_bank_slot(a, b);
        if (*(int *)p == -1)
            continue;
        if (c == 0)
            break;
        if (p[0x4e] == 0x81 || p[0x4e] == 0x94) {
            if (p[0x50] != c && (p[0x50] != 0x35 || c != 0x34))
                continue;
            break;
        }
        if (p[0x4e] != 1 || p[0x4f] != 1 || (p[0x51] != 2 && p[0x51] != 3))
            break;
        if (c == 0xd)
            break;
    }
    return b;
}

/*
 * Browser for records of category a starting at e, with delete ("けす?").
 */
int item_record_browser(int a, int b, u8 *c, int d, int e, int f)
{
    u8 k;
    int r, x, t;
    u8 *p;

    snd_wait_end();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    k = item_bank_next_record(a, e, f);
loop:
    r = item_special_area_count();
    if (r == 0)
        return 0;
    p = (u8 *)c + d * k;
    switch ((u32)a) {
    case 9:
        break;
    case 7:
    case 8:
    default:
        item_draw_card((struct TamagoFile *)p, 0);
        break;
    }
    if (r > 1) {
        x = item_bank_next_record(a, k, f);
        if (k < x)
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, D_21059A4);
        else
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, D_21059A4);
    }
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses != 0) {
            k = item_bank_next_record(a, k, f);
            goto loop;
        }
        if (btn_b_presses != 0) {
            t = 1;
            switch (ui_yes_no(D_255F2BC, D_255F2C4, D_255F2CC, 0x10, &t)) {
            case 1:
                switch (t) {
                case 1:
                    goto loop;
                case 0:
                    if (ui_confirm_really()) {
                        snd_wait_end();
                        x = d * k;
                        p = (u8 *)c + x;
                        flash_backup_sector(b + ((u32)x >> 16));
                        flash_restore_sector_except(b + ((u32)x >> 16), p, d);
                        r = item_special_area_count();
                        snd_wait_end();
                        save_game();
                        if (r == 0)
                            return 0;
                        k = item_bank_next_record(a, k, f);
                        goto loop;
                    }
                    if (ui_timeout == 0)
                        return 0;
                    goto loop;
                }
                break;
            case 2:
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                ui_timeout = 10;
                goto loop;
            case 3:
                return 0;
            }
        }
        if (btn_c_presses != 0) {
            ui_timeout = 10;
            return 0;
        }
        sys_idle_tick();
        if (ui_timeout == 0)
            return 0;
    }
}

const char D_255F2B4[] = "TAMAGO";
const u8 D_255F2BC[] = _("けす?");
const u8 D_255F2C4[] = _("はい");
const u8 D_255F2CC[] = _("いいえ");

/*
 * Run the record browser item_record_browser on category 15, the special download area
 * item_bank_tbl[15] (sector 0x62, 4 slots of 8 KB); returns 1 when the browser returned
 * nonzero, else 0.
 */
int item_special_area_browser(void)
{
    const struct ItemBank *bk = &item_bank_tbl[15];
    u8 a = bk->first_sector;
    u16 b = bk->slot_size;
    u8 c = bk->slot_count;
    int r;

    r = item_record_browser(15, a, flash_sector_addr(a), b, c, 0);
    if (r != 0)
        r = 1;
    return r;
}
