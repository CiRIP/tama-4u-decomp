/*
 * The game corner menu: the two built-in games and the two downloaded game slots, drawing the
 * page, running the chosen game.
 *
 * Code 251A6E0..251A9BA, .rodata from 255FA00. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/game/game_menu.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_window.h"
#include "proto/town/town_places.h"
#include "proto/shop/shop_browser.h"
#include "proto/anim/anim_scripts.h"
#include "proto/evo/evo_counters.h"
#include "proto/comm/comm_menu.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/dl/dl_chara.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/game/game_builtin.h"

const u8 D_255FA04[] = _("おたからザクザク");
const u8 D_255FA18[] = _("ケーキできるかな");
const struct GameEntry game_menu_tbl[6] = {
    { (u32)D_255FA04, (u32)game_treasure },
    { (u32)D_255FA18, (u32)game_cake },
};
const u8 D_255FA5C[] = _("あそぶ?");
/*
 * game_menu_tbl (in this data block): the game center list: {name, entry function} for the
 * built-in games "おたからザクザク" (game_treasure) and "ケーキできるかな" (game_cake);
 * the two following slots are the downloaded games of category 7 (game_menu).
 */

/*
 * Game menu state p[6]: entries 0/1 built in, flags for the two downloaded game slots
 * (category 7).
 */
void game_menu_state(u8 *p)
{
    int i;

    for (i = 0; i <= 5; i++)
        p[i] = 0xff;
    p[0] = 0;
    p[1] = 0;
    if (*(u32 *)item_bank_slot(7, p[0]) != 0xffffffff)
        p[4] = 2;
    if (*(u32 *)item_bank_slot(7, 1) != 0xffffffff)
        p[5] = 2;
}

/*
 * Draw the game menu page (built-in and downloaded games, cursor).
 */
u8 game_menu_draw(int a, int b, void *c)
{
    u8 fl[2];
    u8 *q;
    int n;
    int i, j;
    u8 z;
    u8 h;
    u8 v;

    fl[0] = 0;
    fl[1] = 0;
    ui_new_screen();
    i = 0;
    gfx_object_create(i, 0x1c, i, i, i, 0x2104794);
    b += *(u8 *)a * 2;
    j = 0;
    n = -1;
    z = 0;
    for (; i < 2; i++) {
        switch (*(u8 *)a) {
        case 0:
            h = gfx_object_create(0x18, 0x20, 0x30, z, z, z);
            gfx_draw_text_line(h, 0x18, j * 16 + 0x20, 0x30, game_menu_tbl[i].label, z, 8);
            j++;
            n++;
            fl[i] = 1;
            b++;
            break;
        case 1:
            b++;
            break;
        default:
            if (*(s8 *)b == 2) {
                q = item_bank_slot(7, i);
                if (*(int *)q != -1) {
                    h = gfx_object_create(0x18, 0x20, 0x30, z, z, z);
                    gfx_draw_text_line(h, 0x18, j * 16 + 0x20, 0x30, (int)(q + 0x5e), z, 8);
                    j++;
                    n++;
                    fl[i] = 1;
                }
            }
            b++;
            break;
        }
    }
    if (n < 0)
        return 0;
    if (*(u8 *)a == 0) {
        i = 0;
        j = -1;
        for (; i < 4; i++, b++)
            if (*(s8 *)b > 0)
                j = 0;
        if (j == 0)
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, 0x21059a4);
        if (fl[0] && fl[1])
            goto one;
        if (fl[0] == 0 && fl[1] == 1)
            v = 1;
        else
            v = 0;
    } else if (*(u8 *)a == 1) {
        i = 0;
        j = -1;
        for (; i < 2; i++, b++)
            if (*(s8 *)b > 0)
                j = 0;
        if (j == 0)
            gfx_object_create(0x72, 0x58, 0x40, 0, 0, 0x21059a4);
        else
            gfx_object_create(0x72, 0x58, 0x40, 1, 0, 0x21059a4);
        if (fl[0] && fl[1])
            goto one;
        if (fl[0] == 0 && fl[1] == 1)
            v = 3;
        else
            v = 2;
    } else {
        gfx_object_create(0x72, 0x58, 0x40, 1, 0, 0x21059a4);
        if (fl[0] && fl[1])
            goto one;
        if (fl[0] == 0 && fl[1] == 1)
            v = 5;
        else
            v = 4;
    }
    *(u8 *)c = v;
one:
    return 1;
}

u8 game_menu_next(int a0, int b0, void *c0)
{
    u8 *a = (u8 *)a0;
    s8 *b = (s8 *)b0;
    u8 *c = (u8 *)c0;
    u8 old = *a;
    u8 x = 0;
    u8 y = 0;
    int i;
    int k;
    s8 *q;

    k = *c;
    if (k > 4) {
        x = 0;
        y = 0;
    } else {
        q = b + k + 1;
        for (i = k + 1; i <= 5; i++, q++) {
            if (*q >= 0) {
                y = i;
                x = i / 2;
                break;
            }
        }
    }
    *c = y;
    *a = x;
    return x != old;
}

/*
 * THE GAME CORNER (ゲームコーナー of the Tama Depa, game center): a paged list
 * (game_menu_draw draws, game_menu_next moves) of the built-in mini games game_menu_tbl[0..3]
 * (x004 = the game's entry function, e.g. the reaction game game_cake, game_treasure
 * ...) followed by the DOWNLOADED games = the items of bank 7 (type 0x94 / category 2 /
 * sub-kind 1: item_bank_slot(7, i - 4), 2 slots of 32 KB at flash 0x2710000; slot
 * game_menu_cursor - 4). Choosing a downloaded game asks "いく? / no / delete" (dialog
 * D_255FA5C; delete rewrites its sector with flash_backup_sector / flash_restore_sector_except) and then -
 * the SECOND NATIVE-CODE LAUNCH SITE, same scheme as the town menu (F28 town_menu):
 *     dl_install_program(p);       install the item at 0x2000000 (unless already there)
 *     entry = 0x2000000 + (p[0xAE] << 24 | p[0xAF] << 16 | p[0xB0] << 8 | p[0xB1]);
 *     r = ((int (*)(void))entry)();      run it (game_session = {1, 0, 0} marks it)
 * i.e. NATIVE CODE from the downloaded file is executed in place from NOR flash; the
 * games call the firmware only through the API block 0x2400AD0..0x2401D92 (verified on
 * the 20 distinct games of the download pack, notes/findings/tamago.md). The result r
 * (0 lost, 1..5 ranks / score classes, or a raw point value > 5; the pack's "EZ_Gotchi"
 * file is a 6-byte program returning 9999) pays lvl x 20 / 60 / 100 / 140 / 200 Gotchi
 * points (anim_game_corner_points / pet_add_points; lvl = 2 during an outing, town_visit_active), weight loss
 * (pet_sub_weight), happiness (pet_add_happiness), the result / prize animations (anim_game_corner_win /
 * 2256 / 2418) and the play counter pet.game_count; rank 5 of a downloaded game also runs
 * dl_chara_affection_update(0x2000000) and applies the stat vector at 0x20000B8 of the installed app
 * (pet_add_traits). Battery is checked at every redraw. No integrity check of the code
 * happens here: the only checks on a downloaded game are those of the exchange record
 * that carried it (comm_validate_record) at reception.
 */
u8 game_menu(void)
{
    s8 buf[8];
    u8 st;
    u32 out;
    u8 *p;
    int dl;
    int r;
    int lvl;
    int k;
    int c, w;
    int i; int s; u8 *q;

    game_menu_state((u8 *)buf);
    game_menu_cursor = 0;
    st = 0;
redraw:
    if (sys_battery_low() == 1)
        sys_power_down();
    if (game_menu_draw((int)&st, (int)buf, &game_menu_cursor) == 0) {
        st++;
        st %= 3;
        goto redraw;
    }
    {
        int s2 = game_menu_cursor;
        k = s2 & 1;
        if (k != 0 && buf[s2 - 1] < 0)
            k = 0;
    }
    game_menu_cursor_obj = gfx_object_create(6, k * 16 + 0x22, 0x20, 0, 0, 0x2104714);
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            if (game_menu_next((int)&st, (int)buf, &game_menu_cursor) != 0)
                goto redraw;
            k = game_menu_cursor & 1;
            if (k != 0 && buf[game_menu_cursor - 1] < 0)
                k = 0;
            gfx_object_move_dirty(game_menu_cursor_obj, 6, k * 16 + 0x22);
            func_00009A6();
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            ui_timeout = 10;
            if (game_menu_tbl[game_menu_cursor].run != 0) {
                dl = 0;
                anim_game_corner_start();
                r = ((int (*)(void))game_menu_tbl[game_menu_cursor].run)();
            } else {
                if (game_menu_cursor > 3) {
                    dl = 1;
                    p = item_bank_slot(7, game_menu_cursor - 4);
                    if (*(u32 *)p == 0xffffffff)
                        continue;
                    out = 0;
                    switch ((u32)ui_use_no_delete((int)D_255FA5C, 0, &out)) {
                    case 2:
                        goto redraw;
                    case 1:
                        switch (out) {
                        case 0:
                            break;
                        case 1:
                            goto redraw;
                        case 2:
                            if (ui_confirm_really() != 0) {
                                snd_wait_end();
                                i = game_menu_cursor - 4; c = item_bank_tbl[7].first_sector; w = item_bank_tbl[7].slot_size; c += (u32)(w * i) >> 16;
                                flash_backup_sector(c);
                                flash_restore_sector_except(c, p, w);
                                buf[game_menu_cursor] = -1;
                                snd_wait_end();
                                save_game();
                                if (game_menu_next((int)&st, (int)buf, &game_menu_cursor) != 0)
                                    goto redraw;
                            } else if (ui_timeout == 0)
                                goto quit;
                            btn_b_presses = 0;
                            btn_c_presses = 0;
                            goto redraw;
                        }
                        break;
                    case 3:
                    quit:
                        town_place_aborted = 1;
                        return 1;
                    }
                }
                game_session.active = 1;
                game_session.x001 = 0;
                game_session.x002 = 0;
                dl_install_program(p);
                q = p; p = (u8 *)(u32)q[0xb1]; p += q[0xb0] << 8; p += q[0xaf] << 16; p += q[0xae] << 24; p += 0x2000000; anim_game_corner_start(); r = ((int (*)(void))p)();
            }
            if (r < 0)
                goto redraw;
            lvl = town_visit_active() ? 2 : 1;
            if (game_menu_tbl[game_menu_cursor].run == (u32)game_treasure) {
                switch (r) {
                case 0:
                    anim_game_corner_lose();
                    break;
                case 1:
                    pet_add_happiness(1);
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 20);
                    pet_add_points(lvl * 20);
                    if (btn_c_presses == 0)
                        anim_game_corner_win();
                    break;
                case 2:
                    pet_add_happiness(2);
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 140);
                    pet_add_points(lvl * 140);
                    if (btn_c_presses == 0)
                        anim_game_corner_win2();
                    break;
                case 3:
                    pet_add_happiness(3);
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 200);
                    pet_add_points(lvl * 200);
                    if (btn_c_presses == 0)
                        anim_game_corner_best();
                    evo_count_item_use_young(2, 0);
                    evo_count_game();
                    if (save.pet.game_count < 99)
                        save.pet.game_count++;
                    break;
                }
            } else {
                switch (r) {
                case 0:
                    anim_game_corner_lose();
                    break;
                case 1:
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 20);
                    pet_add_points(lvl * 20);
                    if (btn_c_presses == 0)
                        anim_game_corner_win();
                    pet_add_happiness(1);
                    break;
                case 2:
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 60);
                    pet_add_points(lvl * 60);
                    if (btn_c_presses == 0)
                        anim_game_corner_win();
                    pet_add_happiness(1);
                    break;
                case 3:
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 100);
                    pet_add_points(lvl * 100);
                    if (btn_c_presses == 0)
                        anim_game_corner_win();
                    if (game_menu_tbl[game_menu_cursor].run == (u32)game_cake)
                        pet_add_happiness(2);
                    else
                        pet_add_happiness(1);
                    break;
                case 4:
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 140);
                    pet_add_points(lvl * 140);
                    if (btn_c_presses == 0)
                        anim_game_corner_win2();
                    pet_add_happiness(2);
                    break;
                case 5:
                    pet_sub_weight(2);
                    anim_game_corner_points(lvl * 200);
                    pet_add_points(lvl * 200);
                    if (btn_c_presses == 0)
                        anim_game_corner_best();
                    pet_add_happiness(3);
                    evo_count_item_use_young(2, 0);
                    evo_count_game();
                    if (dl != 0) {
                        dl_chara_affection_update(0x2000000);
                        pet_add_traits((u8 *)0x20000b8);
                    }
                    if (save.pet.game_count < 99)
                        save.pet.game_count++;
                    break;
                default:
                    pet_sub_weight(2);
                    anim_game_corner_points(r);
                    pet_add_points(r);
                    if (r <= 0x78) {
                        if (btn_c_presses == 0)
                            anim_game_corner_win();
                        pet_add_happiness(1);
                    } else if (r <= 0xa0) {
                        if (btn_c_presses == 0)
                            anim_game_corner_win2();
                        pet_add_happiness(2);
                    } else {
                        if (btn_c_presses == 0)
                            anim_game_corner_best();
                        pet_add_happiness(3);
                    }
                    break;
                }
            }
            snd_wait_end();
            save_game();
            goto redraw;
        }
        if (btn_c_presses != 0) {
            btn_c_presses = 0;
            ui_timeout = 10;
            break;
        }
        sys_idle_tick();
        if (ui_timeout == 0) {
            sys_idle_seconds = 10;
            break;
        }
    }
    if (ui_timeout != 0) {
        ui_timeout = 10;
        sys_idle_seconds = 0;
    }
    return 0;
}

/*
 * The RAM variables this file defines.
 */
struct GameSession game_session;
u8 game_menu_cursor DSTRAM;
u8 game_menu_cursor_obj DSTRAM;
