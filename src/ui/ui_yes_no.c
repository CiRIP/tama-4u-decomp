/*
 * The yes/no question screen used by the departure, downloaded-characters and spot screens.
 *
 * Code 2513E1C..2513E1C, .rodata from 255D304.
 */
#include "proto/ui/ui_yes_no.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/room/room_main.h"
#include "proto/ui/ui_menu.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"

/*
 * Yes / no question with one of three texts a (D_255D308..): 0 "さがしにいく?" (go and search,
 * the pet-gone screen), 1 "ほんとうに?" (really?: also the confirmations of the download slot
 * replacement, the spot reader and the album slot delete), 2 "みおくる?" (see off); cursor b.
 */
int ui_yes_no_question(u8 a, u8 b)
{
    u8 c;
    u8 k;

    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
    c = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    if (a == 0)
        gfx_draw_text_centered(c, 0x20, 0x10, D_255D308, a, 9);
    else if (a == 1)
        gfx_draw_text_centered(c, 0x20, 0x10, D_255D318, 0, 9);
    else
        gfx_draw_text_centered(c, 0x20, 0x10, D_255D328, 0, 9);
    c = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(c, 0x18, 0x40, 0x10, (int)D_255D334, 0, 8);
    c = gfx_object_create(0x18, 0x20, 0x10, 0, 0, 0);
    gfx_draw_text_line(c, 0x18, 0x50, 0x10, (int)D_255D33C, 0, 8);
    k = gfx_object_create(6, b * 16 + 0x42, 0x20, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            ui_timeout = 10;
            b++;
            if (b > 1)
                b = 0;
            gfx_object_move_dirty(k, 6, b * 16 + 0x42);
            func_00009A6();
            btn_a_presses = 0;
        }
        if (btn_b_presses != 0) {
            btn_b_presses = 0;
            if (b == 0)
                return b;
            return 1;
        }
        if (btn_c_presses != 0) {
            btn_c_presses = 0;
            return 2;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    return 3;
}

const u8 D_255D308[] = _("さがしにいく?");
const u8 D_255D318[] = _("ほんとうに?");
const u8 D_255D328[] = _("みおくる?");
const u8 D_255D334[] = _("はい");
const u8 D_255D33C[] = _("いいえ");
