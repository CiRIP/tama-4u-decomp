/*
 * The start-up flow after power-on: first-boot set-up (clock, birthday, name), new game or
 * continue.
 *
 * Code 250DF58..250DF58, .rodata from 255C7FC. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/sys/sys_startup.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/room/room_window.h"
#include "proto/rtc/rtc_clock_screen.h"
#include "proto/sys/sys_boot.h"
#include "proto/evo/evo_special.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_session.h"
#include "proto/txt/txt_input.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/dl/dl_chara.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/test/test_debug_chara.h"
#include "proto/owner/owner_profile.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/comm/comm_outcome.h"
#include "flash.h"
#include "libc.h"
#include "hw.h"




int sys_startup_flow(int a)
{
    u8 unused[4];       /* 4 bytes of the frame no code uses (another local of the original) */
    u8 name[16];
    int repeating;
    s8 field;
    int blink;
    u8 c_presses;
    u8 dl_chara_idx;
    int mode;
    int i;
    u8 chara_obj;
    u8 chara_idx;
    u8 *p;
    int n;

    chara_idx = 0;
    blink = 0;
    dl_chara_idx = 0;
    memset(name, 0, 16);
    pet_life_paused = 1;
    pet_new_game_init();
    sys_set_backlight(sys_backlight_level);
    if (sys_power_on() != 0) {
        srand(T16_TC2);
        return 1;
    }
    ui_show_startup_picture();
    nfc_power_off();
    save_factory_reset();
    INT_F16T45 = 0x40;
    INT_E16T45 = 0x44;
    /* show the start-up picture for 500 ms */
    tmr_flags = 0;
    tmr_t5_start_timeout(500);
    snd_enabled = 1;
    do
        asm volatile("halt");
    while ((tmr_flags & 0x20) == 0);
    tmr_flags &= ~0x20;
    anim_busy = 0;
    rtc_reset();
    INT_EP47_ERTC_EAD = 6;
    mode = 1;
    rtc_set_time(1, 1, 0, 0);
    rtc_irq_on();
    /* set the clock (rtc_clock_setting mode 1; mode 2 when C goes back from the birthday) */
clock:
    ui_new_screen();
    rtc_clock_create();
    rtc_clock_setting(mode);
    field = 0;
    srand(T16_TC2);
    /* the owner's birthday: month (field 0) and day (field 1) as two-digit objects, the field
     * being set blinking; A (held: repeats) steps it up, down when P0 bit 2 reads 0; B goes to
     * the next field, every second C back */
birthday:
    ui_new_screen();
    gfx_object_create(0, 0x1c, 0, 0, 0, D_212BA50);
    if (save.pet.owner_bday_month > 9)
        sys_obj_ids[0] = gfx_object_create(0x26, 0x50, 0x20, 1, 0, D_208EDB0);
    else
        sys_obj_ids[0] = gfx_object_create(0x26, 0x50, 0x20, 0, 0, 0);
    sys_obj_ids[1] = gfx_object_create(0x30, 0x50, 0x20, save.pet.owner_bday_month % 10, 0, D_208EDB0);
    if (save.pet.owner_bday_day > 9)
        sys_obj_ids[2] = gfx_object_create(0x48, 0x50, 0x20, save.pet.owner_bday_day / 10, 0, D_208EDB0);
    else
        sys_obj_ids[2] = gfx_object_create(0x48, 0x50, 0x20, 0, 0, 0);
    sys_obj_ids[3] = gfx_object_create(0x52, 0x50, 0x20, save.pet.owner_bday_day % 10, 0, D_208EDB0);
    func_00009A6();
    INT_F16T45 = 0x40;
    INT_E16T45 = 0x44;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    c_presses = 0;
    repeating = 0;
    D_0084271 = 0;
    tmr_t5_start_timeout(500);
    for (;;) {
        if (sys_display_state != 0) {
            T16_CTL0 = 0;
            T16_CTL1 = 0;
            T16_CTL3 = 0;
            T16_CTL5 = 0;
            sys_display_off();
            while (sys_display_state != 0)
                asm volatile("slp");
            if (sys_battery_check_due) {
                sys_battery_low();
                sys_battery_check_due = 0;
            }
            if (sys_battery_level <= 0x2e3)
                sys_power_down();
            tmr_t5_start_timeout(500);
        } else {
            sys_idle_tick();
        }
        if (tmr_flags & 0x20) {
            blink++;
            tmr_flags &= ~0x20;
            if ((P0_P0D & 1) == 0) {
                D_0084271++;
                if (D_0084271 > 2) {
                    if (repeating == 0) {
                        repeating = 1;
                        tmr_t5_start_timeout(0x4b);
                    }
                    btn_a_presses++;
                    sys_idle_seconds = 0;
                    D_0084271 = 2;
                    snd_play(1);
                }
            } else {
                D_0084271 = 0;
                if (repeating != 0) {
                    repeating = 0;
                    tmr_t5_start_timeout(500);
                }
            }
        }
        if (btn_a_presses != 0) {
            blink = 0;
            if (repeating == 0) {
                c_presses = 0;
                T16_CTL5 = 2;
                T16_CTL5 = 1;
            }
            if (P0_P0D & 4) {
                switch (field) {
                case 0:
                    if (++save.pet.owner_bday_month > 12)
                        save.pet.owner_bday_month = 1;
                    save.pet.owner_bday_day = rtc_next_day_leap(save.pet.owner_bday_month, save.pet.owner_bday_day);
                    break;
                case 1:
                    save.pet.owner_bday_day++;
                    save.pet.owner_bday_day = rtc_next_day_leap(save.pet.owner_bday_month, save.pet.owner_bday_day);
                    break;
                }
            } else {
                switch (field) {
                case 0:
                    if (--save.pet.owner_bday_month == 0)
                        save.pet.owner_bday_month = 12;
                    save.pet.owner_bday_day = rtc_next_day_leap(save.pet.owner_bday_month, save.pet.owner_bday_day);
                    break;
                case 1:
                    save.pet.owner_bday_day--;
                    save.pet.owner_bday_day = rtc_clamp_day(save.pet.owner_bday_month, save.pet.owner_bday_day);
                    break;
                }
            }
            btn_a_presses = 0;
        }
        if (btn_b_presses != 0) {
            c_presses = 0;
            field++;
            if (field > 1)
                break;
            btn_b_presses = 0;
        }
        if (btn_c_presses != 0) {
            c_presses++;
            if ((c_presses & 1) == 0) {
                field--;
                if (field < 0) {
                    mode = 2;
                    goto clock;
                }
            }
            btn_c_presses = 0;
        }
        if (save.pet.owner_bday_month > 9)
            gfx_object_set_image(sys_obj_ids[0], 0x20, 1, D_208EDB0);
        else
            gfx_object_set_image(sys_obj_ids[0], 0x20, 0, 0);
        gfx_object_set_image(sys_obj_ids[1], 0x20, save.pet.owner_bday_month % 10, D_208EDB0);
        if (save.pet.owner_bday_day > 9)
            gfx_object_set_image(sys_obj_ids[2], 0x20, save.pet.owner_bday_day / 10, D_208EDB0);
        else
            gfx_object_set_image(sys_obj_ids[2], 0x20, 0, 0);
        gfx_object_set_image(sys_obj_ids[3], 0x20, save.pet.owner_bday_day % 10, D_208EDB0);
        if (blink & 1) {
            switch (field) {
            case 0:
                gfx_object_set_image(sys_obj_ids[0], 0x20, 0, 0);
                gfx_object_set_image(sys_obj_ids[1], 0x20, 0, 0);
                break;
            case 1:
                gfx_object_set_image(sys_obj_ids[2], 0x20, 0, 0);
                gfx_object_set_image(sys_obj_ids[3], 0x20, 0, 0);
                break;
            }
        }
        func_00009A6();
        rand();
    }
    /* the owner's name (txt_input; cancelled: back to the birthday's day) */
    room_weather_draw_daily();
    for (i = 0; i < T16_TC2; i++)
        rand();
    p = name;
    n = txt_input(4, 0, (int)p);
    if (n > 0) {
        memcpy(save.pet.owner_name, p, 16);
    } else {
        field = 1;
        if (n == 0)
            goto birthday;
    }
    for (i = 0; i < T16_TC2; i++)
        rand();
    if (sys_debug_strap_count == 8) {
        /* debug strap: pick the character (A: the next built-in one, then the downloaded
         * ones; B or C: take it), with full clover leaves, happiness, 99999 points, the
         * unlock flags 0x14..0x28 and an age for its stage */
        spot_reader_mode2();
        chara_idx = 0;
        save.pet.x000 = chara_builtin_ids[0];
        pet_become_character(save.pet.x000);
        if (chara_pet_sprite_set <= 0x63)
            chara_obj = gfx_object_create(0x30, 0x3b, 0x40, 0, 0,
                                    (const u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0);
        else
            chara_obj = gfx_object_create(0x30, 0x3b, 0x40, 0, 0,
                                    dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 0));
        func_00009A6();
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        for (;;) {
            if (btn_a_presses != 0) {
                btn_a_presses = 0;
                chara_idx++;
                if (chara_builtin_ids[chara_idx] <= 0x1388) {
                    save.pet.x000 = chara_builtin_ids[chara_idx];
                    if (save.pet.x000 == 0xffff)
                        chara_idx = 0;
                    save.pet.hunger_timer = 0;
                    save.pet.happiness_timer = 0;
                    save.pet.x000 = chara_builtin_ids[chara_idx];
                    pet_become_character(save.pet.x000);
                    gfx_object_set_image(chara_obj, 0x40, 0,
                                         (const u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0);
                    dl_chara_idx = 0;
                } else {
                    if (dl_chara_count() == 0) {
                        chara_idx = 0;
                    } else {
                        if (dl_chara_idx < dl_chara_count()) {
                            save.pet.x000 = dl_chara_record(dl_chara_idx)->id;
                            dl_chara_idx++;
                            chara_idx--;
                            goto chosen;
                        }
                        chara_idx = 0;
                        dl_chara_idx = chara_idx;
                    }
                    save.pet.x000 = chara_builtin_ids[0];
                chosen:
                    save.pet.hunger_timer = 0;
                    save.pet.happiness_timer = 0;
                    pet_become_character(save.pet.x000);
                    if (save.pet.x000 <= 0x1388)
                        gfx_object_set_image(chara_obj, 0x40, 0,
                                             (const u8 *)chara_sprite_sets[chara_pet_sprite_set].pose0);
                    else
                        gfx_object_set_image(chara_obj, 0x40, 0,
                                             dl_chara_pose_pic((u8)save.pet.chara_row + 0x77, 1));
                }
                func_00009A6();
            }
            if (btn_b_presses != 0) {
                btn_b_presses = 0;
                break;
            }
            if (btn_c_presses != 0) {
                btn_c_presses = 0;
                break;
            }
            sys_idle_tick();
        }
        save.pet.clover_leaves = 15;
        pet_add_happiness(20);
        pet_add_points(99999);
        for (i = 0x1d; i <= 0x28; i++)
            pet_set_unlock_flag(i, 1);
        for (i = 0x14; i <= 0x1c; i++)
            pet_set_unlock_flag(i, 1);
        switch (save.pet.life_stage) {
        case 1:
            save.pet.age_minutes = 1;
            break;
        case 2:
            save.pet.age_minutes = 60;
            break;
        case 3:
        default:
            save.pet.age_minutes = 5820;
            break;
        }
        save.pet.age_days = (save.pet.age_minutes + 1439) / 1440;
        save.pet.owner_id = rand();
        for (i = 0; i < 1000; i++) {
            if (save.pet.owner_id != 0)
                break;
            save.pet.owner_id = rand();
        }
    } else {
        /* a new egg */
        save.pet.owner_id = rand();
        for (i = 0; i < 1000; i++) {
            if (save.pet.owner_id != 0)
                break;
            save.pet.owner_id = rand();
        }
        pet_set_unlock_flag(0x46, 1);
        pet_set_unlock_flag(0x47, 1);
        pet_set_unlock_flag(0x48, 1);
        pet_set_unlock_flag(0x49, 1);
        pet_new_egg_random();
    }
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    snd_wait_end();
    evo_stage_change_reset();
    pet_set_unlock_flag(0x46, 1);
    pet_set_unlock_flag(0x47, 1);
    pet_set_unlock_flag(0x48, 1);
    pet_set_unlock_flag(0x49, 1);
    pet_clear_traits(1);
    pet_clear_growth_items();
    save.pet.baby_id = 0;
    save.pet.child_id = 0;
    save.pet.adult_id = 0;
    save.pet.special_id = 0;
    switch (save.pet.life_stage) {
    case 1:
        save.pet.baby_id = save.pet.x000;
        break;
    case 2:
        save.pet.baby_id = pet_get_gender() == 0 ? 1 : 3;
        save.pet.child_id = save.pet.x000;
        break;
    case 3:
        if (pet_get_gender() == 0) {
            save.pet.baby_id = 1;
            save.pet.child_id = 6;
        } else {
            save.pet.baby_id = 3;
            save.pet.child_id = 12;
        }
        save.pet.adult_id = save.pet.x000;
        break;
    }
    save_game();
    return 0;
}

/*
 * The RAM variables this file defines: the object ids of the birthday screen's four digits
 * (month tens / ones, day tens / ones) and D_0084271, the count of blink ticks A has been
 * held on that screen (auto-repeat from the third).
 */
u8 sys_obj_ids[4] DSTRAM;
u8 D_0084271 DSTRAM;
