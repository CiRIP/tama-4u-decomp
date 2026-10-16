/*
 * Sound library: tone channel on.
 *
 * Code 25352AE..253540A, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/snd/snd_ch_on.h"
#include "proto/lib/snd/snd_sequencer.h"
#include "hw.h"

#define REG8(a) (*(volatile u8 *)(a))
#define REG16(a) (*(volatile u16 *)(a))
/*
 * Tone channel 0 on 16-bit timer 0, output TM0 = pin P10 (one terminal of the piezo):
 * route P10 to the timer (pP1_03_CFP bits 1:0 = 01), clock MCLK/16 = 1.875 MHz, compare
 * B = a (period - 1), compare A = b (high from A+1 to B, so a 50 % square wave when b = a
 * / 2), control 0x34 (PTM output on + OUTINV + SELCRB buffered compare; no fine mode) when
 * mode == 1, else 0x24 (the same without the inversion), then PRESET (output to its
 * initial level, counter 0; snd_voice_start sets PRUN later). a = 1000, b = 2000 (A > B: no
 * compare-A match, the output stays at its off level) is the silence the library uses.
 * The game loads voice 0 with mode 0 here and voice 1 (snd_ch1_on, TM1/P11) with mode 1
 * and the same melody: P10 and P11 carry complementary square waves, a bridge drive of a
 * piezo between the two pins. Note periods: the table snd_period_table_main
 * (snd/snd_data). Siblings snd_ch2_on (timer 2 / P12) and snd_ch3_on (timer 3 / P13) are never
 * used for sound: TM2 is the LCD backlight PWM, P13 a GPIO of the NFC front-end.
 */
void snd_ch0_on(u16 a, u16 b, int mode)
{
    snd_irq_save_disable();
    P1_03_CFP &= ~3;
    P1_03_CFP |= 1;
    T16_CLKCTL_0_B0 = 11;
    T16_CR0B = a;
    T16_CR0A = b;
    if (mode == 1)
        T16_CTL0_B0 = 0x34;
    else
        T16_CTL0_B0 = 0x24;
    T16_CTL0_B0 |= 2;
    snd_irq_restore();
}

/*
 * Sound channel 1 on: P11 = TM1, T16 ch.1 (see snd_ch0_on). The game always loads it
 * with mode 1 (inverted output) and the same melody as channel 0: TM1 is the inverse of
 * TM0, the other side of the piezo.
 */
void snd_ch1_on(u16 a, u16 b, int mode)
{
    snd_irq_save_disable();
    P1_03_CFP &= ~0xc;
    P1_03_CFP |= 4;
    T16_CLKCTL_1_B0 = 11;
    T16_CR1B = a;
    T16_CR1A = b;
    if (mode == 1)
        T16_CTL1_B0 = 0x34;
    else
        T16_CTL1_B0 = 0x24;
    T16_CTL1_B0 |= 2;
    snd_irq_restore();
}

/*
 * Sound channel 2 on: P12 = TM2, T16 ch.2 (see snd_ch0_on). Library code that is never
 * reached: the game only loads voices 0 and 1, and T16 ch.2 / TM2 is the LCD backlight
 * PWM (sys_apply_backlight).
 */
void snd_ch2_on(u16 a, u16 b, int mode)
{
    snd_irq_save_disable();
    P1_03_CFP &= ~0x30;
    P1_03_CFP |= 0x10;
    T16_CLKCTL_2_B0 = 11;
    T16_CR2B = a;
    T16_CR2A = b;
    if (mode == 1)
        T16_CTL2_B0 = 0x34;
    else
        T16_CTL2_B0 = 0x24;
    T16_CTL2_B0 |= 2;
    snd_irq_restore();
}

/*
 * Sound channel 3 on: P13 = TM3, T16 ch.3 (see snd_ch0_on). Library code that is never
 * reached: the game only loads voices 0 and 1. T16 ch.3 is the NFC 100 ms tick
 * (nfc_tick_start) and the ms timeout (tmr_t3_start_timeout), and P13 is a GPIO of the NFC
 * front-end power-up (nfc_hw_power_on).
 */
void snd_ch3_on(u16 a, u16 b, int mode)
{
    snd_irq_save_disable();
    P1_03_CFP &= 0x3f;
    P1_03_CFP |= 0x40;
    T16_CLKCTL_3_B0 = 11;
    T16_CR3B = a;
    T16_CR3A = b;
    if (mode == 1)
        T16_CTL3_B0 = 0x34;
    else
        T16_CTL3_B0 = 0x24;
    T16_CTL3_B0 |= 2;
    snd_irq_restore();
}

/*
 * Sound channel ch (0..3) on with compare B a, compare A b, mode (1 = inverted output).
 * Called by snd_voice_load with a = 1000, b = 2000 (silent until the first note).
 */
void snd_ch_on(int ch, u16 a, u16 b, int mode)
{
    switch (ch) {
    case 0:
        snd_ch0_on(a, b, mode);
        break;
    case 1:
        snd_ch1_on(a, b, mode);
        break;
    case 2:
        snd_ch2_on(a, b, mode);
        break;
    case 3:
        snd_ch3_on(a, b, mode);
        break;
    }
}
