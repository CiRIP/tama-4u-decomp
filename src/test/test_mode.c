/*
 * The factory test mode: entry loop and ROM sums, flash erase-all (factory reset), battery,
 * sound, RTC count tests.
 *
 * Code 25196DC..251A41E, .rodata from 255F64C.
 */
#include "proto/test/test_mode.h"
#include "proto/save/save_flash.h"
#include "proto/iram.h"
#include "proto/anim/anim_engine.h"
#include "proto/pet/pet_core.h"
#include "proto/room/room_main.h"
#include "proto/rtc/rtc_clock_screen.h"
#include "proto/sys/sys_boot.h"
#include "proto/ir/ir_session.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/rtc/rtc_driver.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/nfc/nfc_session.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "flash.h"
#include "hw.h"

/*
 * Test mode ROM check (only caller test_mode): 16-bit sum of the program halfwords
 * 0x2400000..0x2563548 (vectors, code, .rodata and the .data / .iram load images) minus
 * the reference test_rom_sum_ref. The reference itself lies in the summed range (its .data load
 * image at 0x25627D4), hence the subtraction; the caller compares the result with
 * test_rom_sum_ref again. On the dump both are 0x8F12 (verified).
 */
u16 test_rom_sum_program(void)
{
    u16 sum = -test_rom_sum_ref;
    u16 *p = flash_program_start;

    while (p < flash_program_end)
        sum += *p++;
    return sum;
}

/*
 * Test mode ROM check ("オール", only caller test_mode): 16-bit sum of the graphics data
 * 0x2080000..0x226D890 and 0x2340000..0x23D92B0; the caller expects 0x520A (the dump's
 * sum, verified).
 */
u16 test_rom_sum_graphics(void)
{
    u16 sum = 0;
    u16 *p;

    p = flash_graphics_start;
    while (p < flash_graphics_end_a)
        sum += *p++;
    p = flash_graphics_start_b;
    while (p < flash_graphics_end_b)
        sum += *p++;
    return sum;
}

/*
 * FACTORY TEST MODE (asm). Only caller: the power-on sequence sys_power_on, when the code
 * A, C, A, B, C is pressed within about 3 s of the start-up picture (test_check_entry_code). Runs
 * with lcd_palette_set = 1 (first palette table set in the LCD flush) and backlight level 4.
 * With the P06 debug strap low (sys_debug_strap_count == 8) it only shows 0x20804B8 "DB ON" and halts.
 * Otherwise (see notes/findings/testmode.md):
 *   1. PN512 off; full-screen 0x001F, 0x07E0, 0xF800, 0xFFFF, left with A, B, C, A.
 *   2. Power-down: only the A key interrupt on (pINT_EK01_EP03 = 1), display off and SLEEP
 *      (sys_display_off), then sys_display_state = 2 and a second `slp`; A wakes each time.
 *   3. test_rtc_count: RTC reset and 1 s interrupt count ("COUNT"), A ends it.
 *   4. "VER:" (0x2087100) 004 and 15-05-16 (the build date). A -> 5; B then B+C -> the
 *      sound test test_sound and return.
 *   5. test_battery_check battery check (empty: "でんちをかえてね!!" and halt). Then A -> 7, or
 *      B+C -> 6.
 *   6. "ROM TEST" (0x2084CE8) on green; A = "メイン" (blue): program sum test_rom_sum_program ==
 *      test_rom_sum_ref; B+C = "オール" (red): graphics sum test_rom_sum_graphics == 0x520A, then the
 *      program sum. Pass: "ROM OK" (0x2083ADC), A -> 8. Fail: "ROM ERROR" (0x20828D0),
 *      halt.
 *   7. "ROM TEST" on white: flash test / factory reset test_flash_erase_all (all user flash areas
 *      erased, zero-filled, verified, erased); "ROM ERROR" + halt or "ROM OK"; A; the
 *      "さうんど おん/おふ" panel 0x2089DF0; A; sound 0x37 (sound forced on, snd_enabled);
 *      "SOUND OK" 0x2085EF4; A.
 *   8. Loop: "NFC INITIALIZE" OK/NG (test_nfc_init_check, NG halts); A; Touch Spot tag tests R / C
 *      / W (test_spot_read / test_spot_check / test_spot_write); NFC power-up (NG: halt); link test
 *      test_nfc_link as a Tamagotchi (session type 1, peer kind 0xF040), then with peer kind
 *      0xF0F0: success + B returns (normal start-up follows); otherwise the brightness
 *      screen lcd_brightness_setting (pet.life_stage forced to 1 for it) and the loop again.
 * The text lines use the katakana font 0x208F3C0 (frame = gojuon index: ア = 0 .. ン =
 * 45, small kana from 46, voiced from 55) and the symbol font 0x2090E0C (0 = ー, 9 = ".").
 */
void test_mode(void)
{
    int x[1];
    volatile u8 *port;
    int a, b, f, t;

    lcd_palette_set = 1;
    gfx_set_background(0xffff);
    func_00009A6();
    sys_backlight_level = 4;
    sys_set_backlight(4);
    gfx_display_reset();
    if (sys_debug_strap_count == 8) {
        nfc_hw_power_off();
        nfc_session.x001 = 2;
        lcd_palette_set = 0;
        ui_new_screen();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x20804b8);
        func_00009A6();
        for (;;)
            asm volatile("halt");
    }
    nfc_hw_power_off();
    nfc_session.x001 = 2;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    gfx_set_background(0x1f);
    func_00009A6();
    for (;;) {
        if (btn_a_presses)
            break;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    gfx_set_background(0x7e0);
    func_00009A6();
    for (;;) {
        if (btn_b_presses)
            break;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    gfx_set_background(0xf800);
    func_00009A6();
    for (;;) {
        if (btn_c_presses)
            break;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    gfx_set_background(0xffff);
    func_00009A6();
    for (;;) {
        if (btn_a_presses)
            break;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
    btn_wait_release();
    btn_wait_release();
    btn_wait_release();
    btn_wait_release();
    test_clear_buttons();
    INT_EP47_ERTC_EAD = 0;
    INT_EK01_EP03 = 1;
    sys_display_off();
    sys_display_state = 2;
    asm volatile("slp");
    btn_rearm();
    test_rtc_count();
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    gfx_display_reset();
    gfx_set_background(0x7e0);
    func_00009A6();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2087100);
    gfx_object_create(0x44, 0x38, 0x10, 0, 0, 0x208edb0);
    gfx_object_create(0x4d, 0x38, 0x10, 0, 0, 0x208edb0);
    gfx_object_create(0x56, 0x38, 0x10, 4, 0, 0x208edb0);
    gfx_object_create(0x1c, 0x47, 0x10, 1, 0, 0x208edb0);
    gfx_object_create(0x25, 0x47, 0x10, 5, 0, 0x208edb0);
    gfx_object_create(0x2e, 0x47, 0x10, 0, 0, 0x208f37c);
    gfx_object_create(0x37, 0x47, 0x10, 0, 0, 0x208edb0);
    gfx_object_create(0x40, 0x47, 0x10, 5, 0, 0x208edb0);
    gfx_object_create(0x49, 0x47, 0x10, 0, 0, 0x208f37c);
    gfx_object_create(0x52, 0x47, 0x10, 1, 0, 0x208edb0);
    gfx_object_create(0x5b, 0x47, 0x10, 6, 0, 0x208edb0);
    func_00009A6();
    while ((P0_P0D & 7) != 7)
        ;
port = (volatile u8 *)0x300380;
keys:
    if ((*port & 7) == 6)
        goto test;
    if (*port & 2)
        x[0] = 0;
    else if ((*port & 7) == 5)
        x[0] = 1;
    else {
        a = 0;
        if (x[0] == 1)
            a = x[0];
        b = 0;
        if ((*port & 7) == 1)
            b = 1;
        if (a & b) {
            test_sound();
            return;
        }
    }
    asm volatile("halt");
    sys_idle_housekeeping();
    goto keys;

test:
    test_battery_check();
    f = 0;
again:
    if (btn_a_presses)
        goto done;
    t = P0_P0D & 6;
    if (t != 0)
        goto idle2;
    test_clear_buttons();
    gfx_display_reset();
    gfx_set_background(0x7e0);
    func_00009A6();
    gfx_object_create(t, 0x1c, t, t, t, 0x2084ce8);
    func_00009A6();
wait7:
    if ((P0_P0D & 7) != 7) {
        asm volatile("halt");
        sys_idle_housekeeping();
        goto wait7;
    }
poll:
    if (btn_a_presses) {
        gfx_display_reset();
        f = 1;
        gfx_set_background(0x1f);
        gfx_object_create(0x2e, 0x6b, 0x10, 0x21, 0x4000, 0x208f3c0);
        gfx_object_create(0x3a, 0x6b, 0x10, f, 0x4000, 0x208f3c0);
        gfx_object_create(0x46, 0x6b, 0x10, 0x2d, 0x4000, 0x208f3c0);
        func_00009A6();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2084ce8);
        func_00009A6();
        if (test_rom_sum_program() == test_rom_sum_ref)
            goto ok;
        gfx_display_reset();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x20828d0);
        func_00009A6();
        for (;;)
            asm volatile("halt");
    }
    t = P0_P0D & 6;
    if (t == 0) {
        gfx_display_reset();
        f = 1;
        gfx_set_background(0xf800);
        gfx_object_create(0x2e, 0x6b, 0x10, 4, 0x4000, 0x208f3c0);
        gfx_object_create(0x3a, 0x6b, 0x10, t, 0x4000, 0x2090e0c);
        gfx_object_create(0x46, 0x6b, 0x10, 0x28, 0x4000, 0x208f3c0);
        func_00009A6();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2084ce8);
        func_00009A6();
        if (test_rom_sum_graphics() != 0x520a) {
            gfx_display_reset();
            gfx_object_create(0, 0x1c, 0, 0, 0, 0x20828d0);
            func_00009A6();
            for (;;)
                asm volatile("halt");
        }
        if (test_rom_sum_program() == test_rom_sum_ref)
            goto ok;
        gfx_display_reset();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x20828d0);
        func_00009A6();
        for (;;)
            asm volatile("halt");
    }
    asm volatile("halt");
    sys_idle_housekeeping();
    goto poll;
ok:
    gfx_display_reset();
    gfx_object_create(0, 0x1c, 0, 0, 0, 0x2083adc);
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    func_00009A6();
    for (;;) {
        if (btn_a_presses)
            goto done;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
idle2:
    asm volatile("halt");
    sys_idle_housekeeping();
    goto again;
done:
    if (f == 0) {
        gfx_display_reset();
        gfx_set_background(0xffff);
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2084ce8);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        func_00009A6();
        if (test_flash_erase_all() != 0) {
            gfx_display_reset();
            gfx_object_create(0, 0x1c, 0, 0, 0, 0x20828d0);
            func_00009A6();
            for (;;)
                asm volatile("halt");
        }
        gfx_display_reset();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2083adc);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        func_00009A6();
        for (;;) {
            if (btn_a_presses)
                break;
            asm volatile("halt");
            sys_idle_housekeeping();
        }
        gfx_display_reset();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2089df0);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        func_00009A6();
        for (;;) {
            if (btn_a_presses)
                break;
            asm volatile("halt");
            sys_idle_housekeeping();
        }
        snd_enabled = 1;
        snd_play(0x37);
        snd_wait_end();
        snd_enabled = 0;
        gfx_display_reset();
        gfx_object_create(0, 0x1c, 0, 0, 0, 0x2085ef4);
        btn_a_presses = 0;
        btn_b_presses = 0;
        btn_c_presses = 0;
        func_00009A6();
        for (;;) {
            if (btn_a_presses)
                break;
            asm volatile("halt");
            sys_idle_housekeeping();
        }
    }
    for (;;) {
        nfc_session.x001 = 2;
        test_nfc_init_check();
        test_clear_buttons();
        for (;;) {
            if (btn_a_presses)
                break;
            asm volatile("halt");
            sys_idle_housekeeping();
        }
        test_spot_read();
        test_spot_check();
        test_spot_write();
        gfx_display_reset();
        gfx_set_background(0xffff);
        func_00009A6();
        nfc_session.x001 = 2;
        if (nfc_power_up() != 0) {
            nfc_show_session_screen(7);
            for (;;)
                asm volatile("halt");
        }
        test_clear_buttons();
        sys_idle_housekeeping();
        nfc_session.x003 = 1;
        nfc_session.peer_kind = 0xf040;
        test_nfc_link();
        nfc_session.x003 = 1;
        nfc_session.peer_kind = 0xf0f0;
        if (test_nfc_link() > 0)
            return;
        lcd_palette_set = 0;
        ui_new_screen();
        save.pet.life_stage = 1;
        lcd_brightness_setting();
        save.pet.life_stage = 0;
        lcd_palette_set = 1;
    }
}

/*
 * Test mode flash test / factory reset (only caller test_mode, "ROM TEST" screen):
 * for each entry of test_flash_areas {flags, first sector, last sector, address, fill value,
 * halfwords} with interrupts masked: bit 0 erase the sectors (func_0000A54), bit 1 fill
 * the area with the value (func_0000C46 in fixed-source mode 1), bit 2 verify every
 * halfword, bit 3 erase again. All entries use 0xF and the value 0, so every user flash
 * area ends blank: sectors 1-4 (0x2000000, the game/app code area), 8 (0x200E000, the
 * save), 9-11 (0x2010000), 12-15 (0x2040000), 0x34-0x3B (0x22C0000) and 0x62-0x87
 * (0x25A0000..0x2800000); program and graphics are not touched. Returns 0 on success.
 */
int test_flash_erase_all(void)
{
    int i, j, r;
    struct FlashTestArea *p;

    sys_irq_disable();
    for (i = 0; ; i++) {
        if (test_flash_areas[i].flags == 0xffff)
            break;
        if (test_flash_areas[i].flags & 1) {
            if (func_0000A54(0x2000000, test_flash_areas[i].first_sector, test_flash_areas[i].last_sector) != 0)
                goto fail;
        }
        if (test_flash_areas[i].flags & 2) {
            r = 1;
            if (func_0000C46(&test_flash_areas[i].fill_value, test_flash_areas[i].address, test_flash_areas[i].halfwords, r) != 0) {
                sys_irq_enable();
                return r;
            }
        }
        if (test_flash_areas[i].flags & 4) {
            p = test_flash_areas;
            for (j = 0; j < (int)p[i].halfwords; j++) {
                if (((u16 *)p[i].address)[j] != p[i].fill_value)
                    goto fail;
                p = test_flash_areas;
            }
        }
        if ((test_flash_areas[i].flags & 8)
            && func_0000A54(0x2000000, test_flash_areas[i].first_sector, test_flash_areas[i].last_sector) != 0) {
fail:
            sys_irq_enable();
            return 1;
        }
    }
    sys_irq_enable();
    return 0;
}



/*
 * Test mode battery check (only caller test_mode): title "バッテリチェック" (katakana
 * font 0x208F3C0 frames バ ッ テ リ チ ェ ッ ク), T16 ch.5 ticking every 0x1E8 counts at
 * MCLK/4096 (about 67 ms; not 0x1E8 ms), one A/D conversion started (the end-of-conversion
 * ISR sys_adc_isr stores sys_battery_level and stops the converter), sys_battery_level checked at each of
 * 10 ticks: <= 0x2E3 draws 0x2129614 "でんちをかえてね!!" and halts forever. Otherwise
 * prints "V.v" = sys_battery_level / 310 and (sys_battery_level / 31) % 10 (probably volts with a 3.3 V
 * full scale) with the "." of the symbol font 0x2090E0C.
 */
void test_battery_check(void)
{
    int n;
    int y;

    gfx_display_reset();
    y = 0x1c;
    gfx_set_background(0xffff);
    n = 0;
    gfx_object_create(0x10, y, 0x10, 0x46, n, D_208F3C0);
    gfx_object_create(0x1c, y, 0x10, 0x33, n, D_208F3C0);
    gfx_object_create(0x28, y, 0x10, 0x12, n, D_208F3C0);
    gfx_object_create(0x34, y, 0x10, 0x27, n, D_208F3C0);
    gfx_object_create(0x40, y, 0x10, 0x10, n, D_208F3C0);
    gfx_object_create(0x4c, y, 0x10, 0x31, n, D_208F3C0);
    gfx_object_create(0x58, y, 0x10, 0x33, n, D_208F3C0);
    gfx_object_create(0x64, y, 0x10, 7, n, D_208F3C0);
    func_00009A6();
    T16_CTL5 = 2;
    T16_CR5B = 0x1e8;
    T16_CLKCTL_5 = 0xf;
    T16_CTL5 = 1;
    tmr_flags &= ~0x20;
    INT_EP47_ERTC_EAD = 2;
    AD_EN_SMPL_STAT |= 4;
    AD_EN_SMPL_STAT |= 2;
    do {
        if (tmr_flags & 0x20) {
            tmr_flags &= ~0x20;
            n++;
            if (sys_battery_level <= 0x2e3) {
                gfx_display_reset();
                gfx_object_create(0, y, 0, 0, 0, D_2129614);
                func_00009A6();
                for (;;)
                    asm volatile("halt");
            }
        }
        asm volatile("halt");
        sys_idle_housekeeping();
    } while (n <= 9);
    AD_EN_SMPL_STAT &= ~6;
    INT_EP47_ERTC_EAD = 0;
    gfx_object_create(0x3a, 0x38, 0x10, 9, 0, (const u8 *)&D_2090E0C);
    gfx_object_create(0x31, 0x38, 0x10, sys_battery_level / 0x136 % 10, 0, D_208EDB0);
    gfx_object_create(0x46, 0x38, 0x10, sys_battery_level / 0x1f % 10, 0, D_208EDB0);
    func_00009A6();
    test_clear_buttons();
    asm volatile("halt");
    sys_idle_housekeeping();
}

/*
 * Clear the three button counters.
 */
void test_clear_buttons(void)
{
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
}

/*
 * Wait (halting) until the three buttons P00..P02 are released.
 */
void btn_wait_release(void)
{
    for (;;) {
        if ((P0_P0D & 7) == 7)
            break;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
}

/*
 * Empty function (stub, `ret` only).
 */
void func_251A28A(void)
{
}

/*
 * Test mode: show "SOUND" and a 3-digit number n (sound test screen of test_sound).
 */
void test_sound_draw(int n)
{
    u8 a;

    gfx_display_reset();
    gfx_set_background(0xffff);
    a = gfx_object_create(0, 0, 0x10, 0, 0, 0);
    gfx_draw_text_centered(a, 0x26, 0x10, D_255F6F8, 0, 9);
    gfx_object_create(0x34, 0x4c, 0x20, n / 100 % 10, 0, D_208EDB0);
    gfx_object_create(0x3d, 0x4c, 0x20, n / 10 % 10, 0, D_208EDB0);
    gfx_object_create(0x46, 0x4c, 0x20, n % 10, 0, D_208EDB0);
    func_00009A6();
}

/*
 * Test mode sound test (reached by B, then B+C at the version screen; after it the test
 * mode returns): "SOUND" and the number n (test_sound_draw), polling the P0 pins directly:
 * A alone = n + 1, C alone = n - 1 (both wrap in 0..0x52 and repeat while held), B alone
 * = play sound n + 1 (sound forced on with snd_enabled), hold C and add A = leave.
 */
void test_sound(void)
{
    int dir = 0;
    int n = 0;

    test_clear_buttons();
    test_sound_draw(dir);
    for (;;) {
        if ((P0_P0D & 7) == 7)
            break;
        asm volatile("halt");
        sys_idle_housekeeping();
    }
redraw:
    test_sound_draw(n);
    for (;;) {
        if ((P0_P0D & 7) == 6) {
            if (++n > 0x52)
                n = 0;
            goto redraw;
        }
        if ((P0_P0D & 7) == 5) {
            snd_enabled = 1;
            snd_play(n + 1);
            snd_wait_end();
            snd_enabled = 0;
        }
        if (!(P0_P0D & 4)) {
            if ((P0_P0D & 7) == 3) {
                if (--n < 0)
                    n = 0x52;
                dir = 1;
                goto redraw;
            } else if ((dir == 1) & ((P0_P0D & 7) == 2)) {
                break;
            }
        } else {
            dir = 0;
        }
        test_clear_buttons();
        asm volatile("halt");
        sys_idle_housekeeping();
    }
}



extern const u8 D_255F704[];

/*
 * Test mode RTC test ("COUNT"): resets the RTC to 00:00:00 1 Jan (rtc_reset, so the
 * test mode clears the clock), enables the RTC and A/D interrupts (0x300277 = 6) and the
 * RTC 1 s interrupt (rtc_irq_on). test_rtc_count_mode = 1 makes the RTC handler rtc_isr
 * count seconds in test_rtc_seconds (from -1, up to 100) instead of running the game; the count
 * is shown in two digits. Once it passed 2, A alone ends the test: test_rtc_count_mode = 0, RTC
 * interrupt off again (rtc_irq_off), 0x300277 = 0.
 */
void test_rtc_count(void)
{
    int flag;
    int y = 0x4a;
    u8 a, b;
    u8 k[3];

    gfx_display_reset();
    flag = 0;
    gfx_set_background(0xffff);
    gfx_draw_text_centered(gfx_object_create(flag, flag, 0x10, flag, flag, (const u8 *)flag), 0x26, 0x10, (u8 *)D_255F704, flag, 9);
    a = gfx_object_create(0x34, y, 0x20, flag, flag, (const u8 *)flag);
    b = gfx_object_create(0x40, y, 0x28, flag, flag, (const u8 *)flag);
    func_00009A6();
    rtc_reset();
    INT_EP47_ERTC_EAD = 6;
    rtc_irq_on();
    test_rtc_seconds = -1;
    test_rtc_count_mode = 1;
    for (;;) {
        if ((u8)test_rtc_seconds <= 0x63) {
            gfx_object_set_image(a, 0x20, (s8)(test_rtc_seconds / 10) % 10, D_208EDB0);
            gfx_object_set_image(b, 0x28, test_rtc_seconds % 10, D_208EDB0);
            func_00009A6();
            if (test_rtc_seconds > 2)
                flag = 1;
        }
        if (flag == 1) {
            k[0] = P0_P0D & 7;
            k[1] = P0_P0D & 7;
            k[2] = k[0] | k[1];
            if ((k[2] & 7) == 6)
                break;
        }
        test_clear_buttons();
        asm volatile("halt");
        sys_idle_housekeeping();
    }
    test_rtc_count_mode = 0;
    rtc_irq_off();
    INT_EP47_ERTC_EAD = 0;
}

/* The flash areas of the test-mode flash test test_flash_erase_all: {flags (1 erase, 2 fill,
 * 4 verify, 8 erase again), first sector, last sector, address, fill value, halfwords}. */
const struct FlashTestArea test_flash_areas[7] = {
    { 0xf, 0x01, 0x04, 0x2000000, 0, 0x4000 },
    { 0xf, 0x08, 0x08, 0x200e000, 0, 0x1000 },
    { 0xf, 0x09, 0x0b, 0x2010000, 0, 0x18000 },
    { 0xf, 0x0c, 0x0f, 0x2040000, 0, 0x20000 },
    { 0xf, 0x34, 0x3b, 0x22c0000, 0, 0x40000 },
    { 0xf, 0x62, 0x87, 0x25a0000, 0, 0x130000 },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff },
};
const u8 D_255F6F8[] = _("SOUND");
const u8 D_255F704[] = _("COUNT");

/*
 * The RAM variables this file defines.
 */
u8 test_rtc_count_mode;
s8 test_rtc_seconds;
u16 test_rom_sum_ref = 0x8F12;
