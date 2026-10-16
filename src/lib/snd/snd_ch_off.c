/*
 * Sound library: tone channel off.
 *
 * Code 253545E..253550A, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/snd/snd_ch_off.h"
#include "proto/lib/snd/snd_sequencer.h"
#include "hw.h"

/*
 * Sound channel 0 off: P10 back to GPIO (an output driven low since sys_gpio_init), timer 0
 * control cleared (stopped, TM0 output off), its clock off. (snd_ch1_off/25354B2/25354DE:
 * channels 1..3; snd_ch3_off has a bug.)
 */
void snd_ch0_off(void)
{
    snd_irq_save_disable();
    P1_03_CFP &= ~3;
    T16_CTL0_B0 &= ~0xff;
    T16_CLKCTL_0_B0 &= ~8;
    snd_irq_restore();
}

/*
 * Sound channel 1 off.
 */
void snd_ch1_off(void)
{
    snd_irq_save_disable();
    P1_03_CFP &= ~0xc;
    T16_CTL1_B0 &= ~0xff;
    T16_CLKCTL_1_B0 &= ~8;
    snd_irq_restore();
}

/*
 * Sound channel 2 off.
 */
void snd_ch2_off(void)
{
    snd_irq_save_disable();
    P1_03_CFP &= ~0x30;
    T16_CTL2_B0 &= ~0xff;
    T16_CLKCTL_2_B0 &= ~8;
    snd_irq_restore();
}

/*
 * Sound channel 3 off. Firmware bug: it clears the control register of T16 ch.1
 * (0x30078E) instead of ch.3 (0x30079E), so timer 3 keeps its run bit while its clock is
 * turned off. Never reached: voice 3 is never used.
 */
void snd_ch3_off(void)
{
    snd_irq_save_disable();
    P1_03_CFP &= 0x3f;
    T16_CTL1_B0 &= ~0xff;
    T16_CLKCTL_3_B0 &= ~8;
    snd_irq_restore();
}

/*
 * Sound channel ch (0..3) off.
 */
void snd_ch_off(int ch)
{
    switch (ch) {
    case 0:
        snd_ch0_off();
        break;
    case 1:
        snd_ch1_off();
        break;
    case 2:
        snd_ch2_off();
        break;
    case 3:
        snd_ch3_off();
        break;
    }
}
