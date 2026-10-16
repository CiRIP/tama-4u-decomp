/*
 * The bingo / Gashapon menu: daily bingo plays and tickets.
 *
 * Code 2510B0A..2510B0A, .rodata from 255CFAC.
 */
#include "proto/spot/spot_bingo_menu.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/room/room_main.h"
#include "proto/comm/comm_menu.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/spot/spot_bingo.h"
#include "proto/spot/spot_gashapon.h"
#include "flash.h"

/*
 * "ビンゴゲーム / たまガシャポン" (Bingo game / Tama Gashapon) menu, reached from the main
 * menu: each entry is greyed (layer 3) when the matching app item is not installed (the
 * downloaded Bingo card record at 0x2750000, Gashapon ticket at 0x2760000). Bingo, by
 * the card id (spot_bingo_card_kind): a touch bingo card (x301-x599) with fewer than 3 plays today
 * (pet.bingo_plays_today, cleared at midnight) runs spot_touch_any_reader(0) (touch any reader) and then the
 * board spot_bingo_screen(0) with a random number (a win resets the play count); after the
 * third play of the day "きょうのビンゴはこれでおわりだよ!" (that was today's last bingo).
 * A spot bingo card (x001-x299) runs the bingo spot reader spot_bingo_read ->
 * spot_reader(0) and marks the spot's number spot_bingo_number (spot_bingo_screen(k)); no daily limit.
 * Gashapon: spot_touch_any_reader(1) (touch any reader) then spot_gashapon (the capsule animation
 * and prize).
 * A = move the cursor, B = select, C / 10 s timeout = leave.
 */
void spot_bingo_gashapon_menu(void)
{
    struct LabelPairB txt;
    u16 w;
    int ret;
    int sel;
    u8 c;
    u8 cur;
    u8 v;
    s8 k;

    ret = 0;
    txt = spot_bingo_gashapon_menu_labels;
    sel = 0;
redraw:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2104794);
    c = spot_bingo_card_kind(spot_bingo_card_slot) == -1 ? 3 : 0;
    w = 0x24;
    if (c == 0)
        w = 0x20;
    cur = gfx_object_create(0, 0, 0, 0, c, 0);
    gfx_draw_text_line(cur, 0x18, 0x20, w, txt.first, 0, 6);
    c = spot_is_gashapon_ticket(spot_gashapon_ticket_slot) == -1 ? 3 : 0;
    w = 0x24;
    if (c == 0) {
        goto set;
    ok1:
        spot_gashapon();
        room_view = ret;
        ui_timeout = 10;
        goto out;
    set:
        w = 0x20;
    }
    cur = gfx_object_create(0, 0, 0, 0, c, 0);
    gfx_draw_text_line(cur, 0x18, 0x30, w, txt.second, 0, 7);
    cur = gfx_object_create(6, sel * 16 + 0x22, 0x30, 0, 0, 0x2104714);
    func_00009A6();
    comm_remember_count();
    btn_clear();
    ui_timeout = 10;
    for (;;) {
        if (btn_a_presses) {
            sel = sel == 0;
            gfx_object_move_dirty(cur, 6, sel * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses) {
            switch (sel) {
            case 0:
                switch (spot_bingo_card_kind(spot_bingo_card_slot)) {
                case 0:
                    if (save.pet.bingo_plays_today > 2) {
                        snd_play(0xc);
                        snd_wait_end();
                        btn_b_presses = 0;
                        ui_timeout = 10;
                        continue;
                    }
                    spot_bingo_screen(-1);
                    room_idle = 0;
                    ret = spot_touch_any_reader(0);
                    if (ret == 0) {
                        if (spot_bingo_screen(ret) == 0) {
                            save.pet.bingo_plays_today++;
                            snd_wait_end();
                            save_game();
                            if (save.pet.bingo_plays_today > 2)
                                ui_message_box(D_255CFD8, D_255CFEC, D_255D004, D_255D014, ret);
                        } else {
                            save.pet.bingo_plays_today = 0;
                            snd_wait_end();
                            save_game();
                        }
                        goto clr;
                    }
                    if (ret == -1)
                        goto redraw;
                    room_view = 0;
                    goto out;
                case 1:
                    room_idle = 0;
                    spot_bingo_screen(-1);
                    spot_bingo_number = -1;
                    ret = spot_bingo_read();
                    k = spot_bingo_number;
                    if (k >= 0) {
                        room_idle = 0;
                        spot_bingo_screen(k);
                        room_view = 0;
                    }
                    if (ret == -1)
                        goto redraw;
                    if (ret == -2)
                        room_view = 0;
                    goto out;
                default:
                    snd_error_beep();
                    btn_b_presses = 0;
                    ui_timeout = 10;
                    continue;
                }
            case 1:
                if (spot_is_gashapon_ticket(spot_gashapon_ticket_slot) == 0) {
                    room_idle = 0;
                    ret = spot_touch_any_reader(1);
                    if (ret == 0)
                        goto ok1;
                    if (ret == -1)
                        goto redraw;
                clr:
                    room_view = 0;
                    goto out;
                }
                snd_error_beep();
                btn_b_presses = 0;
                ui_timeout = 10;
                continue;
            }
            break;
        }
        if (btn_c_presses)
            break;
        sys_idle_tick();
        if ((v = ui_timeout) == 0) {
            ui_main_icon = v;
            break;
        }
    }
out:
    comm_notebook_unlock_check();
    if (ret == -2) {
        sys_idle_seconds = 0;
        sys_display_state = 0;
    }
    btn_clear();
}

const u8 D_255CFB0[] = _("ビンゴゲーム");
const u8 D_255CFC0[] = _("たまガシャポン");
const struct LabelPairB spot_bingo_gashapon_menu_labels = { (u32)D_255CFB0, (u32)D_255CFC0 };
const u8 D_255CFD8[] = _("きょうのビンゴは");
const u8 D_255CFEC[] = _("これでおわりだよ!\0");
const u8 D_255D004[] = _("またあした\0");
const u8 D_255D014[] = _("あそんでね!");

/*
 * The RAM variables this file defines.
 */
s8 spot_bingo_number;
