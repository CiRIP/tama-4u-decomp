/*
 * Sound library: tone channel period.
 *
 * Code 2535542..253558A, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/snd/snd_ch_pitch.h"
#include "hw.h"

/*
 * Change the pitch of tone channel 0 while it runs: timer 0 compare B = a (period - 1),
 * compare A = b (duty) of the TM0 output. The sequencer writes P - 1 and P/2 - 1 for a
 * note of period P (f = 1.875 MHz / P), or 1000 / 2000 for silence. Same for the other
 * channels: snd_set_pitch_1 (1), snd_set_pitch_2 (2), snd_set_pitch_3 (3).
 */
void snd_set_pitch_0(u16 a, u16 b)
{
    T16_CR0B = a;
    T16_CR0A = b;
}

/*
 * Sound channel 1: set period/duty.
 */
void snd_set_pitch_1(u16 a, u16 b)
{
    T16_CR1B = a;
    T16_CR1A = b;
}

/*
 * Sound channel 2: set period/duty.
 */
void snd_set_pitch_2(u16 a, u16 b)
{
    T16_CR2B = a;
    T16_CR2A = b;
}

/*
 * Sound channel 3: set period/duty.
 */
void snd_set_pitch_3(u16 a, u16 b)
{
    T16_CR3B = a;
    T16_CR3A = b;
}

/* Sequencer: set the pitch (period a, duty b) of tone channel sel (0..3), if the channel
 * is enabled (snd_voice_state). */
/*
 * Sound channel ch: set period/duty if voice sel is loaded (snd_voice_state(sel) != 0; the
 * original passed no argument and so read the voice still in %r6, sel).
 */
void snd_set_pitch(int sel, u16 a, u16 b)
{
    if (snd_voice_state(sel) == 0)
        return;
    switch (sel) {
    case 0:
        snd_set_pitch_0(a, b);
        break;
    case 1:
        snd_set_pitch_1(a, b);
        break;
    case 2:
        snd_set_pitch_2(a, b);
        break;
    case 3:
        snd_set_pitch_3(a, b);
        break;
    }
}
