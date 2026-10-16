/*
 * NFC hardware unit: UART interrupts, the 100 ms tick, PN512 power and init.
 *
 * Code 252D204..252D3F2, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "flash.h"
#include "hw.h"

/*
 * The three EFSIO ch.1 ISRs of the PN512 link. 0x300286 is the ITC cause flag register of
 * SIO ch.0/1 (bit 3 = ch.1 receive error, bit 4 = ch.1 receive buffer full, bit 5 = ch.1
 * transmit buffer empty; 0x300276 = the matching enables).
 *
 * Transmit buffer empty: send the next byte of nfc_tx_fifo, draining any received bytes
 * into nfc_rx_fifo on the way; when the ring is empty, disable the interrupt.
 */
void __attribute__((interrupt_handler)) nfc_uart_tx_isr(void)
{
    INT_FSIF01 = 0x20;
    if (nfc_tx_fifo.count == 0) {
        INT_ESIF01 &= ~0x20;
    } else {
        EFSIF1_TXD = *nfc_tx_fifo.rd;
        nfc_tx_fifo.rd++;
        if (nfc_tx_fifo.count != 0)
            nfc_tx_fifo.count--;
        while (EFSIF1_STATUS & 1) {
            INT_FSIF01 = 0x10;
            *nfc_rx_fifo.wr = EFSIF1_RXD;
            nfc_rx_fifo.wr++;
            nfc_rx_fifo.count++;
        }
    }
}

/*
 * Receive buffer full: append RXD1 to nfc_rx_fifo (no wrap-around or overflow check: the
 * ring is reset by nfc_uart_send before every exchange).
 */
void __attribute__((interrupt_handler)) nfc_uart_rx_isr(void)
{
    INT_FSIF01 = 0x10;
    *nfc_rx_fifo.wr = EFSIF1_RXD;
    nfc_rx_fifo.wr++;
    nfc_rx_fifo.count++;
}

/*
 * Receive error (ch.1 cause flag, 0x300286 bit 3): meant to collect the overrun /
 * parity / framing flags (STATUS bits 2..4 = 0x1C) into nfc_rx_fifo.status, but it reads
 * 0x300B02 = EFSIF0_STATUS, the status of ch.0, instead of 0x300B12 (ch.1). A bug, but a
 * harmless one: ch.0 is never configured anywhere in the firmware, so its error bits are
 * 0 and nothing is ever added; the real ch.1 error flags stay set (they are only cleared
 * by writing 0, S1C33E07 manual V.1.4.3) and reception carries on regardless. The only
 * effect is that a corrupted UART byte is never reported by nfc_uart_recv; the protocol
 * layers still catch bad frames through the PN512 ErrorReg, length and content checks.
 */
void __attribute__((interrupt_handler)) nfc_uart_err_isr(void)
{
    INT_FSIF01 = 8;
    nfc_rx_fifo.status |= EFSIF0_STATUS & 0x1c;
}

/*
 * NFC: start the 100 ms tick on T16 ch.3 (0xB71B counts at MCLK/64 = 10 Hz, compare-A
 * interrupt vector 43 -> nfc_tick_isr: nfc_ticks++ and, when nfc_cancel_request == 1, the abort
 * request nfc_request_abort). nfc_ticks is the time base of all NFC timeouts (units of 100 ms).
 */
void nfc_tick_start(void)
{
    nfc_ticks = 0;
    INT_F16T23 = 0x80;
    T16_CTL3 = 2;
    T16_CR3A = 0xb71b;
    T16_CR3B = 0xb71b;
    T16_CLKCTL_3 = 12;
    T16_CTL3 = 1;
    INT_E16T23 |= 0x80;
}

/*
 * NFC: stop the 100 ms tick (T16 ch.3) and its interrupt.
 */
void nfc_tick_stop(void)
{
    INT_E16T23 &= ~0x80;
    T16_CTL3 = 0;
    T16_CLKCTL_3 = 4;
    INT_F16T23 = 0x80;
}

void __attribute__((interrupt_handler)) nfc_tick_isr(void)
{
    INT_F16T23 = 0x80;
    nfc_ticks++;
    if (nfc_cancel_request == 1)
        nfc_request_abort();
}

/*
 * Short busy-wait used between the NFC power pin changes.
 */
void nfc_power_delay(void)
{
    int i;
    for (i = 0; i < 0x5dc0; i++)
        asm("nop");
}

/*
 * NFC front-end hardware reset / power on, first step of nfc_init: P14 (P1 bit 4)
 * pulsed low-high-low around the delay nfc_power_delay, then P13 (bit 3) low, then a short
 * delay. The meaning of the pins is not certain: P14 = the PN512 NRSTPD reset line and
 * P13 = power/enable (Fable), or a power switch / latch (Opus leaves it open).
 */
void nfc_hw_power_on(void)
{
    int i;

    P1_P1D &= ~0x10;
    P1_P1D |= 0x10;
    nfc_power_delay();
    P1_P1D &= ~0x10;
    P1_P1D &= ~0x08;
    for (i = 0; i <= 9; i++)
        __asm__("nop");
}

/*
 * PN512 (NFC front-end) power off, the inverse of nfc_hw_power_on: P13 high, then a P14
 * pulse (reset). Used when the chip fails to answer during init (nfc_init returns
 * -15) and by nfc_power_off.
 */
void nfc_hw_power_off(void)
{
    int i;

    P1_P1D |= 0x08;
    for (i = 0; i <= 9; i++)
        __asm__("nop");
    P1_P1D &= ~0x10;
    P1_P1D |= 0x10;
    nfc_power_delay();
    P1_P1D &= ~0x10;
}



/*
 * PN512 bring-up (nfc_power_up, at start-up and after a failure): power/reset pulse
 * (nfc_hw_power_on), nfc_flash_base = p (the flash base used when programming received data),
 * P04/P05 to their serial function (pP0_47_CFP 0x3003A1 = SIN1/SOUT1), P94 as the IRQ
 * input, EFSIO ch.1 = 8-bit async, internal clock, 1 stop bit, no parity (CTL1 = 3), 1/8
 * sampling (IRDA1 = 0x10), reload 0xC3 = 9566 bd (the PN512 starts at 9.6 kbd).
 * Then, by PN512 register:
 *   CommandReg 0x01 = 0x22 (RcvOff + Generate RandomID), read back (0x81) until Idle with
 *     RcvOff ((c & 0x2F) == 0x20), 20 tries, else power off and -15;
 *   FIFOLevel 0x0A = 0x80 (flush), ControlReg 0x0C = 0x20 (WrNFCIDtoFIFO), wait for 10
 *     bytes in the FIFO and read them (0x89) into nfc_id3. So our NFCID3 is the 10-byte
 *     random number the chip has just generated (datasheet 18.3.1.3), not a fixed chip
 *     id; bytes 0/1 and 8/9 are then forced to F0 40, the "I am a Tamagotchi" marker the
 *     peer checks in nfc_detect_initiator / nfc_detect_target;
 *   DivIEn 0x03 = 0x82 (IRQPushPull + RfOnIEn), ComIrq/DivIrq 0x04/0x05 = 0x7F (clear),
 *   RxThreshold 0x18 = 0x55, RFCfg 0x26 = 0x59 (RxGain 38 dB, RF level 1001), GsNOn
 *   0x27 = 0xF4, TxAuto 0x15 = 0x40 (Force100ASK), TxControl 0x14 = 0x80 (antenna
 *   drivers off), CommandReg = 0x37 (soft power-down).
 * The chip then sleeps until an external RF field pulls its IRQ (nfc_irq_isr sets
 * nfc_field_irq). Returns 0, or -15 when the chip does not answer.
 */
s8 nfc_init(struct FlashBase *p)
{
    volatile s8 c;
    u32 k, i;

    nfc_hw_power_on();
    nfc_flash_base = (u32)p;
    P0_47_CFP &= 0xf0;
    P0_47_CFP |= 5;
    P9_47_CFP &= 0xfc;
    MISC_PUP9 &= ~0x10;
    P9_IOC9 &= ~0x10;
    PINTSEL_SPT1215 |= 3;
    PINTPOL_SPP815 &= ~0x10;
    INT_FP815 = 0x10;
    INT_EP815 |= 0x10;
    INT_ESIF01 &= 7;
    EFSIF1_BRTRUN = 0;
    EFSIF1_CTL &= 0x3f;
    INT_FSIF01 = 0x38;
    EFSIF1_IRDA = 0x10;
    EFSIF1_CTL = 3;
    EFSIF1_BRTRD = 0xc3;
    EFSIF1_BRTRUN = 1;
    asm("nop");
    EFSIF1_CTL |= 0xc0;
    for (i = 0; i <= 49999; i++)
        asm("nop");
    k = 0;
    do {
        EFSIF1_TXD = 1;
        EFSIF1_TXD = 0x22;
        for (i = 0; i <= 4999; i++)
            asm("nop");
        c = EFSIF1_RXD;
        EFSIF1_TXD = 0x81;
        for (i = 0; i <= 4999; i++)
            asm("nop");
        c = EFSIF1_RXD;
        if (++k > 20) {
            nfc_hw_power_off();
            return -15;
        }
    } while ((c & 0x2f) != 0x20);
    EFSIF1_TXD = 10;
    EFSIF1_TXD = 0x80;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 0xc;
    EFSIF1_TXD = 0x20;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    k = 0;
    do {
        EFSIF1_TXD = 0x8a;
        for (i = 0; i <= 1999; i++)
            asm("nop");
        c = EFSIF1_RXD;
        if (++k > 20) {
            nfc_hw_power_off();
            return -15;
        }
    } while (c <= 9);
    for (k = 0; k <= 9; k++) {
        EFSIF1_TXD = 0x89;
        for (i = 0; i <= 1999; i++)
            asm("nop");
        ((u8 *)&nfc_id3)[k] = EFSIF1_RXD;
    }
    nfc_id3.marker0 = 0xf0;
    nfc_id3.marker1 = 0x40;
    nfc_id3.marker2 = 0xf0;
    nfc_id3.marker3 = 0x40;
    EFSIF1_TXD = 3;
    EFSIF1_TXD = 0x82;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 4;
    EFSIF1_TXD = 0x7f;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 5;
    EFSIF1_TXD = 0x7f;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 0x18;
    EFSIF1_TXD = 0x55;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 0x26;
    EFSIF1_TXD = 0x59;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 0x27;
    EFSIF1_TXD = 0xf4;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 0x15;
    EFSIF1_TXD = 0x40;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 0x14;
    EFSIF1_TXD = 0x80;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_TXD = 1;
    EFSIF1_TXD = 0x37;
    for (i = 0; i <= 1999; i++)
        asm("nop");
    c = EFSIF1_RXD;
    EFSIF1_BRTRUN = 0;
    asm("nop");
    EFSIF1_CTL &= 0x3f;
    return 0;
}

/*
 * The RAM variables this file defines.
 */
volatile u32 nfc_ticks;
volatile struct NfcTxFifo nfc_tx_fifo;
volatile struct NfcRxFifo nfc_rx_fifo;
struct NfcId3 nfc_id3;
u8 nfc_cancel_request;
u32 nfc_flash_base = (u32)&flash_base;
