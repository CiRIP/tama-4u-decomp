/*
 * The pet-left screen and the search question that may bring it back.
 *
 * Code 24FE0C6..24FE21C, .rodata from 253A4D0. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/pet/pet_gone.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/pet/pet_bath.h"
#include "proto/food/food_meals.h"
#include "proto/room/room_window.h"
#include "proto/diary/diary_notebook.h"
#include "proto/mail/mail_letters.h"
#include "proto/rtc/rtc_clock_screen.h"
#include "proto/anim/anim_scripts.h"
#include "proto/sys/sys_boot.h"
#include "proto/ui/ui_message.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/chara/chara_render_flash.h"
#include "proto/ui/ui_menu.h"
#include "proto/chara/chara_select.h"
#include "proto/save/save_flash.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care.h"
#include "proto/pet/pet_status.h"
#include "proto/ui/ui_yes_no.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "hw.h"

/*
 * Event after the pet has been in state x011 bit 4 (departure) less than one day
 * (pet.gone_minutes < 1440 minutes): runs ui_yes_no_question until it is resolved, with animations.
 */
u8 pet_search_question(void)
{
    int r;

    if (save.pet.gone_minutes >= 1440) {
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        btn_events &= 0xf0;
        return 0;
    }
    for (;;) {
        r = ui_yes_no_question(0, 0);
        if (r == 0) {
            btn_a_presses = 1;
            btn_b_presses = 1;
            btn_c_presses = 1;
            anim_busy = 1;
            anim_search_1();
            room_visitor_scene = 1;
            anim_search_2();
            room_visitor_scene = 0;
            anim_search_3();
            anim_search_4();
            anim_search_5();
            anim_search_6();
            anim_search_7();
            anim_search_found();
            ui_show_message(5, 0, chara_text(save.pet.x000, 0xb, 0));
            anim_search_return();
            save.pet.decline_minutes = 0;
            save.pet.status = 0;
            save.pet.sick_count = 0;
            save.pet.starve_minutes = 0;
            save.pet.gone_minutes = 0;
            save.pet.revive_talks = 0;
            save.pet.neglect_minutes = 0;
            anim_happy();
            snd_wait_end();
            save_game();
            anim_busy = 0;
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            btn_events &= 0xf0;
            room_clock_notice = 0;
            return 1;
        }
        if (r == 1) {
            r = ui_yes_no_question(1, 1);
            if (r == 0) {
                save.pet.gone_minutes = 1440;
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                btn_events &= 0xf0;
                return 0;
            }
            if (r <= 2)
                continue;
            btn_a_presses = 0;
            btn_b_presses = 0;
            btn_c_presses = 0;
            btn_events &= 0xf0;
            return 0;
        }
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        btn_events &= 0xf0;
        return 0;
    }
}

/*
 * The "pet has gone" screen (main, status bits 3 / 4 = the last steps of the departure
 * ladder): a == 0 plays the leaving sequence until bit 4 is set; then the empty home waits
 * for the owner: within 1440 minutes away (pet.gone_minutes) a key offers the search
 * (pet_search_question, "さがしにいく?"), A + C held starts a new egg. Not a sleep screen.
 */
void pet_gone_screen(int a)
{
    int cnt;
    volatile u8 *p;
    u8 *s;
    u8 *u;
    u8 *w;

    pet_life_paused = 0;
    sys_display_wake(1);
    btn_events = 0;
    if (a == 0) {
        ui_main_icon = a;
        save.pet.x082 = a;
        save.pet.x062 = a;
        save.pet.x0cc = a;
        save.pet.x148 = a;
        save.pet.calls = a;
        friend_loop_noop_a();
        save_game();
        btn_a_presses = 1;
        btn_b_presses = 1;
        btn_c_presses = 1;
        do
            anim_pet_leaving();
        while (!(save.pet.status & 0x10));
        anim_pet_gone();
        save_game();
        save.pet.companion_sprite_set = 0;
        btn_events = 0;
    }
top:
    anim_scene_room(2);
    anim_draw_pic(0, (u8 *)0x216e27c, 0, 0x40, 0x24);
    anim_draw_pic(1, (u8 *)0x2196d8c, 0, 0x40, 0x24);
    anim_busy = 0;
    func_00009A6();
    tmr_t5_start_timeout(500);
    cnt = 0;
    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    for (;;) {
        if (save.pet.gone_minutes <= 0x59f) {
            btn_events |= 0x70;
            if (btn_events & 7) {
                btn_events &= 0xf0;
                if (pet_search_question() == 1) {
                    btn_a_presses = 0;
                    btn_b_presses = 0;
                    btn_c_presses = 0;
                    btn_events = 0;
                    if (save.pet.praise_timer == 0)
                        pet_habit_learned();
                    return;
                }
                btn_events &= 0xf0;
            }
        } else {
            if (save.pet.gone_minutes == 0x5a0) {
                save.pet.gone_minutes++;
                save_game();
            }
            if (save.pet.gone_minutes <= 0x59f) {
                btn_events &= 0xf;
                btn_events |= 0x70;
            } else {
                btn_events &= 0x7f;
                btn_events |= 0x70;
            }
            p = &btn_events;
            if ((*p & 1) && (P0_P0D & 4)) {
                *p &= 0xf0;
                mail_farewell_letter();
                *p = 0;
                *p |= 0x50;
                ui_timeout = 10;
                for (;;) {
                    if (*p & 1) {
                        pet_status_pages(1);
                        ui_timeout = 10;
                        *p &= ~1;
                        for (;;) {
                            if (btn_events & 1) {
                                btn_events &= ~1;
                                break;
                            }
                            if (btn_events & 4) {
                                btn_events &= ~4;
                                break;
                            }
                            asm volatile("halt");
                            if (ui_timeout == 0)
                                break;
                            sys_idle_housekeeping();
                        }
                        *p &= 0xf0;
                        break;
                    }
                    if (btn_events & 4) {
                        btn_events &= ~4;
                        break;
                    }
                    asm volatile("halt");
                    if (ui_timeout == 0)
                        break;
                    sys_idle_housekeeping();
                }
                btn_events &= 0xf0;
                goto top;
            }
            if (*p & 2) {
                ui_new_screen();
                rtc_clock_create();
                rtc_clock_show_now();
                func_00009A6();
                *p = 0;
                *p |= 0x30;
                for (;;) {
                    if (btn_events & 1) {
                        if (!(P0_P0D & 4))
                            btn_events &= ~4;
                        else
                            rtc_clock_blink_toggle();
                        btn_events &= ~1;
                    }
                    if (btn_events & 2) {
                        if (P0_P0D & 4) {
                            btn_events &= ~2;
                            btn_events &= 0xf0;
                            goto top;
                        }
                        btn_events &= ~4;
                        btn_events &= ~2;
                    }
                    if (btn_events & 4)
                        btn_events &= ~4;
                    if (sys_display_state != 0) {
                        T16_CTL0 = 0;
                        T16_CTL1 = 0;
                        T16_CTL3 = 0;
                        T16_CTL5 = 0;
                        sys_display_off();
                        if (sys_display_state == 2) {
                            do
                                asm volatile("slp");
                            while (sys_display_state == 2);
                        }
                        if (sys_battery_check_due != 0) {
                            sys_battery_low();
                            sys_battery_check_due = 0;
                        }
                        goto chk;
                    }
                    sys_idle_tick();
                    rtc_clock_show_now();
                    func_00009A6();
                }
            }
            if (btn_events & 4) {
                snd_wait_end();
                btn_c_presses = 1;
                btn_events &= ~4;
            }
            if ((P0_P0D & 5) == 0) {
                pet_reset_for_new_egg();
                save.pet.family_continues = 0;
                save.pet.revive_talks = 0;
                rtc_seconds = 0;
                pet_new_egg_random();
                save.pet.generation = 1;
                rtc_seconds = 0;
                room_clock_notice = 0;
                room_mood_question_due = 1;
                btn_a_presses = 0;
                btn_b_presses = 0;
                btn_c_presses = 0;
                btn_events = 0;
                return;
            }
        }
        if (sys_display_state != 0) {
            T16_CTL0 = 0;
            T16_CTL1 = 0;
            T16_CTL3 = 0;
            T16_CTL5 = 0;
            sys_display_off();
            if (sys_display_state == 2) {
                do
                    asm volatile("slp");
                while (sys_display_state == 2);
            }
            if (sys_battery_check_due != 0) {
                sys_battery_low();
                sys_battery_check_due = 0;
            }
        chk:
            if (sys_battery_level <= 0x2e3)
                sys_power_down();
            goto top;
        }
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            cnt = (u8)(cnt + 1);
            switch (cnt) {
            case 1:
                if (save.pet.gone_minutes <= 0x59f) {
                    anim_scene_room(2);
                    anim_draw_pic(0, (u8 *)0x216e27c, 0, 0x40, 0x24);
                    anim_draw_pic(1, (u8 *)0x2196d8c, 0, 0x40, 0x24);
                } else {
                    ui_new_screen();
                    s = (u8 *)0x216e27c;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x40 - (s[1] >> 1), 0, 0, 0, (int)s);
                    s = (u8 *)0x2196d8c;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x40 - (s[1] >> 1), 0x10, 0, 0, (int)s);
                }
                break;
            case 2:
            case 3:
                if (save.pet.gone_minutes <= 0x59f) {
                    anim_scene_room(4);
                    anim_draw_pic(0, (u8 *)0x216e27c, 0, 0x40, 0x24);
                    anim_draw_pic(1, (u8 *)0x2196ff8, 0, 0x40, 0x27);
                    w = (u8 *)0x214da4c;
                    anim_draw_pic(2, w, 0, 0x27, 0x13);
                    anim_draw_pic(3, w, 0x10, 0x59, 0x13);
                } else {
                    ui_new_screen();
                    s = (u8 *)0x216e27c;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x40 - (s[1] >> 1), 0, 0, 0, (int)s);
                    s = (u8 *)0x2196ff8;
                    gfx_object_create(0x40 - (s[0] >> 1), 0x43 - (s[1] >> 1), 0x10, 0, 0, (int)s);
                    u = (u8 *)0x214da4c;
                    gfx_object_create(0x27 - (u[0] >> 1), 0x2f - (u[1] >> 1), 0x20, 0, 0, (int)u);
                    gfx_object_create(0x59 - (u[0] >> 1), 0x2f - (u[1] >> 1), 0x30, 0, 0x10, (int)u);
                }
                break;
            default:
                goto top;
            }
            anim_busy = 0;
            func_00009A6();
        }
        sys_idle_tick();
    }
}
