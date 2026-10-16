/*
 * The sound front end: the bank of 83 sounds, playing a sound, waiting for its end, the error
 * beep and the sound initialisation on top of the sequencer library.
 *
 * Code 250DB16..250DC28, .rodata from 255C510.
 */
#include "proto/snd/snd_play.h"
#include "proto/snd/snd_data.h"
#include "proto/ir/ir_session.h"
#include "proto/lib/snd/snd_sequencer.h"
#include "hw.h"

/*
 * End-of-sound callback (registered by snd_init; the sequencer tick snd_sequencer_tick
 * calls it through snd_end_callback, with the voice number, which is ignored, once both
 * voices have reached their end byte): interrupts off (snd_irq_save_disable; reti restores them),
 * stop voices 0 and 1 (snd_voice_stop: P10 / P11 back to GPIO, timers and clocks off), stop
 * the T16 ch.4 tick (CTL4 = 0) and set snd_finished = 1 = "sound finished" (snd_wait_end
 * waits for it).
 */
void snd_end_callback_fn(int voice)
{
    snd_irq_save_disable();
    snd_voice_stop(0);
    snd_voice_stop(1);
    T16_CTL4 = 0;
    snd_finished = 1;
}

/*
 * Sound system init / clock change: pick the sequencer tempo and the note period table for
 * the current clock (snd_sequencer_init(0xE3) = 7.78 ms ticks and snd_period_table_main, tuned for MCLK =
 * 30 MHz; 0x71 and D_24F5394 when pCMU_CLKCNTL bit 12 MCLKDIV halves MCLK), register
 * snd_end_callback_fn as the end-of-sound callback (snd_set_end_callback; the sequencer tick snd_sequencer_tick calls through it), and stop timer 4 (CTL4 = 0). The second
 * branch never runs: MCLKDIV is only set together with the 32 kHz OSC1 clock
 * (sys_sleep_osc1), and this runs at boot (sys_main) and from sys_leave_low_power right after
 * the switch back to 0x770003. D_24F5394 is also tuned for a 9.216 MHz MCLK (C3 = 4403),
 * not 15 MHz: a leftover, like the unused 18.432 MHz table at 0x24F5318.
 */
void snd_init(void)
{
    if ((CMU_CLKCNTL & 0x1000) == 0) {
        snd_sequencer_init(0xe3);
        snd_period_table = snd_period_table_main;
    } else {
        snd_sequencer_init(0x71);
        snd_period_table = D_24F5394;
    }
    snd_set_end_callback(snd_end_callback_fn);
    T16_CTL4 = 0;
}

/*
 * Play sound k (1..0x53) from the sound bank snd_bank: the only entry point to the sound
 * library in the game. snd_finished = 0 (cleared until the end callback snd_end_callback_fn sets
 * it). If sound is on (snd_enabled): run the T16 ch.4 tick (CTL4 = 1) and load the same
 * melody snd_bank[k] with the period table snd_period_table and no transposition on voice 0
 * (mode 0, TM0/P10) and voice 1 (mode 1 = inverted, TM1/P11), then start both with
 * interrupts masked (snd_voice_start): P10 and P11 drive the piezo in antiphase (bridge).
 * Voices 2 and 3 are never used. Sound off: CTL4 = 0 only. Ids seen: 1/2/3 key clicks
 * A/B/C, 4 call / alarm, 0xC error ("not possible"), 0x19 "read OK" fanfare (test and
 * IR screens), 0x37 test-mode tone, 0x3D power-on jingle, 0x4F NFC success jingle; the
 * animations queue others with anim_queue_sound. Full list: notes/findings/sound.md.
 */
void snd_play(u8 k)
{
    int i = k;

    snd_finished = 0;
    if (snd_enabled != 0) {
        T16_CTL4 = 1;
        snd_voice_load(0, snd_period_table, (const u16 *)snd_bank[i], 0, 0);
        snd_voice_load(1, snd_period_table, (const u16 *)snd_bank[i], 0, 1);
        snd_irq_save_disable();
        snd_voice_start(0);
        snd_voice_start(1);
        snd_irq_restore();
    } else {
        T16_CTL4 = 0;
    }
}

/*
 * The sound bank: 84 sequence pointers, entry 0 unused (NULL). Each sequence is u16 0x3302
 * then event bytes ending with 0 (format: snd_sequencer_tick), in D_24F5410 (snd/snd_data,
 * after the third period table). 1..0x42 are short effects and jingles (31 ms .. 3.4 s),
 * 0x43..0x4E and 0x53 tunes of 6 to 32 s.
 */
const u8 *const snd_bank[84] = {
    0,
    D_24F5410 + 0x0,
    D_24F5410 + 0x8,
    D_24F5410 + 0x14,
    D_24F5410 + 0x1c,
    D_24F5410 + 0x30,
    D_24F5410 + 0x44,
    D_24F5410 + 0x60,
    D_24F5410 + 0x70,
    D_24F5410 + 0x94,
    D_24F5410 + 0xc8,
    D_24F5410 + 0xd0,
    D_24F5410 + 0xd8,
    D_24F5410 + 0xe0,
    D_24F5410 + 0xf0,
    D_24F5410 + 0xf8,
    D_24F5410 + 0x108,
    D_24F5410 + 0x11c,
    D_24F5410 + 0x12c,
    D_24F5410 + 0x138,
    D_24F5410 + 0x144,
    D_24F5410 + 0x14c,
    D_24F5410 + 0x154,
    D_24F5410 + 0x15c,
    D_24F5410 + 0x178,
    D_24F5410 + 0x180,
    D_24F5410 + 0x1a0,
    D_24F5410 + 0x1b4,
    D_24F5410 + 0x1fc,
    D_24F5410 + 0x218,
    D_24F5410 + 0x234,
    D_24F5410 + 0x23c,
    D_24F5410 + 0x250,
    D_24F5410 + 0x284,
    D_24F5410 + 0x2bc,
    D_24F5410 + 0x2c4,
    D_24F5410 + 0x2d4,
    D_24F5410 + 0x2e4,
    D_24F5410 + 0x300,
    D_24F5410 + 0x310,
    D_24F5410 + 0x328,
    D_24F5410 + 0x334,
    D_24F5410 + 0x34c,
    D_24F5410 + 0x364,
    D_24F5410 + 0x378,
    D_24F5410 + 0x3a4,
    D_24F5410 + 0x3b4,
    D_24F5410 + 0x3d4,
    D_24F5410 + 0x3f4,
    D_24F5410 + 0x408,
    D_24F5410 + 0x418,
    D_24F5410 + 0x420,
    D_24F5410 + 0x428,
    D_24F5410 + 0x434,
    D_24F5410 + 0x450,
    D_24F5410 + 0x460,
    D_24F5410 + 0x468,
    D_24F5410 + 0x48c,
    D_24F5410 + 0x4a4,
    D_24F5410 + 0x4bc,
    D_24F5410 + 0x4c4,
    D_24F5410 + 0x4d4,
    D_24F5410 + 0x4ec,
    D_24F5410 + 0x500,
    D_24F5410 + 0x518,
    D_24F5410 + 0x544,
    D_24F5410 + 0x588,
    D_24F5410 + 0x5dc,
    D_24F5410 + 0x66c,
    D_24F5410 + 0x6b8,
    D_24F5410 + 0x708,
    D_24F5410 + 0x800,
    D_24F5410 + 0x878,
    D_24F5410 + 0x8e8,
    D_24F5410 + 0x994,
    D_24F5410 + 0xb2c,
    D_24F5410 + 0xb8c,
    D_24F5410 + 0xbec,
    D_24F5410 + 0xc2c,
    D_24F5410 + 0xcc4,
    D_24F5410 + 0xcd0,
    D_24F5410 + 0xce4,
    D_24F5410 + 0xd00,
    D_24F5410 + 0xd1c,
};

/*
 * Wait until the sound currently playing has finished (snd_finished, set by the
 * end-of-melody callback snd_end_callback_fn); returns at once when sound is off (snd_enabled ==
 * 0). Busy loop: the sound runs from the T16 ch.4 interrupt.
 */
void snd_wait_end(void)
{
    if (snd_enabled)
        while (snd_finished == 0)
            ;
}

/*
 * "Not possible" sound (12) and wait.
 */
void snd_error_beep(void)
{
    snd_play(0xc);
    snd_wait_end();
}

volatile u32 snd_finished DSTRAM;
const u16 *snd_period_table DSTRAM;
