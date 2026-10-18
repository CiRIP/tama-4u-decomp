/*
 * The bingo card: marking numbers, prizes and drawing the card; with erasing a card.
 *
 * Code 251D666..251DB6E, .rodata from 255FAD8.
 */
#include "proto/spot/spot_bingo.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/item/item_use.h"
#include "proto/room/room_main.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"

/*
 * Bingo card screen k (spot_bingo_mark) run with anim_busy = 1 ("in a sub-screen": the RTC
 * tick defers its events).
 */
int spot_bingo_screen(s8 k)
{
    int r;

    anim_busy = 1;
    r = spot_bingo_mark(k);
    anim_busy = 0;
    return r;
}

/*
 * Bingo board: pet.bingo_board = (card layout 1..3) << 12 | 9-bit mask of marked cells; the
 * layouts' numbers are in tbl (3 x 9). Marks the cell holding the drawn number a
 * (a < 0: just redraw; 0 = a random number), draws the card picture from the Bingo item
 * at 0x2750000 and the cells (spot_bingo_draw_card), animates the new mark (6 blinks on the
 * timer-5 compare-B flag, tmr_flags bit 5), waits for a button or 10 s, then checks the 8
 * lines: a bingo gives the prize (item_prize_record(0, card) = prize item record, stored with
 * item_add_to_inventory, intro animation anim_capsule_prize, spot_bingo_card_erase), clears the card state and
 * returns 1; otherwise the mask is saved into pet.bingo_board.
 */
int spot_bingo_mark(s8 a)
{
    u8 pairs[9][2];
    u16 k = save.pet.bingo_board >> 12;
    u16 mask = save.pet.bingo_board & 0xfff;
    u8 tbl[3][9] = {
        { 2, 9, 5, 8, 1, 3, 6, 4, 7 },
        { 5, 2, 6, 1, 9, 3, 7, 4, 8 },
        { 1, 6, 2, 7, 5, 9, 3, 8, 4 },
    };
    u32 kept[15];
    u32 s[3];
    const u8 *buf[15];
    int x, y, r;
    u8 i, idx;
    u8 *p;
    int t;
    struct GfxObject *e;
    u16 old;

    r = 0;
    memcpy(kept, anim_pic_slots, 0x3c);
    if (k < 1 || k > 3)
        k = 1;
    if (a > 9)
        a = 1;
    while (a == 0)
        a = rand() % 10;
    p = tbl[k - 1];
    for (i = 0; i <= 8; i++) {
        pairs[i][0] = *p;
        pairs[i][1] = (mask >> i) & 1;
        if (pairs[i][0] == a) {
            mask |= 1 << i;
            pairs[i][1] = 1;
            idx = i + 2;
        }
        p++;
    }
    item_load_pictures(anim_pic_slots[0], (struct TamagoFile *)spot_bingo_card_slot, 0xf);
    s[0] = anim_pic_slots[0][0];
    switch (k) {
    case 1:
        s[1] = 0x21be018;
        break;
    case 2:
        s[1] = 0x21be39c;
        break;
    case 3:
        s[1] = 0x21be720;
        break;
    }
    s[2] = anim_pic_slots[0][1];
    spot_bingo_draw_card((int)s, (int)pairs);
    if (a < 0)
        return r;
    e = &gfx_objects[idx];
    tmr_t5_start_timeout(500);
    i = 0;
again:
wait:
    if (!(tmr_flags & 0x20))
        goto wait;
    tmr_flags &= ~0x20;
    if (i <= 5) {
        if (i & 1) {
            switch (idx) {
            case 2: x = 0x32; y = 0xf; break;
            case 3: x = 0x40; y = 0xf; break;
            case 4: x = 0x4e; y = 0xf; break;
            case 5: x = 0x32; y = 0x1d; break;
            case 6: x = 0x40; y = 0x1d; break;
            case 7: x = 0x4e; y = 0x1d; break;
            case 8: x = 0x32; y = 0x2b; break;
            case 9: x = 0x40; y = 0x2b; break;
            case 10: x = 0x4e; y = 0x2b; break;
            }
        } else {
            x = -100;
            y = e->y;
        }
        gfx_object_move_dirty(idx, x - (e->width >> 1), y - (e->height >> 1) + 0x1c);
        func_00009A6();
        i++;
        goto again;
    }
    T16_CTL5 = 0;
    btn_clear();
    ui_timeout = 10;
    do {
        sys_idle_tick();
        if (btn_a_presses != 0)
            break;
        if (btn_b_presses != 0)
            break;
        if (btn_c_presses != 0)
            break;
    } while (ui_timeout != 0);
    if ((pairs[0][1] && pairs[1][1] && pairs[2][1])
        || (pairs[3][1] && pairs[4][1] && pairs[5][1])
        || (pairs[6][1] && pairs[7][1] && pairs[8][1])
        || (pairs[0][1] && pairs[3][1] && pairs[6][1])
        || (pairs[1][1] && pairs[4][1] && pairs[7][1])
        || (pairs[2][1] && pairs[5][1] && pairs[8][1])
        || (pairs[0][1] && pairs[4][1] && pairs[8][1])
        || (pairs[2][1] && pairs[4][1] && pairs[6][1])) {
        t = item_prize_record(0, (struct TamagoFile *)spot_bingo_card_slot);
        item_load_pictures(buf, (struct TamagoFile *)t, 0xf);
        old = chara_other_sprite_set;
        chara_other_sprite_set = 0x35;
        anim_pic_slots[0][0] = (const u8 *)&spot_bingo_card_pic;
        anim_pic_slots[0][1] = (const u8 *)&spot_bingo_card_pic;
        anim_capsule_prize(buf[0]);
        item_add_to_inventory((struct TamagoFile *)t);
        chara_other_sprite_set = old;
        spot_bingo_card_erase();
        save.pet.bingo_board = 0;
        r = 1;
    } else {
        save.pet.bingo_board |= mask;
    }
    save_game();
    anim_happy();
    memcpy(anim_pic_slots, kept, 0x3c);
    return r;
}

/*
 * Draw a 3x3 card a with the marks q (9 cells, hidden ones moved off screen).
 */
void spot_bingo_draw_card(int a, int b)
{
    u32 *p = (u32 *)a;
    u8 *q = (u8 *)b;

    anim_scene_room(0xb);
    anim_draw_pic(0, (u8 *)*p, 0, 0x40, 0x24);
    p++;
    anim_draw_pic(1, (u8 *)*p, 0, 0x40, 0x1d);
    p++;
    anim_draw_pic(2, (u8 *)*p, 0, q[1] ? 0x32 : -100, 0xf);
    q += 2;
    anim_draw_pic(3, (u8 *)*p, 0, q[1] ? 0x40 : -100, 0xf);
    q += 2;
    anim_draw_pic(4, (u8 *)*p, 0, q[1] ? 0x4e : -100, 0xf);
    q += 2;
    anim_draw_pic(5, (u8 *)*p, 0, q[1] ? 0x32 : -100, 0x1d);
    q += 2;
    anim_draw_pic(6, (u8 *)*p, 0, q[1] ? 0x40 : -100, 0x1d);
    q += 2;
    anim_draw_pic(7, (u8 *)*p, 0, q[1] ? 0x4e : -100, 0x1d);
    q += 2;
    anim_draw_pic(8, (u8 *)*p, 0, q[1] ? 0x32 : -100, 0x2b);
    q += 2;
    anim_draw_pic(9, (u8 *)*p, 0, q[1] ? 0x40 : -100, 0x2b);
    anim_draw_pic(10, (u8 *)*p, 0, q[3] ? 0x4e : -100, 0x2b);
    anim_frame(500, 0);
}

/*
 * Erase flash sector 0x7D (0x2750000, the bingo card spot_bingo_card_slot: spot_bingo_mark's
 * prize resets the card) and clear mark 3.
 */
int spot_bingo_card_erase(void)
{
    int r;

    sys_irq_disable();
    r = func_0000A54(FLASH_BASE, FLASH_SEC_BINGO, FLASH_SEC_BINGO);
    sys_irq_enable();
    ui_set_mark(3, 0);
    return r;
}
