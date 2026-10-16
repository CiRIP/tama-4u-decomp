/*
 * Low-power mode: sleep on OSC1, resume on OSC3, leaving low power (sound off, clock back to 30
 * MHz) and a small helper.
 *
 * Code 24F714C..24F71D8, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/sys/sys_low_power.h"
#include "proto/snd/snd_play.h"
#include "hw.h"

void sys_sleep_osc1(void)
{
    CMU_PROTECT = 0x96;
    CMU_CLKCNTL = 0x771007;
    CMU_OPT = 0;
    asm volatile("slp");
    CMU_CLKCNTL = 0x771005;
    CMU_OPT = 1;
    CMU_PROTECT = 0;
}

void sys_sleep_osc3(void)
{
    CMU_PROTECT = 0x96;
    CMU_CLKCNTL = 0x770003;
    CMU_OPT = 0x5c00;
    asm volatile("slp");
    CMU_OPT = 1;
    CMU_PROTECT = 0;
}

/*
 * Leave low-power mode: wait for the current sound to finish (snd_wait_end), put the CMU
 * back on the OSC3 high-speed clock (pCMU_CLKCNTL = 0x770003 under the 0x96 protect key),
 * then redo the sound setup for that clock (snd_init: tempo, note table, callback).
 * Counterpart of sys_sleep_osc1 (asm: switch to OSC1 and slp).
 */
void sys_leave_low_power(void)
{
    snd_wait_end();
    CMU_PROTECT = 0x96;
    CMU_CLKCNTL = 0x770003;
    CMU_PROTECT = 0;
    snd_init();
}

void func_24F71D8(void)
{
}
