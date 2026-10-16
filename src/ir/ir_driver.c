/*
 * The IR port driver: carrier, edge interrupt, timer 5 ticks, packet send / receive with
 * checksum, button history and critical sections.
 *
 * Code 2504844..25051CE, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/ir/ir_driver.h"
#include "proto/ir/ir_session.h"
#include "proto/sys/sys_interrupts.h"
#include "proto/ui/ui_menu.h"
#include "proto/snd/snd_play.h"
#include "proto/spot/spot_reader.h"
#include "hw.h"

/*
 * Unused (no caller, so not a post-session re-init): IR port setup for an
 * interrupt-driven receiver: timer-5 ISR vector back to the default (sys_copy_ram_routines),
 * counters sys_trap_state/D_0000D55 cleared, P0 pull-ups, P03/P05 outputs, P04/P05 back to
 * plain GPIO (pP0_47_CFP = 0: the PN512 UART pins released), P04 as input with the
 * port-input-4 interrupt (polarity/edge as in sys_gpio_init, priority 4), all port
 * 4-7/RTC/AD cause flags cleared. It is the only writer of pINT_PP45L, so without it the
 * port-input-4 priority keeps its undefined reset value. The vector-68 handler it would need
 * (ir_edge_isr) is not in the vector table either: the receive side is incomplete.
 */
void ir_port_setup(void)
{
    sys_copy_ram_routines();
    sys_trap_state = 0;
    sys_trap_last = 0;
    MISC_PUP0 = 0xef;
    P0_IOC0 = 0x28;
    P0_P0D = 0xdf;
    P0_47_CFP = 0;
    PINTPOL_SPP07 = 0;
    PINTEL_SEPT07 = 0x17;
    INT_FP47_FRTC_FAD = 0xff;
    INT_PP45L = 4;
    ir_session_busy = 0;
}

/*
 * IR receive edge counter, meant for the port-input-4 interrupt (P04 falling edge): unless
 * a session-level transfer is running (ir_session_busy == 1), count the edge in ir_edge_count2 (edges
 * in the current burst) and ir_edge_count (edges since the last poll), clear the FP4 flag.
 * Unreferenced: vector 68 (port input 4) is the halt stub sys_isr_halt_a, so the IR receive
 * path (ir_detect_start / ir_decode_bytes) never sees an edge on the 4U+.
 */
void __attribute__((interrupt_handler)) ir_edge_isr(void)
{
    if (ir_session_busy != 1) {
        ir_edge_count2++;
        ir_edge_count++;
        INT_FP47_FRTC_FAD = 8;
    }
}

/*
 * Start T16 ch.5 as a periodic tick: reset, compare A = a - 1, compare B = a (period a + 1
 * counts: the counter resets on compare B), clock control b | 8 (on, prescaler b: 0..7 =
 * MCLK/1, 2, 4, 16, 64, 256, 1024, 4096), timer-5 compare-A interrupt enabled
 * (level kept, bit 7 of pINT_E16T45), flag cleared, run. Interrupts are masked around the
 * setup (sys_irq_save_disable saves the PSR and clears it, sys_irq_restore restores it with IE). The
 * compare-A interrupt (vector 51) goes through the RAM vector ir_ram_routines (default handler
 * at 0xD94: count tmr_t5_ticks). tmr_t5_start_raw is the same without the "clock on" bit forced.
 * Used by the IR link of the earlier models (dead code on the 4U+).
 */
void tmr_t5_start_tick(u16 a, u8 b)
{
    int x;

    sys_irq_save_disable();
    T16_CTL5 = 2;
    T16_CR5A = a - 1;
    T16_CR5B = a;
    T16_CLKCTL_5 = b | 8;
    x = INT_E16T45 & 0xf;
    x |= 0x80;
    INT_E16T45 = x;
    INT_F16T45 = 0x80;
    T16_CTL5 = 1;
    sys_irq_restore();
}

/*
 * Wait one T16 ch.5 period: start timer 5 with period a and prescaler b (tmr_t5_start_tick, with
 * the clock-on bit 8 added) and spin until the compare-A tick handler increments
 * tmr_t5_ticks. ir_count_low does the same while counting how many polls saw P04 low.
 */
void tmr_t5_wait_period(u16 a, u8 b)
{
    tmr_t5_ticks = 0;
    tmr_t5_start_tick(a, b);
    while (tmr_t5_ticks == 0)
        ;
    tmr_t5_ticks = 0;
}

/*
 * Unused: counts how many timer ticks P04 (IR receiver) is low during one timer period
 * (a, b = timer 5 period and prescaler): a carrier-detect helper.
 */
int ir_count_low(u16 a, u8 b)
{
    int n = 0;

    tmr_t5_ticks = n;
    tmr_t5_start_tick(a, b);
    while (tmr_t5_ticks == 0)
        n += ~P0_P0D & 0x10;
    n >>= 4;
    return n;
}

/*
 * Switch the system clock back to the 30 MHz high-speed oscillator OSC3 (pCMU_CLKCNTL =
 * 0x770003: OSCSEL = OSC3, OSC3DIV = 1/1, OSC1 and OSC3 on, PLL/LCDC dividers at their
 * defaults; the CMU registers are write-protected, 0x96 unlocks), so MCLK = 30 MHz for the
 * IR timings, and set ir_port_ptr =
 * 0x300380, the P0 data register: the port driven by the IR carrier routine ir_carrier_burst
 * run from RAM and sampled by ir_decode_bytes.
 */
void sys_clock_osc3(void)
{
    CMU_PROTECT = 0x96;
    CMU_CLKCNTL = 0x770003;
    CMU_PROTECT = 0;
    ir_port_ptr = 0x300380;
}

/*
 * IR: P04-P07 back to GPIO (pP0_47_CFP, 0x3003A1 = 0; P04/P05 are otherwise the PN512
 * UART), ir_session_busy = 0; if P03 is high (transceiver shut down, P03 = its active-high SD
 * pin: inferred, medium-high), drive P0 = 0 (P03 low = power up, P05 = LED off) and wait
 * one T16 ch.5 period of 0x3A x MCLK/64 = 124 us (tmr_t5_wait_period(0x39, 4)) for it to wake.
 */
void ir_power_up(void)
{
    ir_session_busy = 0;
    P0_47_CFP = 0;
    if (P0_P0D & 8) {
        P0_P0D = 0;
        tmr_t5_wait_period(0x39, 4);
    }
}

/*
 * IR link off: P0 data = 0x18 (P03 high = transceiver shut down, P05 low = LED off; P04 is
 * an input), clear the port 4-7 flags, disable the port-input-4 (P04) interrupt, reset the
 * edge counters ir_edge_count/D_0000DB8. So it idles the IR hardware (P03 as the active-high
 * shutdown pin: inferred, medium-high; boot also leaves P03 high). Called at the end of
 * every communication menu action (F7), including the live NFC ones, and at the start of
 * each transmit (ir_send_packet / ir_send_ping). Note it writes the whole P0 byte (P07 = 0).
 */
void ir_power_down(void)
{
    ir_session_busy = 0;
    P0_P0D = 0x18;
    INT_FP47_FRTC_FAD = 0;
    INT_EP47_ERTC_EAD &= ~8;
    ir_edge_count = 0;
    ir_edge_count2 = 0;
}

/*
 * IR (the old exchange of the earlier models): start receiving on the bit-banged input:
 * ports to GPIO and transceiver powered up (ir_power_up), ir_xfer_started = 0, enable the
 * port-input-4 interrupt (P04 falling edge, pINT_EP47 bit 3; vector 68 is the halt stub
 * sys_isr_halt_a in this firmware, not the edge counter ir_edge_isr, and its priority
 * pINT_PP45L is only set by the unused ir_port_setup: an edge would hang the toy), wait one
 * timer-5 period of 0x74 x MCLK/16 = 62 us, clear the edge counters ir_edge_count / ir_edge_count2.
 */
void ir_rx_start(void)
{
    ir_power_up();
    ir_xfer_started = 0;
    INT_EP47_ERTC_EAD |= 8;
    tmr_t5_wait_period(0x73, 3);
    ir_edge_count = 0;
    ir_edge_count2 = 0;
}

/*
 * Hand-written assembly (not gcc output): IR start-burst detector, polled on the T16 ch.5
 * sample tick tmr_t5_ticks (tmr_t5_start_tick(0x73, 3) = 61.9 us) together with the two counters of
 * the P04 falling-edge ISR ir_edge_isr: ir_edge_count = edges since the last check ("carrier
 * in this tick"), ir_edge_count2 = edges in the burst. Result in ir_rx_status (and returned):
 * 1 = no edge within 500 ticks (31 ms), 0x11 = burst longer than 98 ticks, 0x10 = more than
 * 0x171 edges, 2 = burst (ended by 3 silent ticks) with <= 0x168 edges, 3 = no edge within
 * about 24 ticks after it (the 1.5 ms start space), 0 = start mark of 368 carrier pulses
 * (5.9 ms) seen. Counting single edges needs a receiver that outputs every carrier pulse
 * (IrDA-type transceiver), not a demodulating one. On the 4U+ ir_edge_isr is not in the
 * vector table, so the counters never move (see notes/findings/ir.md).
 */
asm(".include \"asm/func/func_2504A48.s\"");

/*
 * Hand-written assembly: decode the 20 bytes after the start mark into buffer r6. Per bit:
 * a mark of 14..24 edges within 8 ticks (sent: 18 pulses = 290 us), ended by 3 silent
 * ticks, else ir_result = 5; then the silent ticks up to the next edge: > 10 (619 us) =
 * 1, else 0 (sent: 823 / 452 us), > 24 = error 6. Bits come LSB first (shifted in at bit
 * 7). After 20 bytes ir_result = 0x14 (returned).
 */
asm(".include \"asm/func/func_2504B64.s\"");

/*
 * IR RECEIVE of one 20-byte packet into buffer a (bytes 0..0x12 + checksum byte 0x13):
 * the old IR exchange of the earlier models (part of the group 2504A14..2505112; the
 * edge-counting ISR ir_edge_isr it depends on is not in the vector table of this
 * firmware, so this path cannot work on the 4U+: dead code, confidence medium; see
 * notes/findings/ir.md). RTC interrupt masked meanwhile (pRTCINTMODE 6 = 1 s, RTCIEN 0).
 * ir_xfer_started = 0: plain start with timer 5 sampling every 116 x MCLK/16 = 61.9 us
 * (tmr_t5_start_tick(0x73, 3)); otherwise (after a transmit shut the transceiver down) power
 * it up (ir_rx_start) first. Waits (up to 10 tries of ir_detect_start, each up to 31 ms:
 * ir_edge_count = carrier edges in the last tick, ir_edge_count2 = edges in the burst) for the
 * start mark while sampling the buttons (btn_history_sample; abort if button C (P02) is held);
 * then ir_decode_bytes decodes the bits from the space lengths (short = 0, long = 1, LSB
 * first, 20 bytes, ir_result = 0x14 or error 5/6) and the last byte must equal the 8-bit
 * sum of the first 19 (ir_checksum). Error codes in ir_result (set only if it is still 0;
 * not cleared here): 0x10 = no start burst / aborted, 0x11 = wrong length, 0x12 = checksum
 * mismatch. Restores the RTC interrupt mode (7) and stops timer 5. Returns ir_result
 * (0x14 = ok). The tmr_t5_start_raw(0x2E5, 8) the callers issue first is overridden here.
 */
int ir_recv_packet(int a)
{
    int i;
    int r;

    RTCINTMODE = 6;
    i = ir_xfer_started;
    if (i == 0) {
        tmr_t5_start_tick(0x73, 3);
        do {
            ir_xfer_failed = ir_detect_start();
            btn_history_sample();
            if (ir_xfer_failed == 0)
                break;
            if ((btn_samples[2] & 3) == 3)
                break;
            i++;
        } while (i <= 9);
    } else {
        ir_rx_start();
        i = 0;
        tmr_t5_start_tick(0x73, 3);
        do {
            ir_xfer_failed = ir_detect_start();
            btn_history_sample();
            if (ir_xfer_failed == 0)
                break;
            if ((btn_samples[2] & 3) == 3)
                break;
            i++;
        } while (i <= 9);
    }
    if (ir_xfer_failed != 0) {
        ir_stop(4);
        if (ir_result == 0)
            ir_result = 0x10;
    } else {
        r = ir_decode_bytes(a);
        ir_xfer_failed = r;
        if (r != 0x14) {
            ir_stop(4);
            if (ir_result == 0)
                ir_result = 0x11;
        } else if (((u8 *)a)[0x13] != ir_checksum((u8 *)a)) {
            ir_stop(4);
            if (ir_result == 0)
                ir_result = 0x12;
        }
    }
    T16_CTL5 = 0;
    T16_CR5A = 0;
    RTCINTMODE = 7;
    return ir_result;
}

/*
 * IR TRANSMIT of one 20-byte packet a (legacy IR link, dead on the 4U+): byte 19 = the
 * 8-bit sum ir_checksum; RTC interrupt masked (pRTCINTMODE 6); T16 ch.5 ticking every
 * 242 MCLK = 8.07 us (tmr_t5_start_raw(0xF1, 8)); the RAM carrier routine at ir_carrier_ram
 * (ir_carrier_burst: r6 pulses of 1 tick on / 1 tick off on P05 = 62 kHz) sends: start mark
 * 0x170 pulses (5.94 ms), space until tmr_t5_ticks > 0xB8 (1.5 ms), then 160 bits LSB first,
 * each an 0x12-pulse mark (290 us) and a space of 0x37 (bit 0, 452 us) or 0x65 (bit 1,
 * 823 us) ticks, then a 0x2E-pulse stop mark (742 us) that ends the last space. About
 * 130-190 ms per packet. Afterwards timer 5 stops, RTC mode 7, P0 = 0x1F (P03 high =
 * transceiver shut down, P05 = LED off); ir_xfer_started = 1 makes the next receive
 * (ir_recv_packet) power the transceiver up first.
 */
void ir_send_packet(int a)
{
    ir_tx_ptr = a;
    *(volatile u32 *)&ir_xfer_started = 1;
    ir_power_down();
    ((u8 *)a)[0x13] = ir_checksum((u8 *)a);
    RTCINTMODE = 6;
    tmr_t5_start_raw(0xf1, 8);
    asm volatile("pushn %r12\n\txld.w %r4,ir_tx_ptr\n\tld.w %r10,[%r4]\n\txld.w %r6,0x170\n\txld.w %r7,ir_carrier_ram\n\tcall %r7\n\txld.w %r6,0xb8\n\tld.w %r1,0x0\n\txld.w %r4,tmr_t5_ticks\n\tld.w [%r4],%r1\n"
                 "1:\n\tld.w %r5,[%r4]\n\tcmp %r5,%r6\n\tjrule 1b\n\txld.w %r11,0x100\n\tld.w %r12,0x14\n\tld.w %r2,0x0\n\tld.w %r3,0x1\n"
                 "2:\n\tld.w %r6,0x12\n\txld.w %r7,ir_carrier_ram\n\tcall %r7\n\txld.w %r6,0x64\n\tld.w %r5,%r10\n\tadd %r5,%r2\n\tld.ub %r4,[%r5]\n\tand %r4,%r3\n\tjrne 3f\n\txld.w %r6,0x36\n"
                 "3:\n\txld.w %r4,tmr_t5_ticks\n"
                 "4:\n\tld.w %r5,[%r4]\n\tcmp %r5,%r6\n\tjrule 4b\n\tnop\n\tsll %r3,0x1\n\tcmp %r3,%r11\n\tjrne 2b\n\tnop\n\tld.w %r3,0x1\n\tadd %r2,%r3\n\tcmp %r2,%r12\n\tjrne 2b\n\tnop\n\txld.w %r6,0x2e\n\txld.w %r7,ir_carrier_ram\n\tcall %r7\n\tpopn %r12");
    T16_CTL5 = 0;
    T16_CR5A = 0;
    RTCINTMODE = 7;
    P0_P0D = 0x1f;
}

/*
 * IR "ping": the same set-up as ir_send_packet (the checksum is computed but unused) and a
 * single burst of 10 carrier pulses (161 us). The initiating side of the session drivers
 * sends it when B is pressed; the peer sees it as edges in ir_edge_count and becomes the
 * responder (ir_session_run, ir_test_session).
 */
void ir_send_ping(int a)
{
    ir_tx_ptr = a;
    *(volatile u32 *)&ir_xfer_started = 1;
    ir_power_down();
    ((u8 *)a)[0x13] = ir_checksum((u8 *)a);
    RTCINTMODE = 6;
    tmr_t5_start_raw(0xf1, 8);
    asm volatile("pushn %r12\n\txld.w %r4,ir_tx_ptr\n\tld.w %r10,[%r4]\n\tld.w %r6,0xa\n\txld.w %r7,ir_carrier_ram\n\tcall %r7\n\tpopn %r12");
    T16_CTL5 = 0;
    T16_CR5A = 0;
    RTCINTMODE = 7;
    P0_P0D = 0x1f;
}

/*
 * Pause a x about 31 ms: T16 ch.5 period a * 0xE2 at MCLK/4096 (30.86 ms per unit), halt
 * until its compare-A tick (the RAM handler at 0xD94 increments tmr_t5_ticks, so sys_copy_ram_routines
 * must have run). Live: also used outside the IR code (btn_enable, F14).
 */
void tmr_t5_pause(u8 a)
{
    tmr_t5_ticks = 0;
    tmr_t5_start_raw(a * 0xe2, 0xf);
    while (tmr_t5_ticks == 0)
        asm volatile("halt");
}

/*
 * tmr_t5_pause(a), then the standard 3 Hz tick again (tmr_t5_standard_tick).
 */
void tmr_t5_pause_restore(u8 a)
{
    tmr_t5_ticks = 0;
    tmr_t5_start_raw(a * 0xe2, 0xf);
    while (tmr_t5_ticks == 0)
        asm volatile("halt");
    tmr_t5_standard_tick();
}

/*
 * Copy two small interrupt-time routines from flash into internal RAM (fast, and not
 * affected by flash programming), halfword by halfword up to the 0x0000 halfword that
 * follows each routine: ir_carrier_burst -> ir_carrier_ram (0xD58..0xD8D, the IR carrier burst
 * generator: toggles the port at ir_port_ptr with 0xE0/0 on every T16 ch.5 tick tmr_t5_ticks,
 * n pulses) and tmr_t5_tick_handler -> ir_ram_routines (0xD94..0xDB1, the T16 ch.5 compare-A handler,
 * tmr_t5_ticks++, which vector 51 tmr_t5_cmpa_vector jumps to). Only ROM code is ever copied here:
 * the RAM vector is not a hook for downloaded code. Called at every communication menu
 * entry (comm_prepare), so the 0xD94 handler is live (the delays tmr_t5_pause / EE2 need
 * it); the 0xD58 carrier routine is only used by the dead IR transmit.
 */
void sys_copy_ram_routines(void)
{
    u8 i;
    u16 x;

    i = 0;
    do {
        x = ir_carrier_ram[i] = ir_carrier_code[i];
        i++;
    } while (x != 0);
    i = 0;
    do {
        x = ir_ram_routines[i] = tmr_t5_handler_code[i];
        i++;
    } while (x != 0);
}

/*
 * Hand-written assembly, run from RAM 0xD58 (copied by sys_copy_ram_routines up to the `nop` =
 * 0x0000 that follows its `ret`): the IR carrier burst generator, called with r6 = n
 * pulses. Clears tmr_t5_ticks, takes the port address from ir_port_ptr (0x300380 = P0 data,
 * set by sys_clock_osc3), then on each T16 ch.5 tick alternately stores 0xE0 (`ld.w
 * %r9,0x20` sign-extends to 0xFFFFFFE0) and 0x00: one tick on, one tick off, n times. At
 * the 8.07 us tick of ir_send_packet that is a 62.0 kHz carrier on P05 (the only output of
 * bits 5-7 in the IR port set-up; P07 also toggles once main has made it an output). Ends
 * with tmr_t5_ticks = 0 so the caller can time the following space. The `xld.w
 * %r8,0x3007ae` is dead (overwritten by the load from ir_port_ptr).
 */
asm(".include \"asm/func/func_2504F68.s\"");

/* Empty (`ret`). */
void func_2504F9E(void)
{
}

/*
 * Wait until a button is pressed (btn_a_presses..22) or a seconds pass (ui_timeout), idling
 * (halt + sys_idle_housekeeping) on the standard 3 Hz tick. Live: used by the NFC code too.
 */
void btn_wait_press_or_timeout(u8 a)
{
    tmr_t5_standard_tick();
    ui_timeout = a;
    btn_a_presses = 0;
    btn_b_presses = 0;
    btn_c_presses = 0;
    for (;;) {
        if ((u8)(btn_a_presses | btn_b_presses | btn_c_presses))
            break;
        asm volatile("halt");
        if (ui_timeout == 0)
            break;
        sys_idle_housekeeping();
    }
}

/*
 * IR packet checksum: 8-bit sum of bytes 0..18 (stored in byte 19). The Tamagotchi
 * Connection IR packets end with the same kind of sum (see notes/findings/ir.md).
 */
u8 ir_checksum(u8 *p)
{
    u8 sum = 0;
    s16 i;

    for (i = 0; i <= 18; i++)
        sum += p[i];
    return sum;
}

/*
 * Critical section enter: save the PSR in sys_saved_psr and clear it (interrupts off);
 * sys_irq_restore restores it with the IE bit (0x10) set.
 */
void sys_irq_save_disable(void)
{
    asm volatile("ld.w %%r4,%%psr\n\tpushn %%r0\n\txld.w %%r0,sys_saved_psr\n\tld.w [%%r0],%%r4\n\tpopn %%r0\n\tld.w %%r4,0x0\n\tld.w %%psr,%%r4" : : : "r4");
}

/*
 * Restore interrupts (PSR from sys_saved_psr with IE set).
 */
void sys_irq_restore(void)
{
    asm volatile("ld.w %%psr,%0" : : "r"(sys_saved_psr | 0x10));
}

/*
 * Button sample history init: btn_samples[i] = 0xFF when button i (P00..P02, active low) is
 * currently pressed, else 0. btn_history_sample shifts a new sample into each byte every call
 * ((btn_samples[2] & 3) == 3 = C pressed for the last two samples).
 */
void btn_history_init(void)
{
    u8 x = P0_P0D;
    u8 i;

    for (i = 0; i <= 2; i++) {
        if (((x >> i) & 1) == 0)
            btn_samples[i] = 0xff;
        else
            btn_samples[i] = 0;
    }
}

/*
 * Buttons: shift one new sample of P00..P02 into the history bytes btn_samples[0..2] (bit 0 =
 * newest, 1 = pressed). Used for debouncing / "held" detection: (x & 3) == 3 = pressed in the
 * last two samples.
 */
void btn_history_sample(void)
{
    u8 x = P0_P0D;
    u8 i;

    for (i = 0; i <= 2; i++) {
        btn_samples[i] <<= 1;
        if (((x >> i) & 1) == 0)
            btn_samples[i] |= 1;
    }
}

/*
 * Wait for the key-click sound to finish (snd_wait_end), then disable the button
 * interrupts (port inputs 0..3) and clear their flags, with interrupts masked.
 */
void btn_disable(void)
{
    snd_wait_end();
    sys_irq_save_disable();
    INT_EK01_EP03 = 0;
    INT_FK01_FP03 = 0xff;
    sys_irq_restore();
}

/*
 * Re-enable the buttons and restart the standard tick after an exchange, keeping the
 * button counters (a 62 ms pause tmr_t5_pause(2) in between, not a sound).
 */
void btn_enable(void)
{
    u8 a = btn_a_presses;
    u8 b = btn_b_presses;
    u8 c = btn_c_presses;

    btn_a_presses = 1;
    btn_b_presses = 1;
    btn_c_presses = 1;
    snd_wait_end();
    sys_irq_save_disable();
    btn_rearm();
    sys_irq_restore();
    tmr_t5_pause(2);
    tmr_t5_standard_tick();
    btn_a_presses = a;
    btn_b_presses = b;
    btn_c_presses = c;
}

/*
 * Same as tmr_t5_start_tick but the caller passes the full clock control value b (prescaler
 * without the forced "on" bit). Settings used: (0xF1, 8) = 8.07 us IR transmit tick,
 * (0x2D, 0xF) = 6.3 ms and (0xF4, 0xF) = 33 ms session polling ticks, (a * 0xE2, 0xF) =
 * the a x 31 ms pause of tmr_t5_pause.
 */
void tmr_t5_start_raw(u16 a, u8 b)
{
    int x;

    sys_irq_save_disable();
    T16_CTL5 = 2;
    T16_CR5A = a - 1;
    T16_CR5B = a;
    T16_CLKCTL_5 = b;
    x = INT_E16T45 & 0xf;
    x |= 0x80;
    INT_E16T45 = x;
    INT_F16T45 = 0x80;
    T16_CTL5 = 1;
    sys_irq_restore();
}

/*
 * Timer 5 back to its normal role after any IR / delay use: flags cleared, pINT_E16T45 =
 * 0x44 (timer 4 and timer 5 compare B enabled, timer 5 compare A off), period 0x989 + 1 =
 * 2442 counts at MCLK/4096 (clock control 0xF) = 30 MHz / 4096 / 2442 = 3.0 Hz; the
 * compare-B ISR (vector 50) sets tmr_flags bit 5, which the menus use as their coarse tick.
 */
void tmr_t5_standard_tick(void)
{
    INT_F16T45 = 0xc0;
    INT_E16T45 = 0x44;
    T16_CTL5 = 2;
    T16_CR5A = 0;
    T16_CR5B = 0x989;
    T16_CLKCTL_5 = 0xf;
    T16_CTL5 = 1;
}

/*
 * Vector 51 (T16 ch.5 compare A): save r0-r2 and jump to the handler in RAM at 0xD94
 * (ir_ram_routines); the `reti` after the jump is never reached.
 */
void __attribute__((interrupt_handler)) tmr_t5_cmpa_vector(void)
{
    asm volatile("pushn %r2\n\txld.w %r1,ir_ram_routines\n\tjp %r1");
}

/*
 * Hand-written assembly, run from RAM 0xD94 (copied by sys_copy_ram_routines): the tail of the
 * vector-51 handler: tmr_t5_ticks++, clear the timer-5 compare-A flag (halfword write of 0x80
 * to 0x300284), popn r2, reti. It is the tick of the IR carrier and sampling loops and also
 * of the live delays tmr_t5_pause / tmr_t5_pause_restore / tmr_t5_wait_period. Until sys_copy_ram_routines runs
 * (every communication menu entry), 0xD94 holds zeros.
 */
asm(".include \"asm/func/func_25051AE.s\"");

/* Empty (`ret`). */
void func_25051CC(void)
{
}

/* Stop timer 5 and set the RTC interrupt mode to 7 (1-second, level sensed, enabled);
 * ir_last_error = v (status of the aborted serial transfer). Returns v. */
/*
 * IR: stop T16 ch.5, set the RTC interrupt mode back to 7 and remember the error code a in
 * ir_last_error.
 */
int ir_stop(int v)
{
    T16_CTL5 = 0;
    T16_CR5A = 0;
    RTCINTMODE = 7;
    ir_last_error = v;
    return v;
}

/*
 * The RAM variables this file defines.
 */
volatile u16 ir_result;
u32 ir_port_ptr;
u32 sys_saved_psr;
u16 ir_last_error;
u8 ir_session_busy;
u16 ir_carrier_ram[30];
u16 ir_ram_routines[16];

