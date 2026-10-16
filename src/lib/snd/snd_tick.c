/*
 * Sound library: the sequencer tick timer (T16 ch.4).
 *
 * Code 253520C..25352A2, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/snd/snd_tick.h"
#include "proto/lib/snd/snd_sequencer.h"
#include "hw.h"

/*
 * Sound: start the sequencer tick on T16 ch.4: clock MCLK/1024 (pT16_CLKCTL_4 = 14),
 * compare B = v (the tick period: v + 1 counts; v = 0xE3 from snd_init gives 228 /
 * 29297 Hz = 7.78 ms), compare A = 0, timer reset, compare-B routed to the interrupt (not
 * IDMA), timer-4 interrupt priority 3 (pINT_P16T45), flags cleared, only E16TU4 enabled,
 * run. The ISR (vector 46, snd_tick_isr) calls snd_sequencer_tick = one sequencer tick, which
 * plays the voices on tone channels T16 ch.0/1 (the game uses only these two; see
 * notes/findings/sound.md). snd_irq_save_disable / snd_irq_restore mask interrupts around the setup.
 * Called by snd_sequencer_init; the game then starts and stops the tick through CTL4 alone
 * (snd_play / snd_end_callback_fn).
 */
void snd_tick_start(u16 v)
{
    int t;

    snd_irq_save_disable();
    T16_CLKCTL_4_B0 = 14;
    T16_CR4B = v;
    T16_TC4 = 0;
    T16_CTL4_B0 = 2;
    IDMAREQ_R16T14 &= ~0x40;
    t = INT_P16T45;
    INT_P16T45 = (t & 0xf0) | 3;
    INT_F16T45 = 12;
    INT_E16T45 &= ~0xf;
    INT_E16T45 |= 4;
    T16_CTL4_B0 = 2;
    T16_CTL4_B0 |= 1;
    snd_irq_restore();
}

/*
 * Sound: stop the sequencer tick (T16 ch.4): run bit off, its interrupts disabled and
 * flags cleared, clock off (with interrupts masked by snd_irq_save_disable/func_25356D6 = save PSR
 * and clear / restore). Only called by snd_tick_stop, which nothing calls.
 */
void snd_tick_timer_stop(void)
{
    snd_irq_save_disable();
    T16_CTL4_B0 &= ~1;
    INT_E16T45 &= ~0xf;
    INT_F16T45 = 12;
    T16_CLKCTL_4_B0 &= ~8;
    snd_irq_restore();
}

/*
 * Sound: acknowledge the T16 ch.4 interrupt flags (pINT_F16T45: F16TU4 | F16TC4, write 1
 * to clear). First thing the sequencer tick snd_sequencer_tick does.
 */
void snd_tick_ack(void)
{
    INT_F16T45 = 0xc;
}
