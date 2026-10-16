/*
 * Start-up: the reset handler, bus / interrupt controller / GPIO / timer set-up, copying the
 * RAM code and .data, clearing .bss, interrupt enable / disable, and the A/D converter set-up.
 *
 * Code 24F61BC..24F659E, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/sys/sys_boot.h"
#include "proto/room/room_main.h"
#include "proto/tmr/tmr_timeouts.h"
#include "proto/nfc/nfc_session.h"
#include "proto/lib/snd/snd_sequencer.h"
#include "libc.h"
#include "hw.h"

/*
 * A/D converter setup (S1C33E07 ADC; registers 0x30055E, 0x300520, 0x300542): advanced
 * mode, clock on with division ratio c (0xF = MCLK/256), start channel a, end channel b,
 * trigger/mode bits d (pAD_TRIG_CHNL: TS/MS). Called once at boot from sys_init as
 * (0, 0, 0xF, 0): channel 0 only (AIN0 on P70 = the battery voltage divider), software
 * trigger.
 */
void sys_adc_setup(int a, int b, int c, int d)
{
    AD_ADVMODE |= 0x100;
    AD_CLKCTL = c | (AD_CLKCTL & ~0xf);
    AD_TRIG_CHNL = (AD_TRIG_CHNL & ~0x3f38) | (((a & 7) << 8) | ((b & 7) << 11) | (d & 0x38));
}

/*
 * A/D out-of-range comparison: in the control/status register pAD_EN_SMPL_STAT (0x300544)
 * set bits 12..14 to a (the channel compared) and bit 15 (limit compare enable), and write
 * the upper/lower limits b (pAD_UPPER, 0x300558) and c (pAD_LOWER, 0x30055A) used for the
 * out-of-range interrupt. Not called by the firmware.
 */
void sys_adc_set_limits(int a, u16 b, u16 c)
{
    AD_EN_SMPL_STAT = (AD_EN_SMPL_STAT & 0xfff) | ((a & 7) << 12) | 0x8000;
    AD_UPPER = b;
    AD_LOWER = c;
}

/*
 * A/D interrupt setup: b = channel mask for the conversion-complete interrupt
 * (pAD_CH04_INTMASK, 0x30055C); a non-zero b also enables CNVINTEN, zero disables it.
 * a = extra pAD_EN_SMPL_STAT bits, OR'ed in together with INTMODE = "complete only".
 * Boot calls it with (0, 1): channel 0 only. The completion interrupt (vector 64,
 * sys_adc_isr) stores the result in sys_battery_level.
 */
void sys_adc_irq_setup(u16 a, u16 b)
{
    if (b == 0)
        AD_EN_SMPL_STAT &= ~0x10;
    else
        AD_EN_SMPL_STAT |= 0x10;
    AD_EN_SMPL_STAT = (AD_EN_SMPL_STAT & ~0x20) | a | 0x40;
    AD_CH04_INTMASK = b;
}

/*
 * The reset handler (vector 0): clock set-up (CMU registers unlocked with 0x96), the RAM
 * 0..0x5000 cleared and the stack set up, then the bus and the ports, the runtime and the
 * game (sys_main), which never returns.
 */
void sys_reset(void)
{
    CMU_PROTECT = 0x96;
    CMU_GATEDCLK0 = 0xf;
    CMU_GATEDCLK1 = 0xa07f1ad;
    LCDC_IRAM = 1;
    CMU_GATEDCLK0 = 8;
    CMU_CLKCNTL = 0x770003;
    CMU_OPT = 0x5c00;
    asm volatile("slp");
    CMU_PLL = 0x101804;
    CMU_OPT = 1;
    CMU_PROTECT = 0;
    /* clear the RAM 0..0x5000, zero all registers, set up the stack */
    asm volatile(
        "ld.w %r0,0x0\n\t"
        "ld.w %psr,%r0\n\t"
        "ld.w %r1,0x0\n\t"
        "xld.w %r2,0x5000\n"
        "1:\n\t"
        "ld.w [%r1]+,%r0\n\t"
        "cmp %r1,%r2\n\t"
        "jrne 1b\n\t"
        "ld.w %r1,%r0\n\t"
        "ld.w %r2,%r0\n\t"
        "ld.w %r3,%r0\n\t"
        "ld.w %r4,%r0\n\t"
        "ld.w %r5,%r0\n\t"
        "ld.w %r6,%r0\n\t"
        "ld.w %r7,%r0\n\t"
        "ld.w %r8,%r0\n\t"
        "ld.w %r9,%r0\n\t"
        "ld.w %r10,%r0\n\t"
        "ld.w %r11,%r0\n\t"
        "ld.w %r12,%r0\n\t"
        "ld.w %r13,%r0\n\t"
        "ld.w %r14,%r0\n\t"
        "ld.w %alr,%r0\n\t"
        "ld.w %ahr,%r0\n\t"
        "xld.w %r15,0x5000\n\t"
        "ld.w %sp,%r15\n\t"
        "ld.w %r15,0x0");
    sys_bus_setup();
    sys_gpio_init();
    /*
     * The firmware then unlocks the CMU again to set CMU_GATEDCLK1 = 0xA07E12D, through the
     * registers that held the register addresses and 0x96 before the RAM clear, which has
     * zeroed them: the three stores go to RAM address 0 (later overwritten by the
     * internal-RAM code) and CMU_GATEDCLK1 keeps 0xA07F1AD.
     */
    sys_init();
    nfc_session_clear();
    sys_main();
    for (;;)
        ;
}

/*
 * Boot-time hardware/runtime init, called from the reset handler (sys_reset, asm) after
 * the clocks, the SRAM controller/bus and the ports are set up (interrupts still disabled)
 * and before main (sys_main). Order: runtime/heap setup (_init_sys, _init_lib,
 * ansi_InitMalloc on the IRAM area 0xD08..0x1108), interrupt controller (sys_itc_init), 16-bit
 * timers (tmr_init), P24/P25 driven low as outputs, start a 3000 ms timeout on T16
 * ch.5 (tmr_t5_start_timeout only arms the timer; nothing waits for it here), copy the IRAM code
 * (sys_copy_iram_code) and .data (sys_copy_data), clear the two .bss areas (sys_clear_bss_ram,
 * sys_clear_bss_iram), A/D converter on channel 0 (battery) with its interrupt
 * (sys_adc_setup/func_24F621C), ADC stopped, sys_battery_level (last battery reading) preset to
 * 0x3FF = full scale until the first conversion, then enable interrupts (sys_irq_enable).
 */
void sys_init(void)
{
    _init_sys();
    _init_lib();
    ansi_InitMalloc((unsigned long)sys_iram_bss_start, (unsigned long)sys_heap_end);
    sys_itc_init();
    tmr_init();
    P2_IOC2 = 0x30;
    P2_P2D = 0;
    tmr_t5_start_timeout(3000);
    sys_copy_iram_code();
    sys_copy_data();
    sys_clear_bss_ram();
    sys_clear_bss_iram();
    sys_adc_setup(0, 0, 0xf, 0);
    sys_adc_irq_setup(0, 1);
    AD_EN_SMPL_STAT &= ~6;
    sys_battery_level = 0x3ff;
    sys_irq_enable();
}

/*
 * External bus (SRAMC, 0x301500..) setup, the first thing the reset handler calls:
 * BCLK/2 and #CE9 BCLK/2 setup (pSRAMC_BCLK_SETUP = 0x81), static wait states per chip
 * enable (pSRAMC_SWAIT = 0x72707777: CE4..CE7 and CE9/CE11 = 7, CE8 = 0, CE10 = 2), device
 * sizes (pSRAMC_SLV_SIZE = 0x4655: 16-bit areas except #CE8 = 8 bits, the area 8
 * 0x600000..0x7FFFFF of the LCD controller, which also gets 0 wait cycles; A0 is the
 * controller's command/data select) and device types (pSRAMC_A0_BSL
 * = 0, A0-type devices) of the chip-select areas used by the NOR flash (code/data mirror at
 * 0x2000000) and the LCD controller (0x600000/0x600001).
 */
void sys_bus_setup(void)
{
    SRAMC_BCLK_SETUP = 0x81;
    SRAMC_SWAIT = 0x72707777;
    SRAMC_SLV_SIZE = 0x4655;
    SRAMC_A0_BSL = 0;
}

/*
 * Interrupt controller (S1C33E07 ITC, 0x300260..0x3002AA) initial state.
 * Enables: port inputs 0-2 (buttons A/B/C on P00..P02, pINT_EK01_EP03 = 7), T16 ch.3
 * compare B (E16TU3, ms timeout, vector 42) and T16 ch.5 compare B (E16TU5, delay, vector
 * 50); everything else (DMA, timers 0/1, SIO, RTC, A/D, LCDC, SPI, ports 8-15, I2S) off.
 * Every interrupt cause flag is then cleared (writing 0xFF).
 * Priorities: serial Ch.1 = 7 (highest: the PN512 UART), A/D 3, port inputs 12/13 (PN512
 * IRQ) 3, RTC 3, T16 ch.3 and ch.5 (the odd channel of the pairs 2-3 and 4-5) 3, port
 * inputs 0-3 (buttons) 3.
 * The RTC, A/D, SIO1 and timer 3/4 enables are switched on later by their drivers.
 */
void sys_itc_init(void)
{
    INT_EK01_EP03 = 7;
    INT_EDMA = 0;
    INT_E16T01 = 0;
    INT_E16T23 = 0x40;
    INT_E16T45 = 0x40;
    INT_ESIF01 = 0;
    INT_EP47_ERTC_EAD = 0;
    INT_ELCDC = 0;
    INT_ESIF2_ESPI = 0;
    INT_EP815 = 0;
    INT_EI2S = 0;
    INT_FK01_FP03 = 0xff;
    INT_FDMA = 0xff;
    INT_F16T01 = 0xff;
    INT_F16T23 = 0xff;
    INT_F16T45 = 0xff;
    INT_FSIF01 = 0xff;
    INT_FP47_FRTC_FAD = 0xff;
    INT_FLCDC = 0xff;
    INT_FSIF2_FSPI = 0xff;
    INT_FP815 = 0xff;
    INT_FI2S = 0xff;
    INT_PSIO1_PAD = 0x37;
    INT_PP1213L = 3;
    INT_PRTC = 3;
    INT_P16T23 = 0x30;
    INT_P16T45 = 0x30;
    INT_PP01L = 0x33;
    INT_PP23L = 3;
}

/*
 * GPIO init (ports P0..P9: pull-ups, function select, directions, port-input interrupt
 * routing; pin assignment in notes/fable/peripherals.md):
 *   pull-ups: P0 except P04 (SIN1), all P1, P3[0..6], all P6, P7, P8, P9 except P94;
 *   function select: P10-P12 = TM0-TM2 timer outputs (pP1_03_CFP = 0x15: sound tones and
 *   the LCD backlight PWM on TM2), P70 = AIN0 (battery A/D), P04/P05 = SIN1/SOUT1
 *   (0x3003A1 = 5: serial Ch.1 = UART to the PN512 NFC chip);
 *   directions: P0 inputs except P03/P05 (0x28: P00-P02 = buttons A/B/C, P06/P07 inputs,
 *   P07 = the LCD controller strap, see lcd_init), all other ports outputs driven low
 *   (P13 high, P05 low; P94 input = PN512 IRQ);
 *   port-input interrupts: FPT0..3 taken from P00..P03 (pPINTSEL_SPT03 = 0), low/falling
 *   polarity (pPINTPOL = 0), edge triggered for FPT0,1,2,4 (pPINTEL_SEPT07 = 0x17).
 */
void sys_gpio_init(void)
{
    MISC_PUP0 = 0xef;
    MISC_PUP1 = 0xff;
    MISC_PUP3 = 0x7f;
    MISC_PUP6 = 0xff;
    MISC_PUP7 = 0x1f;
    MISC_PUP8 = 0x3f;
    MISC_PUP9 = 0xef;
    P1_03_CFP = 0x15;
    P7_03_CFP = 1;
    P0_47_CFP = 5;
    P0_IOC0 = 0x28;
    P1_IOC1 = 0xff;
    P2_IOC2 = 0xff;
    P3_IOC3 = 0xff;
    P4_IOC4 = 0xff;
    P5_IOC5 = 0xff;
    P6_IOC6 = 0xff;
    P8_IOC8 = 0xff;
    P9_IOC9 = 0xef;
    P0_P0D = 0xdf;
    P1_P1D = 8;
    P2_P2D = 0;
    P3_P3D = 0;
    P4_P4D = 0;
    P5_P5D = 0;
    P6_P6D = 0;
    P7_P7D = 0;
    P8_P8D = 0;
    P9_P9D = 0;
    PINTSEL_SPT03 = 0;
    PINTPOL_SPP07 = 0;
    PINTEL_SEPT07 = 0x17;
}

/*
 * 16-bit timers: advanced mode (pT16_ADVMODE 0x3007DE = 1, needed for the counter writes
 * and pause bits) and stop/reset (CTL = 2 = PRESET) of timers 0, 2, 3, 5. Timer roles:
 * 0-3 = speaker tone channels (TM0-TM3 outputs; which ones the sound code really drives is
 * notes/OPEN.md item 10), 3 also the 100 ms NFC tick, 4 = sound sequencer tick, 5 =
 * general delays/timeouts (ISR through the RAM vector ir_ram_routines).
 */
void tmr_init(void)
{
    T16_ADVMODE = 1;
    T16_CTL0 = 2;
    T16_CTL2 = 2;
    T16_CTL3 = 2;
    T16_CTL5 = 2;
}

/*
 * Copy the RAM-resident code (asm/iram.s: sprite blitters, pixel routines, LCD flush,
 * flash erase/program) from its load address 0x2562840 to internal RAM 0..0xD08. The flash
 * programming code must run from RAM because the flash cannot be read while a command is
 * in progress, and the blits are speed critical.
 */
void sys_copy_iram_code(void)
{
    u8 *s = D_2562840;
    u8 *d = sys_iram_code;
    u32 n = sys_iram_bss_start - d;
    u32 i;

    for (i = 0; i < n; i++)
        *d++ = *s++;
}

/*
 * Copy the .data initial values (asm/ramdata.s, load address 0x25626FC) to their run
 * address 0x84474..0x845B8 in the 2 KB DST RAM.
 */
void sys_copy_data(void)
{
    u8 *s = D_25626FC;
    u8 *d = sys_ram_data_start;
    u32 n = sys_ram_data_end - d;
    u32 i;

    for (i = 0; i < n; i++)
        *d++ = *s++;
}

/*
 * Zero the .bss part of the DST RAM (0x84000 .. start of .data at 0x84474).
 */
void sys_clear_bss_ram(void)
{
    u8 *d = sys_ram_bss_start;
    u32 n = sys_ram_data_start - d;
    u32 i;

    for (i = 0; i < n; i++)
        *d++ = 0;
}

/*
 * Zero the internal-RAM .bss variables 0xD08 (right after the RAM code) .. 0x4AE4; the
 * stack lives above it up to 0x5000.
 */
void sys_clear_bss_iram(void)
{
    u8 *d = sys_iram_bss_start;
    u32 n = sys_iram_bss_end - d;
    u32 i;

    for (i = 0; i < n; i++)
        *d++ = 0;
}

void sys_irq_enable(void)
{
    asm volatile("psrset 0x4");
}

void sys_irq_disable(void)
{
    asm volatile("psrclr 0x4");
}

/*
 * The RAM variables this file defines.
 */
volatile u16 sys_battery_level;
