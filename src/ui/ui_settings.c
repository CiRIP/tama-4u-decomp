/*
 * The settings menu: theme, brightness, sound, and the clock (blocked during visits).
 *
 * Code 25133C4..25136CC, .rodata from 255D23C.
 */
#include "proto/ui/ui_settings.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/rtc/rtc_clock_screen.h"
#include "proto/comm/comm_menu.h"
#include "proto/ui/ui_menu.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "flash.h"
#include "libc.h"

/* F48 = the SETTINGS menu (せってい): じかんせってい clock (ui_settings_blocked -> the clock
 * editor rtc_clock_create / rtc_clock_setting; greyed while an NFC session is pending,
 * visit_active), アイコンバック (the icon-bar theme: ui_settings_theme picks among the
 * unlocked themes into pet.icon_theme), あかるさせってい brightness (lcd_brightness_setting),
 * サウンドせってい sound on/off (ui_sound_menu). */
/*
 * Settings menu (じかんせってい clock, アイコンバック icon background/theme,
 * あかるさせってい brightness, サウンドせってい sound).
 */
int ui_settings_menu(void)
{
    u32 t[4];
    int r = -1;
    u8 sel;
    u8 a;
    u8 k;
    u8 i;

    memcpy(t, ui_settings_labels, 0x10);
    sel = 0;
again:
    ui_new_screen();
    {
        int z = 0;
        gfx_object_create(z, 0x1c, z, z, z, D_2104794);
        a = gfx_object_create(6, sel * 16 + 0x22, 0x20, z, z, D_2104714);
    }
    for (i = 0; i <= 3; i++) {
        if (i == 0 && visit_active() != 0) {
            k = gfx_object_create(0x18, 0x20, 0x34, 0, 3, 0);
            gfx_draw_text_line(k, 0x18, i * 16 + 0x20, 0x34, t[i], 0, 8);
            continue;
        }
        k = gfx_object_create(0, 0, 0, 0, 0, 0);
        gfx_draw_text_line(k, 0x18, i * 16 + 0x20, 0x30, t[i], 0, 8);
    }
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            sel = sel > 2 ? 0 : sel + 1;
            gfx_object_move_dirty(a, 6, sel * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            switch (sel) {
            case 0:
                r = ui_settings_blocked();
                break;
            case 1:
                r = ui_settings_theme();
                break;
            case 2:
                r = ui_settings_brightness();
                break;
            case 3:
                r = ui_settings_sound();
                break;
            }
            if (r < 0)
                goto out;
            goto again;
        }
        if (btn_c_presses != 0) {
            r = 0;
            goto out;
        }
        sys_idle_tick();
    } while (ui_timeout != 0);
    r = -1;
out:
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return r;
}

/*
 * Settings entry blocked during a visit mode (no-sound).
 */
int ui_settings_blocked(void)
{
    if (visit_active()) {
        snd_play(12);
        snd_wait_end();
        return 0;
    }
    ui_new_screen();
    rtc_clock_create();
    rtc_clock_setting(1);
    pet_life_paused = 0;
    return 0;
}

/*
 * Icon-bar theme choice (pet.x0C5).
 */
int ui_settings_theme(void)
{
    int r = 0;
    u8 sel = 0;
    u8 h;

    if (ui_theme_count() == 0)
        return;
    ui_theme_draw_list();
    h = gfx_object_create(6, 0x22, 0x60, 0, 0, D_2104714);
    func_00009A6();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    ui_timeout = 10;
    do {
        if (btn_a_presses != 0) {
            sel = sel >= ui_theme_count() - 1 ? 0 : sel + 1;
            gfx_object_move_dirty(h, 6, sel * 16 + 0x22);
            func_00009A6();
            btn_a_presses = 0;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            save.pet.icon_theme = ui_theme_nth(sel);
            r = 1;
            goto out;
        }
        if (btn_c_presses != 0)
            goto out;
        sys_idle_tick();
    } while (ui_timeout != 0);
    r = -1;
out:
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    return r;
}

/*
 * Brightness setting screen (clear the screen, lcd_brightness_setting).
 */
int ui_settings_brightness(void)
{
    ui_new_screen();
    return lcd_brightness_setting();
}

/*
 * Sound setting screen.
 */
int ui_settings_sound(void)
{
    ui_new_screen();
    return ui_sound_menu();
}

const u8 D_255D240[] = _("じかんせってい");
const u8 D_255D250[] = _("アイコンバック");
const u8 D_255D260[] = _("あかるさせってい");
const u8 D_255D274[] = _("サウンドせってい");
const u8 *const ui_settings_labels[4] = { D_255D240, D_255D250, D_255D260, D_255D274 };
