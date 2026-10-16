/*
 * Sound library: tone channel restart.
 *
 * Code 25355E6..2535610, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/snd/snd_ch_restart.h"
#include "hw.h"

/*
 * Sound channel 0: restart the timer (PRESET|PRUN), used when a voice starts
 * (snd_voice_start via snd_channel_restart). (snd_restart_ch1/2535602/2535610: ch. 1..3.)
 */
void snd_restart_ch0(void)
{
    T16_CTL0_B0 |= 3;
}

/*
 * Sound channel 1: restart its timer.
 */
void snd_restart_ch1(void)
{
    T16_CTL1_B0 |= 3;
}

/*
 * Sound channel 2: restart its timer.
 */
void snd_restart_ch2(void)
{
    T16_CTL2_B0 |= 3;
}

/*
 * Sound channel 3: restart its timer.
 */
void snd_restart_ch3(void)
{
    T16_CTL3_B0 |= 3;
}
