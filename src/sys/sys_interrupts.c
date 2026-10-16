/*
 * Interrupt handlers: the RTC second / minute tick that drives the pet clock, button and timer
 * interrupts, traps, the A/D interrupt, and idle housekeeping.
 *
 * Code 24FF904..2500436, .rodata from 253B244. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/sys/sys_interrupts.h"
#include "proto/save/save_flash.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/pet/pet_bath.h"
#include "proto/town/town_places.h"
#include "proto/pet/pet_toilet.h"
#include "proto/comm/comm_menu.h"
#include "proto/ir/ir_receive.h"
#include "proto/ui/ui_menu.h"
#include "proto/town/town_street.h"
#include "proto/chara/chara_select.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/snd/snd_play.h"
#include "proto/pet/pet_care_room.h"
#include "proto/pet/pet_care.h"
#include "proto/spot/spot_reader.h"
#include "proto/test/test_mode.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/nfc/nfc_session.h"
#include "proto/sys/sys_boot.h"
#include "hw.h"

/*
 * Re-arm the buttons: clear the press counters btn_a_presses..D_0001422 (A, B, C), clear the
 * port-input 0-2 cause flags and enable their interrupts (pINT_EK01_EP03 = 7).
 */
void btn_rearm(void)
{
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    INT_FK01_FP03 = 7;
    INT_EK01_EP03 = 7;
}

/*
 * Sample the P06 debug strap: sys_debug_strap_count counts the consecutive ticks with P06 low
 * (saturating at 8; reset to 0 when high). P06 is an input that is not one of the three
 * buttons (a debug strap or test pad); with sys_debug_strap_count == 8 at boot main does not arm
 * the mood question (room_mood_question_due = 0), and comm_menu_enable / comm_main_menu check P06 for the debug menu
 * variant. Other effects of sys_debug_strap_count == 8: the factory test mode test_mode only shows
 * "DB ON" and halts; the RTC handler rtc_isr never counts down to the automatic
 * screen-off (sys_idle_seconds); the C button ISR btn_c_isr turns a C press into the time
 * fast-forward flag sys_debug_fast_forward; test_debug_set_character (debug set) gives all souvenirs.
 */
void sys_sample_debug_strap(void)
{
    if ((P0_P0D & 0x40) == 0) {
        sys_debug_strap_count++;
        if (sys_debug_strap_count > 8)
            sys_debug_strap_count = 8;
    } else {
        sys_debug_strap_count = 0;
    }
}

/* Per-tick service, called after every `halt` by sys_idle_tick (the idle tick): sample the
 * P06 input for the hold counter sys_debug_strap_count (sys_sample_debug_strap), apply a pending backlight level
 * change (sys_apply_backlight), update the care-miss icon (ui_call_icon_sync). */
/*
 * Idle housekeeping after each `halt`: sample the P06 debug strap (sys_sample_debug_strap), apply
 * the backlight level (sys_apply_backlight), keep the attention icon in sync (ui_call_icon_sync).
 */
void sys_idle_housekeeping(void)
{
    sys_sample_debug_strap();
    sys_apply_backlight();
    ui_call_icon_sync();
}

void __attribute__((interrupt_handler)) rtc_isr(void)
{
    u8 st;
    u8 v0;
    u8 k;
    u8 m, h, d, mo, x;
    u8 cnt;
    int flag;

    rtc_read_clock();
    st = save.pet.status;
    if (test_rtc_count_mode == 1) {
        if (++test_rtc_seconds > 100)
            test_rtc_seconds = 100;
    } else {
        if (pet_life_paused == 0) {
            if (sys_stub_zero() != 0) {
                if (save.pet.toilet_timer != 0) {
                    if (save.pet.training[0] != 0xff)
                        pet_make_poop();
                    save.pet.toilet_timer = 0;
                }
            } else {
                if (save.pet.x040 != 0)
                    save.pet.x040--;
                if (save.pet.toilet_timer != 0 && pet_sleep_state() != 3 && --save.pet.toilet_timer == 0) {
                    k = 7;
                    if (save.pet.training[0] != 0xff) {
                        pet_make_poop();
                        k = save.pet.poop_count;
                    }
                    if (save.pet.poop_count > 6) {
                        pet_poop_overflow = 1;
                        save.pet.poop_count = 6;
                    } else
                        pet_poop_overflow = 0;
                    if ((sys_display_state == 2 || anim_busy != 0) && k <= 6) {
                        v0 = sys_display_state;
                        if (save.pet.poop_count == 6) {
                            if (pet_get_sick() == 1)
                                sys_display_wake(1);
                            else
                                sys_display_wake(0);
                            if (anim_busy == 0 && room_idle == 1 && (st & 0xfd) == 0)
                                snd_play(4);
                        }
                        if (save.pet.poop_count > 6)
                            save.pet.poop_count = 6;
                        if (v0 != 0 || anim_busy != 0)
                            pet_poops_drawn = save.pet.poop_count;
                    }
                    if (sys_display_state != 0 || anim_busy != 0)
                        pet_poops_drawn = save.pet.poop_count;
                    pet_toilet_expired = 1;
                }
                if (rtc_get_hour() == 0x12 && rtc_get_minute() == 0 && rtc_get_second() == 0) {
                    if (save.pet.cleaning_timer == 0 && save.pet.status == 0) {
                        if (save.pet.life_stage == 2 && save.pet.training[2] != 0xff) {
                            save.pet.cleaning_timer = 900;
                            pet_cleaning_reminder = 1;
                            sys_display_wake(1);
                        } else if (save.pet.life_stage > 1)
                            save.pet.cleaning_timer = 300;
                        if (save.pet.cleaning_timer != 0)
                            pet_request_bits |= 4;
                    }
                    if (save.pet.life_stage > 2 && save.pet.training[2] != 0xff)
                        save.pet.cleaning_timer = 0;
                }
                if (rtc_get_hour() == 0x13 && rtc_get_minute() == 0 && rtc_get_second() == 0) {
                    if (save.pet.bath_timer == 0 && save.pet.status == 0) {
                        if (save.pet.life_stage == 2 && save.pet.training[1] != 0xff)
                            save.pet.bath_timer = 900;
                        else if (save.pet.life_stage > 1)
                            save.pet.bath_timer = 300;
                        if (save.pet.bath_timer != 0)
                            pet_request_bits |= 2;
                    }
                    if (save.pet.life_stage > 2 && save.pet.training[1] != 0xff)
                        save.pet.bath_timer = 0;
                }
                if (save.pet.cleaning_timer != 0 && --save.pet.cleaning_timer == 0) {
                    if (sys_display_state == 2 || anim_busy != 0)
                        sys_display_wake(0);
                    pet_cleaning_expired = 1;
                }
                if (save.pet.bath_timer != 0 && --save.pet.bath_timer == 0)
                    pet_bath_expired = 1;
                if (save.pet.praise_timer != 0 && --save.pet.praise_timer == 0)
                    pet_praise_expired = 1;
            }
        }
        if (sys_debug_fast_forward != 0) {
            cnt = flag = 0;
        again:
            visit_tick();
            m = rtc_get_minute() + 1;
            if (m > 59) {
                m = 0;
                h = rtc_get_hour() + 1;
                if (h > 23) {
                    h = 0;
                    {
                        u8 t = rtc_get_day() + 1;
                        mo = rtc_get_month();
                        d = rtc_next_day_leap(mo, t);
                    }
                    if (rtc_get_day() > d) {
                        mo++;
                        if (mo > 12)
                            mo = 1;
                    }
                    save.pet.clock_month = mo;
                    save.pet.clock_day = d;
                }
                save.pet.clock_hour = h;
            }
            save.pet.clock_minute = m;
            save.pet.clock_second = 0;
            if (pet_minute_tick() != 0)
                flag = 1;
            if (pet_clock_events() != 0)
                flag = 1;
            if (flag != 0) {
                if (outing_time_to_return() != 0 || job_is_finished() != 0) {
                call1:
                    sys_display_wake(1);
                } else {
                    if ((s8)save.pet.calls >= 0)
                        goto rest;
                    goto call1;
                rest:
                    if ((save.pet.status & 0x14) == 4)
                        sys_display_wake(1);
                    else
                        sys_display_wake(0);
                    if (anim_busy == 0 && room_idle == 1) {
                        x = save.pet.status;
                        if (!(x & 0x10) && (x != 0 || (save.pet.calls & 0xf) != 0))
                            snd_play(4);
                    }
                }
            } else {
                sys_awake_seconds += 300;
                if (++cnt <= 4)
                    goto again;
            }
            rtc_set_time(save.pet.clock_month, save.pet.clock_day, save.pet.clock_hour, save.pet.clock_minute);
        } else {
            if (comm_menu_active == 1)
                sys_idle_seconds = 0;
            if (sys_debug_strap_count != 8) {
                if (sys_idle_seconds > 30)
                    sys_display_state = 2;
                else
                    sys_idle_seconds++;
            }
            if (anim_busy != 0)
                sys_idle_seconds = 0;
            if (pet_life_paused == 0)
                rtc_seconds++;
            if (visit_tick() != 0)
                sys_display_wake(1);
            if (friend_event_minute_check() != 0)
                sys_display_wake(1);
            if (rtc_seconds > 59) {
                rtc_seconds = 0;
                if (pet_minute_tick() != 0) {
                    if (outing_time_to_return() != 0 || job_is_finished() != 0 || (s8)save.pet.calls < 0) {
                        sys_display_wake(1);
                    } else {
                        if ((save.pet.status & 0x14) == 4)
                            sys_display_wake(1);
                        else
                            sys_display_wake(0);
                        if (anim_busy == 0 && room_idle == 1) {
                            x = save.pet.status;
                            if (!(x & 0x10) && (x != 0 || (save.pet.calls & 0xf) != 0))
                                snd_play(4);
                        }
                    }
                } else if (save.pet.age_minutes == 1 && (save.pet.calls & 3)) {
                    if (sys_display_state == 2)
                        sys_display_wake(0);
                    snd_play(4);
                }
            }
            if (rtc_get_second() == 0 && pet_clock_events() != 0)
                sys_display_wake(1);
        }
        job_check_hours();
        outing_hours_check();
        if (outing_time_to_return() != 0 || job_is_finished() != 0)
            sys_display_wake(1);
        pet_sitter_hours();
        if (pet_sitter_due() != 0)
            sys_display_wake(1);
        if (++sys_awake_seconds > 1200) {
            sys_awake_seconds = 0;
            if (sys_battery_dead == 0) {
                sys_battery_check_due = 1;
                sys_display_wake(0);
            }
        }
        if (ui_timeout != 0)
            ui_timeout--;
    }
    RTCINTSTAT = 1;
    INT_FP47_FRTC_FAD = 4;
}

void __attribute__((interrupt_handler)) sys_isr_halt_a(void)
{
    asm volatile("halt");
    for (;;)
        ;
}

void __attribute__((interrupt_handler)) sys_trap_halt(void)
{
    asm volatile("halt");
    for (;;)
        ;
}

void __attribute__((interrupt_handler)) sys_trap_wait(void)
{
    asm volatile("halt");
    sys_trap_state = 1;
    for (;;) {
        if (sys_trap_state != 1) {
            sys_trap_state++;
            break;
        }
    }
}

void __attribute__((interrupt_handler)) sys_address_error_trap(void)
{
    asm volatile("halt");
    sys_trap_state = 1;
again:
    if (sys_trap_state == 1)
        goto again;
    sys_trap_state++;
}

void __attribute__((interrupt_handler)) sys_nmi_trap(void)
{
    asm volatile("halt");
    for (;;)
        ;
}

void __attribute__((interrupt_handler)) sys_trap_record(void)
{
    if (sys_trap_state != 0)
        sys_trap_last = sys_trap_state;
    else
        sys_trap_last = 0xff;
}

void __attribute__((interrupt_handler)) tmr_t16_23_isr(void)
{
    tmr_flags |= 4;
    INT_F16T23 = 0x40;
}

void __attribute__((interrupt_handler)) tmr_t16_45_isr(void)
{
    tmr_flags |= 0x20;
    INT_F16T45 = 0x40;
}


/*
 * Button A interrupt (port input 0 = P00, vector 16); btn_b_isr / btn_c_isr are B / C
 * (P01 / P02, vectors 17 / 18). Debounce: count while the pin stays low, up to 0x600; wake
 * the display (sys_display_wake(1)). On a full count, the key click: sound 1 (B: 2, C: 3,
 * snd_play) unless a sub-screen owns the buttons (anim_busy) or the press is already
 * counted, then count the press (btn_a_presses / 1421 / 1422; A and B capped at 100) and set
 * the event bit in btn_events. C has an extra case (sys_debug_strap_count == 8: only sys_debug_fast_forward = 1).
 */
void __attribute__((interrupt_handler)) btn_a_isr(void)
{
    int beeped = 0;

    btn_a_debounce = beeped;
    while (btn_a_debounce <= 0x5ff) {
        if (P0_P0D & 1)
            break;
        btn_a_debounce++;
    }
    if (sys_display_state == 2)
        btn_a_debounce = 0;
    sys_display_wake(1);
    if (btn_a_debounce == 0x600) {
        if (anim_busy == 0 && btn_a_presses == 0) {
            snd_play(1);
            btn_click_played = 1;
            beeped = 1;
        }
        if (btn_a_presses <= 0x63)
            btn_a_presses++;
        if (anim_busy == 0 && (btn_events & 0x10) && !(btn_events & 1) && beeped == 0) {
            snd_play(1);
            btn_click_played = 1;
        }
        btn_events |= 1;
    }
    INT_FK01_FP03 = 1;
}

void __attribute__((interrupt_handler)) btn_b_isr(void)
{
    int beeped = 0;

    btn_b_debounce = beeped;
    while (btn_b_debounce <= 0x5ff) {
        if (P0_P0D & 2)
            break;
        btn_b_debounce++;
    }
    if (sys_display_state == 2)
        btn_b_debounce = 0;
    sys_display_wake(1);
    if (btn_b_debounce == 0x600) {
        if (anim_busy == 0 && btn_b_presses == 0) {
            snd_play(2);
            btn_click_played = 1;
            beeped = 1;
        }
        if (btn_b_presses <= 0x63)
            btn_b_presses++;
        if (anim_busy == 0 && (btn_events & 0x20) && !(btn_events & 2) && beeped == 0) {
            snd_play(2);
            btn_click_played = 1;
        }
        btn_events |= 2;
    }
    INT_FK01_FP03 = 2;
}


/*
 * C button (P02) port-input ISR: debounce as for A/B (0x600 polls); every accepted press
 * sets nfc_session.x000_b = 0xF and requests the NFC session abort (nfc_user_abort). Debug: with the P06 strap low (sys_debug_strap_count == 8) a C press
 * while sys_debug_fast_forward == 0 is not counted but sets sys_debug_fast_forward = 1 = time fast-forward: the RTC
 * handler rtc_isr then advances the game clock by up to 5 minutes per second (minute
 * simulation pet_minute_tick / daily events pet_clock_events each step, stopping at the first
 * event), main shortens its animation tick and anim_frame_wait waits 10 ms. The next C press
 * counts normally and clears sys_debug_fast_forward (as do scene set-ups, care calls and sleep).
 */
void __attribute__((interrupt_handler)) btn_c_isr(void)
{
    int beeped = 0;

    btn_c_debounce = beeped;
    while (btn_c_debounce <= 0x5ff) {
        if (P0_P0D & 4)
            break;
        btn_c_debounce++;
    }
    if (btn_c_debounce == 0x600) {
        nfc_session.x000_b = 0xf;
        nfc_result = nfc_user_abort();
    }
    if (sys_display_state == 2)
        btn_c_debounce = 0;
    sys_display_wake(1);
    if (btn_c_debounce == 0x600) {
        if (sys_debug_strap_count == 8 && sys_debug_fast_forward == 0) {
            sys_debug_fast_forward = 1;
        } else {
            if (anim_busy == 0 && btn_c_presses == 0) {
                snd_play(3);
                beeped = 1;
            }
            sys_debug_fast_forward = 0;
            btn_c_presses++;
            if (anim_busy == 0 && (btn_events & 0x40) && !(btn_events & 4) && beeped == 0)
                snd_play(3);
            btn_events |= 4;
        }
    }
    INT_FK01_FP03 = 4;
}


void __attribute__((interrupt_handler)) sys_adc_isr(void)
{
    sys_battery_level = AD_ADD;
    sys_battery_level = AD_CH0_BUF;
    AD_EN_SMPL_STAT &= ~6;
    sys_adc_busy = 0;
    INT_FP47_FRTC_FAD = 2;
}

/*
 * Returns 0 (stub).
 */
int func_2500436(void)
{
    return 0;
}

/*
 * The RAM variables this file defines.
 */
u8 sys_trap_state;
u8 sys_trap_last;
u8 btn_click_played;
volatile u16 sys_awake_seconds;
volatile u8 sys_debug_strap_count;
volatile u8 btn_events;
volatile u16 btn_a_debounce;
volatile u16 btn_b_debounce;
volatile u16 btn_c_debounce;
u8 sys_battery_check_due;
volatile u8 sys_display_state;
u8 pet_cleaning_reminder;
u8 sys_adc_busy;
