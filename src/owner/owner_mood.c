/*
 * The owner's mood setting screen.
 *
 * Code 25051F0..25051F0, .rodata from 253C400.
 */
#include "proto/owner/owner_mood.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/room/room_main.h"
#include "proto/ui/ui_menu.h"
#include "proto/save/save_flash.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/*
 * Profile mood setting: choose today's mood for the profile (さいこー! / いいかんじ♪ /
 * やるきまんまん / ふつう / いまいち / しょんぼり, 6 entries with pictures), then
 * "プロフィールの きょうのきぶんを とうろくしたよ!" (registered).
 */
void owner_mood_setting(void)
{
    u8 *txt[9];
    u8 *img[9];
    u8 z;
    int mode = 0;
    u8 k = 0;
    int redraw;
    u8 t;
    u32 n = 6;
    void (*draw)(u8, s16, u8, u8 *, u8, u8);

    memcpy(txt, owner_mood_labels, 0x24);
    memcpy(img, owner_mood_pics, 0x24);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    redraw = 1;
    z = 0;
    do {
        if (redraw) {
            ui_new_screen();
            if (mode == 0) {
                gfx_object_create(0, 0x1c, 0, 0, 0, D_2104794);
                gfx_object_create(0x40 - (img[k][0] >> 1), 0x39 - (img[k][1] >> 1), 0x30, 0, 0, (const u8 *)img[k]);
                t = gfx_object_create(0, 0x38, 0x90, 0, 0, 0);
                draw = gfx_draw_text_centered;
                draw(t, 0x4d, 0x90, txt[k], 0, 7);
                redraw = 0;
                func_00009A6();
            } else {
                gfx_object_create(z, 0x1c, z, z, z, D_2104794);
                t = gfx_object_create(z, z, 0x90, z, z, (const u8 *)z);
                gfx_draw_text_centered(t, 0x29, 0x90, txt[6], z, 8);
                draw = gfx_draw_text_centered;
                t = gfx_object_create(z, z, 0x92, z, z, (const u8 *)z);
                draw(t, 0x39, 0x92, txt[7], z, 8);
                t = gfx_object_create(z, z, 0x94, z, z, (const u8 *)z);
                draw(t, 0x49, 0x94, txt[8], z, 8);
                redraw = z;
                func_00009A6();
                save.pet.profile_mood = k + 1;
                save_game();
            }
        }
        if (btn_a_presses != 0) {
            if (mode)
                break;
            redraw = 1;
            k++;
            if (k >= n)
                k = 0;
            btn_a_presses = z;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            if (mode)
                break;
            mode = 1;
            redraw = 1;
            btn_b_presses = z;
            ui_timeout = 10;
        }
        if (btn_c_presses != 0)
            break;
        sys_idle_tick();
    } while (ui_timeout != 0);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
}

const u8 D_253C404[] = _("さいこー!\0");
const u8 D_253C414[] = _("いいかんじ♪");
const u8 D_253C424[] = _("やるきまんまん\0");
const u8 D_253C438[] = _("ふつう\0");
const u8 D_253C444[] = _("いまいち");
const u8 D_253C450[] = _("しょんぼり\0");
const u8 D_253C460[] = _("プロフィールの\0");
const u8 D_253C474[] = _("きょうのきぶんを");
const u8 D_253C488[] = _("とうろくしたよ!");
const u8 *const owner_mood_labels[9] = {
    D_253C404, D_253C414, D_253C424, D_253C438, D_253C444, D_253C450,
    D_253C460, D_253C474, D_253C488,
};
extern u8 D_20F37D0[];
extern u8 D_20F3A68[];
extern u8 D_20F3D00[];
extern u8 D_20F3F98[];
extern u8 D_20F4230[];
extern u8 D_20F29F0[];
const u8 *const owner_mood_pics[9] = {
    D_20F37D0, D_20F3A68, D_20F3D00, D_20F3F98, D_20F4230, D_20F29F0, 0, 0, 0,
};
