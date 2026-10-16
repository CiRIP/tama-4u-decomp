/*
 * The clock screen: drawing the time with digit pictures, blinking, day clamping, the clock
 * setting and the LCD brightness setting.
 *
 * Code 241BA86..241C3D2, .rodata from 255F718.
 */
#include "proto/rtc/rtc_clock_screen.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_window.h"
#include "proto/sys/sys_boot.h"
#include "proto/comm/comm_menu.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/ui/ui_menu.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "flash.h"
#include "hw.h"

/*
 * Clock display: update the digit objects for month a, day b, hour c, minute d (e = ?)
 * selected by mask f (digit pictures rtc_clock_small_digits).
 */
void rtc_clock_draw(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f)
{
    if (f & 1) {
        if (a > 9)
            gfx_object_set_image(rtc_clock_month_tens_obj, 0x30, 0, D_2128064);
        else
            gfx_object_set_image(rtc_clock_month_tens_obj, 0x30, 0, 0);
        gfx_object_set_image(rtc_clock_month_ones_obj, 0x30, 0, rtc_clock_small_digits[a % 10]);
    }
    if (f & 2) {
        if (b > 9)
            gfx_object_set_image(rtc_clock_day_tens_obj, 0x30, 0, rtc_clock_small_digits[b / 10]);
        else
            gfx_object_set_image(rtc_clock_day_tens_obj, 0x30, 0, 0);
        gfx_object_set_image(rtc_clock_day_ones_obj, 0x30, 0, rtc_clock_small_digits[b % 10]);
    }
    if (f & 4) {
        if (rtc_clock_blink != 0) {
            gfx_object_set_image(rtc_clock_ampm_obj, 0x40, 0, 0);
        } else {
            u8 h = c / 12;
            c = c % 12;
            if (c == 0)
                c = 12;
            gfx_object_set_image(rtc_clock_ampm_obj, 0x40, h, D_212826C);
        }
        if (c > 9)
            gfx_object_set_image(rtc_clock_hour_tens_obj, 0x50, 0, rtc_clock_big_digits[c / 10]);
        else
            gfx_object_set_image(rtc_clock_hour_tens_obj, 0x50, 0, 0);
        gfx_object_set_image(rtc_clock_hour_ones_obj, 0x50, 0, rtc_clock_big_digits[c % 10]);
    }
    if (f & 8) {
        gfx_object_set_image(rtc_clock_min_tens_obj, 0x50, 0, rtc_clock_big_digits[d / 10]);
        gfx_object_set_image(rtc_clock_min_ones_obj, 0x50, 0, rtc_clock_big_digits[d % 10]);
    }
    if (f & 0x10) {
        gfx_object_set_image(rtc_clock_sec_tens_obj, 0x30, 0, rtc_clock_small_digits[e / 10]);
        gfx_object_set_image(rtc_clock_sec_ones_obj, 0x30, 0, rtc_clock_small_digits[e % 10]);
    } else {
        gfx_object_set_image(rtc_clock_sec_tens_obj, 0x30, 0, 0);
        gfx_object_set_image(rtc_clock_sec_ones_obj, 0x30, 0, 0);
    }
}

/*
 * Clock display: show the current date and time (all fields).
 */
void rtc_clock_show_now(void)
{
    u8 a, b, c, d, e;

    a = rtc_get_month();
    b = rtc_get_day();
    c = rtc_get_hour();
    d = rtc_get_minute();
    e = rtc_get_second();
    rtc_clock_draw(a, b, c, d, e, 0x1f);
}

/*
 * Toggle rtc_clock_blink (blink state of the clock setting cursor).
 */
void rtc_clock_blink_toggle(void)
{
    rtc_clock_blink = rtc_clock_blink == 0;
}

/* the clock digit pictures 0..9: small (rtc_clock_small_digits) and large (rtc_clock_big_digits) */
extern u8 D_2128238[];
extern u8 D_2128098[];
extern u8 D_21280CC[];
extern u8 D_2128100[];
extern u8 D_2128134[];
extern u8 D_2128168[];
extern u8 D_212819C[];
extern u8 D_21281D0[];
extern u8 D_2128204[];
const u8 *const rtc_clock_small_digits[10] = {
    D_2128238, D_2128064,
    D_2128098, D_21280CC,
    D_2128100, D_2128134,
    D_2128168, D_212819C,
    D_21281D0, D_2128204,
};
extern u8 D_2128020[];
extern u8 D_2127DBC[];
extern u8 D_2127E00[];
extern u8 D_2127E44[];
extern u8 D_2127E88[];
extern u8 D_2127ECC[];
extern u8 D_2127F10[];
extern u8 D_2127F54[];
extern u8 D_2127F98[];
extern u8 D_2127FDC[];
const u8 *const rtc_clock_big_digits[10] = {
    D_2128020, D_2127DBC,
    D_2127E00, D_2127E44,
    D_2127E88, D_2127ECC,
    D_2127F10, D_2127F54,
    D_2127F98, D_2127FDC,
};
/* unreferenced picture (42 x 28) */
const u8 D_255F76C[] = {
    0x2a, 0x1c, 0x08, 0x00, 0x01, 0xff, 0x07, 0xe0, 0x3e, 0xbf, 0x9f, 0xbf, 0x60, 0x00, 0x4d, 0x3a,
    0x3b, 0xd4, 0x4c, 0xea, 0x99, 0xc7, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x02,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x32, 0x33, 0x33, 0x33, 0x33, 0x31, 0x33, 0x33, 0x33, 0x33, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43, 0x44, 0x44, 0x44, 0x44, 0x53,
    0x44, 0x44, 0x44, 0x44, 0x13, 0x22, 0x22, 0x22, 0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43,
    0x44, 0x44, 0x44, 0x44, 0x53, 0x44, 0x44, 0x44, 0x54, 0x13, 0x22, 0x22, 0x22, 0x22, 0x02, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x43, 0x54, 0x55, 0x55, 0x44, 0x53, 0x54, 0x55, 0x55, 0x54, 0x13, 0x22,
    0x22, 0x22, 0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45,
    0x44, 0x44, 0x55, 0x13, 0x22, 0x22, 0x22, 0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43, 0x45,
    0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x22, 0x22, 0x22, 0x02, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x22,
    0x22, 0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43, 0x55, 0x55, 0x55, 0x45, 0x53, 0x55, 0x55,
    0x55, 0x55, 0x13, 0x22, 0x22, 0x22, 0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43, 0x45, 0x44,
    0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x22, 0x22, 0x22, 0x02, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x22, 0x22,
    0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44,
    0x55, 0x13, 0x22, 0x22, 0x22, 0x22, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x43, 0x55, 0x55, 0x55,
    0x45, 0x53, 0x55, 0x55, 0x55, 0x55, 0x13, 0x22, 0x22, 0x22, 0x33, 0x03, 0x33, 0x23, 0x22, 0x22,
    0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x22, 0x32, 0x66,
    0x06, 0x66, 0x36, 0x21, 0x22, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55,
    0x13, 0x22, 0x22, 0x32, 0x66, 0x06, 0x66, 0x36, 0x21, 0x22, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45,
    0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x32, 0x63, 0x66, 0x06, 0x66, 0x66, 0x33, 0x21, 0x22,
    0x43, 0x55, 0x55, 0x55, 0x45, 0x53, 0x55, 0x55, 0x55, 0x55, 0x13, 0x22, 0x63, 0x66, 0x66, 0x06,
    0x66, 0x66, 0x66, 0x13, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13,
    0x22, 0x63, 0x66, 0x66, 0x06, 0x66, 0x66, 0x66, 0x13, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53,
    0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x63, 0x66, 0x66, 0x06, 0x66, 0x66, 0x66, 0x13, 0x22, 0x43,
    0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22, 0x32, 0x63, 0x66, 0x06, 0x66,
    0x66, 0x33, 0x21, 0x22, 0x43, 0x45, 0x44, 0x44, 0x45, 0x53, 0x45, 0x44, 0x44, 0x55, 0x13, 0x22,
    0x32, 0x66, 0x66, 0x06, 0x66, 0x66, 0x36, 0x21, 0x22, 0x43, 0x54, 0x55, 0x55, 0x44, 0x53, 0x54,
    0x55, 0x55, 0x54, 0x13, 0x22, 0x32, 0x66, 0x66, 0x06, 0x66, 0x66, 0x36, 0x21, 0x22, 0x53, 0x55,
    0x55, 0x55, 0x55, 0x53, 0x55, 0x55, 0x55, 0x55, 0x13, 0x22, 0x32, 0x66, 0x36, 0x06, 0x36, 0x66,
    0x36, 0x21, 0x22, 0x32, 0x33, 0x33, 0x33, 0x33, 0x31, 0x33, 0x33, 0x33, 0x33, 0x21, 0x22, 0x22,
    0x33, 0x13, 0x03, 0x13, 0x33, 0x13, 0x22, 0x22, 0x12, 0x11, 0x31, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x13, 0x11, 0x22, 0x22, 0x22, 0x31, 0x21, 0x01, 0x11, 0x31, 0x37, 0x22, 0x22, 0x22, 0x11, 0x73,
    0x23, 0x22, 0x22, 0x12, 0x31, 0x37, 0x22, 0x22, 0x22, 0x11, 0x73, 0x23, 0x02, 0x31, 0x33, 0x33,
    0x33, 0x22, 0x11, 0x33, 0x33, 0x33, 0x23, 0x12, 0x31, 0x33, 0x33, 0x33, 0x22, 0x11, 0x33, 0x33,
    0x33, 0x03, 0x00, 0x00,
};

/*
 * Clamp day v for month k (0 -> last day: 29 for February, 30 for April/June/September/
 * November, 31 otherwise).
 */
u8 rtc_clamp_day(u8 k, u8 v)
{
    switch (k) {
    case 2:
        if (v == 0)
            v = 0x1d;
        break;
    case 4:
    case 6:
    case 9:
    case 11:
        if (v == 0)
            v = 0x1e;
        break;
    default:
        if (v == 0)
            v = 0x1f;
        break;
    }
    return v;
}

void rtc_clock_setting(int mode)
{
    u8 cnt;
    int flag;
    u8 v5;
    u8 hour;
    u8 min;
    int beep;
    u8 tog;
    u8 month, day;
    s8 field;
    u8 c;

    cnt = 0;
    tog = 0;
    pet_life_paused = 1;
    month = rtc_get_month();
    day = rtc_get_day();
    hour = rtc_get_hour();
    min = rtc_get_minute();
    tmr_t5_start_timeout(500);
    rtc_clock_draw(month, day, hour, min, cnt, 0xf);
    v5 = cnt;
    flag = (P0_P0D & 4) == 0;
    if (mode == 2)
        field = 3;
    else
        field = 0;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    beep = 0;
    for (;;) {
        if (sys_display_state != 0) {
            T16_CTL0 = 0;
            T16_CTL1 = 0;
            T16_CTL3 = 0;
            T16_CTL5 = 0;
            sys_display_off();
            while (sys_display_state != 0)
                asm volatile("slp");
            if (sys_battery_check_due != 0) {
                sys_battery_low();
                sys_battery_check_due = 0;
            }
            if (sys_battery_level <= 0x2e3)
                sys_power_down();
            tmr_t5_start_timeout(500);
        } else
            sys_idle_tick();
        if ((s8)save.pet.calls < 0 && save.pet.life_stage == 0)
            break;
        if (flag != 0 && (P0_P0D & 4))
            flag = 0;
        if (tmr_flags & 0x20) {
            cnt++;
            tmr_flags &= ~0x20;
            if (!(P0_P0D & 1)) {
                v5++;
                if (v5 > 1) {
                    if (beep == 0) {
                        tmr_t5_start_timeout(0x4b);
                        beep = 1;
                    }
                    btn_a_presses++;
                    sys_idle_seconds = 0;
                    v5 = 1;
                    snd_play(1);
                }
            } else {
                v5 = 0;
                if (beep != 0) {
                    beep = 0;
                    tmr_t5_start_timeout(500);
                }
            }
        }
        if (btn_a_presses != 0) {
            cnt = 0;
            if (beep == 0) {
                tog = 0;
                T16_CTL5 = 2;
                T16_CTL5 = 1;
            }
            if (P0_P0D & 4) {
                switch (field) {
                case 0:
                    month++;
                    if (month > 12)
                        month = 1;
                    day = rtc_next_day_leap(month, day);
                    break;
                case 1:
                    day++;
                    day = rtc_next_day_leap(month, day);
                    break;
                case 2:
                    hour++;
                    if (hour > 23)
                        hour = 0;
                    break;
                case 3:
                    min++;
                    if (min > 59)
                        min = 0;
                    break;
                }
            } else {
                switch (field) {
                case 0:
                    month--;
                    if (month == 0)
                        month = 12;
                    day = rtc_next_day_leap(month, day);
                    break;
                case 1:
                    day--;
                    day = rtc_clamp_day(month, day);
                    break;
                case 2:
                    hour--;
                    if (hour == 0xff)
                        hour = 23;
                    break;
                case 3:
                    min--;
                    if (min == 0xff)
                        min = 59;
                    break;
                }
            }
            btn_a_presses = 0;
        }
        if (btn_b_presses != 0) {
            tog = 0;
            rtc_clock_draw(month, day, hour, min, 0, 1 << field);
            field++;
            if (field > 3)
                break;
            btn_b_presses = 0;
        }
        if (btn_c_presses != 0) {
            tog++;
            if (!(tog & 1)) {
                rtc_clock_draw(month, day, hour, min, 0, 1 << field);
                if (flag == 0) {
                    field--;
                    if (field < 0) {
                        if (mode == 0)
                            break;
                        field = 0;
                    }
                }
            }
            btn_c_presses = 0;
        }
        rtc_clock_draw(month, day, hour, min, 0, 1 << field);
        if (cnt & 1) {
            switch (field) {
            case 0:
                gfx_object_set_image(rtc_clock_month_tens_obj, 0x30, 0, 0);
                gfx_object_set_image(rtc_clock_month_ones_obj, 0x30, 0, 0);
                break;
            case 1:
                gfx_object_set_image(rtc_clock_day_tens_obj, 0x30, 0, 0);
                gfx_object_set_image(rtc_clock_day_ones_obj, 0x30, 0, 0);
                break;
            case 2:
                gfx_object_set_image(rtc_clock_hour_tens_obj, 0x50, 0, 0);
                gfx_object_set_image(rtc_clock_hour_ones_obj, 0x50, 0, 0);
                break;
            case 3:
                gfx_object_set_image(rtc_clock_min_tens_obj, 0x50, 0, 0);
                gfx_object_set_image(rtc_clock_min_ones_obj, 0x50, 0, 0);
                break;
            }
        }
        func_00009A6();
    }
    c = 1;
    if (rtc_get_month() == month && rtc_get_day() == day) {
        c = 0;
        if (mode != 0)
            c = 1;
    }
    rtc_set_time(month, day, hour, min);
    if (c != 0)
        room_weather_draw_daily();
    room_weather_update_now();
    if (mode == 0)
        pet_life_paused = 0;
    if (c != 0) {
        pet_set_unlock_flag(0x43, 0);
        pet_set_unlock_flag(0x42, 0);
    }
    if (save.pet.window_flower & 1) {
        switch (month) {
        case 3: case 4: case 5:
            break;
        default:
            save.pet.window_flower &= ~1;
        }
    }
    if (save.pet.window_flower & 2) {
        switch (month) {
        case 6: case 7: case 8:
            break;
        default:
            save.pet.window_flower &= ~2;
        }
    }
    if (save.pet.window_flower & 4) {
        switch (month) {
        case 9: case 10: case 11:
            break;
        default:
            save.pet.window_flower &= ~4;
        }
    }
    if (save.pet.window_flower & 8) {
        switch (month) {
        case 1: case 2: case 12:
            break;
        default:
            save.pet.window_flower &= ~8;
        }
    }
    if (c != 0) {
        pet_clear_call(0x10);
        pet_clear_call(0x40);
    }
}

/*
 * Create the clock screen objects (background 0x21283EC, digit objects rtc_clock_ampm_obj..D_00842D4).
 */
void rtc_clock_create(void)
{
    gfx_object_create(0, 0x1c, 0, 0, 0, D_21283EC);
    rtc_clock_month_tens_obj = gfx_object_create(0xe, 0x27, 0x10, 0, 0, 0);
    rtc_clock_month_ones_obj = gfx_object_create(0x16, 0x27, 0x10, 0, 0, 0);
    rtc_clock_day_tens_obj = gfx_object_create(0x22, 0x27, 0x10, 0, 0, 0);
    rtc_clock_day_ones_obj = gfx_object_create(0x2a, 0x27, 0x10, 0, 0, 0);
    rtc_clock_ampm_obj = gfx_object_create(0xa, 0x34, 0x20, 0, 0, 0);
    rtc_clock_hour_tens_obj = gfx_object_create(0xa, 0x40, 0x30, 0, 0, 0);
    rtc_clock_hour_ones_obj = gfx_object_create(0x14, 0x40, 0x30, 0, 0, 0);
    rtc_clock_min_tens_obj = gfx_object_create(0x22, 0x40, 0x30, 0, 0, 0);
    rtc_clock_min_ones_obj = gfx_object_create(0x2c, 0x40, 0x30, 0, 0, 0);
    rtc_clock_sec_tens_obj = gfx_object_create(0x37, 0x44, 0x40, 0, 0, 0);
    rtc_clock_sec_ones_obj = gfx_object_create(0x3f, 0x44, 0x40, 0, 0, 0);
}

/*
 * Brightness setting screen (あかるさせってい, from the settings menu via ui_settings_brightness, and
 * from the factory test mode test_mode): picture 0x212DEA4 with 7 bar sprites
 * (0x212DE78, lit up to the level), A = darker, B = brighter (sys_backlight_level, 1..7, applied at
 * once with sys_set_backlight), A+B held = a hidden picture 0x2134B38 (a character saying
 * "くろまめセンパイみたいになるのがユメなんだよねー★") until a button or the 10 s timeout,
 * C = done (returns 0). Leaves at once when pet.calls bit 7 is set with no
 * pet (pet.life_stage == 0); the 10 s inactivity timeout returns -1. Not a clock screen (the
 * earlier comment said so).
 */
int lcd_brightness_setting(void)
{
    u8 buf[7];
    int i;
    int k;
    int r = 0;
    const u8 *p;

again:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_212DEA4);
    p = D_212DE78;
    for (i = 0; i <= 6; i++) {
        buf[i] = gfx_object_create(i * 9 - (p[0] >> 1) + 0x19, 0x2f - (p[1] >> 1), 0x30, 0, 0, (const u8 *)p);
        if (sys_backlight_level <= i)
            gfx_object_set_image(buf[i], 0x30, 0, 0);
    }
    func_00009A6();
    ui_timeout = 10;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    do {
        if (btn_a_presses != 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            if (--sys_backlight_level == 0)
                sys_backlight_level = 1;
            ui_timeout = 10;
        }
        if (btn_b_presses != 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            if (++sys_backlight_level > 7)
                sys_backlight_level = 7;
            ui_timeout = 10;
        }
        k = P0_P0D & 3;
        if (k == 0) {
            ui_new_screen();
            gfx_object_create(0, 0x1c, 0, 0, 0, D_2134B38);
            func_00009A6();
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            ui_timeout = 10;
            for (;;) {
                if ((u8)(btn_a_presses | btn_b_presses | btn_c_presses))
                    break;
                sys_idle_tick();
                if (ui_timeout == 0)
                    goto again;
            }
            ui_timeout = 10;
            goto again;
        }
        if (btn_c_presses != 0) {
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            goto out;
        }
        if ((signed char)save.pet.calls < 0 && save.pet.life_stage == 0)
            goto out;
        for (i = 0; i <= 6; i++) {
            if (sys_backlight_level <= i)
                gfx_object_set_image(buf[i], 0x30, 0, 0);
            else
                gfx_object_set_image(buf[i], 0x30, 0, D_212DE78);
        }
        sys_set_backlight(sys_backlight_level);
        func_00009A6();
        sys_idle_tick();
    } while (ui_timeout != 0);
    r = -1;
out:
    return r;
}

/*
 * The RAM variables this file defines (external RAM: this object's .bss / .data,
 * in this order).
 */
u8 rtc_clock_blink DSTRAM;
u8 rtc_clock_ampm_obj DSTRAM;
u8 rtc_clock_month_tens_obj DSTRAM;
u8 rtc_clock_month_ones_obj DSTRAM;
u8 rtc_clock_day_tens_obj DSTRAM;
u8 rtc_clock_day_ones_obj DSTRAM;
u8 rtc_clock_hour_tens_obj DSTRAM;
u8 rtc_clock_hour_ones_obj DSTRAM;
u8 rtc_clock_min_tens_obj DSTRAM;
u8 rtc_clock_min_ones_obj DSTRAM;
u8 rtc_clock_sec_tens_obj DSTRAM;
u8 rtc_clock_sec_ones_obj DSTRAM;
