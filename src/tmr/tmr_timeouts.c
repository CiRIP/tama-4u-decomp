/*
 * Millisecond timers and timeouts on T16 ch.3 / ch.5, delays, and the LCD backlight level
 * (requested and applied).
 *
 * Code 251A566..251A650, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/tmr/tmr_timeouts.h"
#include "hw.h"

/*
 * Start a one-shot timeout of t milliseconds on T16 ch.3 (MCLK/4096 = 7324 Hz, so t * 30000
 * / 4096 counts; t * 15000 when MCLK is halved). The compare-B interrupt (vector 42,
 * tmr_t16_23_isr) sets tmr_flags bit 2. Used by tmr_delay_ms (delay).
 */
void tmr_t3_start_timeout(u16 t)
{
    T16_CTL3 = 2;
    if (!(CMU_CLKCNTL & 0x1000))
        T16_CR3B = (t * 30000) >> 12;
    else
        T16_CR3B = (t * 15000) >> 12;
    T16_CLKCTL_3 = 15;
    tmr_flags &= ~4;
    T16_CTL3 = 1;
}

/*
 * One-shot delay on T16 ch.5: t milliseconds (compare B = t * 30000 >> 12 = t * 7.32 counts
 * at MCLK/4096 = 7.3 kHz; half when MCLK is divided by 2, pCMU_CLKCNTL bit 12; same
 * arithmetic as tmr_t3_start_timeout). Clears tmr_flags bit 5, which the compare-B interrupt
 * (vector 50, tmr_t16_45_isr) sets when the time is up; the caller polls it (tmr_t5_wait_100ms
 * waits n x 100 ms this way). tmr_t3_start_timeout is the same on timer 3 (flag bit 2).
 */
void tmr_t5_start_timeout(u16 t)
{
    T16_CTL5 = 2;
    if (!(CMU_CLKCNTL & 0x1000))
        T16_CR5B = (t * 30000) >> 12;
    else
        T16_CR5B = (t * 15000) >> 12;
    T16_CLKCTL_5 = 15;
    tmr_flags &= ~0x20;
    T16_CTL5 = 1;
}

void tmr_delay_ms(u16 ms)
{
    tmr_t3_start_timeout(ms);
    while (!(tmr_flags & 4))
        asm volatile("halt");
}

void tmr_delay_ms_t5(u16 ms)
{
    tmr_t5_start_timeout(ms);
    while (!(tmr_flags & 0x20))
        asm volatile("halt");
}

/*
 * Set the LCD backlight level (applied by sys_apply_backlight).
 */
void sys_set_backlight(int a)
{
    sys_backlight_request = a;
}

/*
 * LCD backlight PWM update, called from the idle loop: when the requested level
 * sys_backlight_request (0..8) differs from the one last applied (sys_backlight_applied), program T16 ch.2 as PWM
 * on TM2/P12: period k = 0x44 (OSC3 clock) or 0xF4 (OSC1), compare A = k * (9 - level) / 10
 * (duty; higher level = longer on time if the output is active low), clock control 0xA
 * (MCLK/4) or 0xD (MCLK/256 by the prescaler table that gives 0xB = /16 and 0xE = /1024);
 * level 0 = timer reset and clock off (backlight off). sys_set_backlight(level) requests a
 * level; sys_backlight_level = the user's brightness setting.
 */
void sys_apply_backlight(void)
{
    int k;

    if (sys_backlight_applied != sys_backlight_request) {
        sys_backlight_applied = sys_backlight_request;
        if (sys_backlight_request != 0) {
            k = !(CMU_CLKCNTL & 2) ? 0x44 : 0xf4;
            T16_CR2A = k * (9 - sys_backlight_request) / 10;
            T16_CR2B = k;
            if (!(CMU_CLKCNTL & 2))
                T16_CLKCTL_2 = 0xa;
            else
                T16_CLKCTL_2 = 0xd;
            T16_CTL2 = 0x25;
        } else {
            T16_CTL2 = 2;
            T16_CLKCTL_2 = 7;
        }
    }
}

/*
 * The RAM variables this file defines.
 */
volatile u8 tmr_flags;
u8 sys_backlight_request;
u8 sys_backlight_applied;
