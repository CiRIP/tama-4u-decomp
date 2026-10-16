/*
 * The PN512 UART link: opening and closing the link at 460k / 921k, sending and receiving
 * frames, the field-detect interrupt.
 *
 * Code 252C5A6..252CFD8, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/nfc/nfc_link.h"
#include "proto/nfc/nfc_session.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "hw.h"

/*
 * PN512 serial link (notes/findings/nfc.md). The PN512 runs in its UART host-interface
 * mode on EFSIO ch.1 (0x300B10 TXD1, 0x300B11 RXD1, 0x300B12 STATUS1, 0x300B13 CTL1,
 * 0x300B14 IRDA1, 0x300B15 BRTRUN1, 0x300B16 BRTRD1), 8N1, LSB first. Register access
 * (PN512 datasheet 9.3.3, Tables 149-151): the address byte is bit 7 = read (1) / write
 * (0), bit 6 = 0, bits 5..0 = register 0x00..0x3F (all four pages, no PageReg needed).
 * Write = address byte + data byte, and the PN512 echoes the address byte; read = the
 * address byte alone (0x8A FIFOLevel, 0x88 Status2, 0x86 Error, 0x89 FIFOData, 0x81
 * Command, 0x8C Control, 0x9F SerialSpeed), the PN512 answers the register value. So a
 * script of n (register, value) pairs in nfc_cmd_buf gets n answer bytes back (the echoed
 * addresses), which the callers read with nfc_uart_recv and drop.
 * Baud rate: async mode with DIVMD1 = 1/8 (0x300B14 = 0x10), bps = MCLK / (16 x (BRTRD1
 * + 1)) (S1C33E07 manual V.1.2): 0xC3 -> 9566 bd (PN512 reset speed 0xEB = 9631 bd),
 * 3 -> 468750 bd (PN512 SerialSpeedReg 0x3A = 27.12 MHz / 59 = 459661 bd, 2 % apart),
 * 1 -> 937500 bd (PN512 0x1C = 27.12 MHz / 29 = 935172 bd, only in the unused
 * nfc_link_open_921k).
 */

/*
 * ISR of the PN512 IRQ pin (P94, port input interrupt; flag cleared with 0x3002A9 =
 * 0x10): nfc_field_irq = 1. The pin is a push-pull, active-low output (DivIEn bit 7
 * IRQPushPull, ComIEn left at its reset value 0x80 = IRqInv); only the RfOn / RfOff
 * sources (DivIEn bits 1 / 0) are ever enabled, so it signals "an RF field appeared"
 * (sleep, nfc_init / nfc_rearm_field_detect with DivIEn 0x82) or "the field went away" (DivIEn
 * 0x81).
 */
void nfc_irq_isr(void) __attribute__((interrupt_handler));

void nfc_irq_isr(void)
{
    u8 i;

    nfc_field_irq = 1;
    INT_FP815 = 0x10;
    for (i = 0; i <= 0x63; i++)
        asm("nop");
}

struct UartRingCfg { u8 *tx_buf; u32 tx_size; u8 *rx_buf; u32 rx_size; };

/*
 * Open an NFC session (every protocol function starts with it; a = {tx buffer, size, rx
 * buffer, size} for the rings nfc_tx_fifo / nfc_rx_fifo): UART ch.1 8N1 at 9566 bd, then wake
 * the PN512 from soft power-down: CommandReg 0x01 = 0x27 (RcvOff, PowerDown = 0,
 * NoCmdChange), then "write PageReg 0x00 = 0x55" until its echo 0x00 comes back (the
 * datasheet 15.2 asks for a 0x55 byte and a register-0 access until the chip answers);
 * 20 x 20 tries, else -1. SIO1 rx/error interrupts on (0x300276 bits 3/4), then:
 * DivIEn 0x03 = 0x81 (IRQPushPull + RfOffIEn), ComIrq 0x04 = DivIrq 0x05 = 0x7F (clear
 * all), and back to the reset values of what the sleep set-up changed: RxThreshold 0x18 =
 * 0x84, RFCfg 0x26 = 0x48, GsNOn 0x27 = 0x88, TxAuto 0x15 = 0. SerialSpeedReg 0x1F = 0x3A
 * (459661 bd), CPU reload 3 (468750 bd), read 0x9F back (must be 0x3A, else -1), and
 * start the 100 ms tick (nfc_tick_start: nfc_ticks = 0, so every "x * 10" timeout of the
 * protocol functions counts from here). Returns 0.
 */
s8 nfc_link_open(int a)
{
    volatile s8 s;
    u8 cmd[3];
    u8 r[2];
    struct UartRingCfg *c = (struct UartRingCfg *)a;
    u32 i, j, retry;

    EFSIF1_CTL &= 0x3f;
    EFSIF1_CTL = 3;
    EFSIF1_BRTRD = 0xc3;
    EFSIF1_BRTRUN = 1;
    asm("nop");
    EFSIF1_CTL |= 0xc0;
    nfc_tx_fifo.buf = c->tx_buf;
    nfc_tx_fifo.end = c->tx_buf + c->tx_size;
    nfc_tx_fifo.size = c->tx_size;
    nfc_tx_fifo.wr = nfc_tx_fifo.buf;
    nfc_tx_fifo.rd = nfc_tx_fifo.buf;
    nfc_tx_fifo.count = 0;
    nfc_rx_fifo.buf = c->rx_buf;
    nfc_rx_fifo.end = c->rx_buf + c->rx_size;
    nfc_rx_fifo.size = c->rx_size;
    nfc_rx_fifo.wr = nfc_rx_fifo.buf;
    nfc_rx_fifo.rd = nfc_rx_fifo.buf;
    nfc_rx_fifo.count = 0;
    nfc_rx_fifo.overflow = 0;
    nfc_rx_fifo.status = 0;
    s = 1;
    retry = 0;
    do {
        EFSIF1_TXD = 1;
        EFSIF1_TXD = 0x27;
        for (j = 0; j <= 4999; j++)
            asm("nop");
        if (EFSIF1_STATUS & 1)
            s = EFSIF1_RXD;
        for (i = 0; i <= 19; i++) {
            EFSIF1_TXD = 0;
            EFSIF1_TXD = 0x55;
            for (j = 0; j <= 1999; j++)
                asm("nop");
            if (EFSIF1_STATUS & 1) {
                s = EFSIF1_RXD;
                if (s == 0)
                    break;
            }
        }
        if (++retry > 20)
            return -1;
    } while (s != 0);
    INT_FSIF01 = 0x38;
    asm("nop");
    INT_ESIF01 |= 0x18;
    asm("nop");
    EFSIF1_TXD = 3;
    EFSIF1_TXD = 0x81;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 4;
    EFSIF1_TXD = 0x7f;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 5;
    EFSIF1_TXD = 0x7f;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x18;
    EFSIF1_TXD = 0x84;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x26;
    EFSIF1_TXD = 0x48;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x27;
    EFSIF1_TXD = 0x88;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x15;
    EFSIF1_TXD = 0;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x1f;
    EFSIF1_TXD = 0x3a;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_BRTRUN = 0;
    asm("nop");
    asm("nop");
    asm("nop");
    EFSIF1_BRTRD = 3;
    EFSIF1_BRTRUN = 1;
    asm("nop");
    cmd[0] = 0x9f;
    nfc_uart_send((u8 *)cmd, 1);
    nfc_uart_recv(r, 1, &r[1]);
    if ((s8)r[0] != 0x3a)
        return -1;
    nfc_tick_start();
    return 0;
}

/*
 * Unused (no reference) 921.6k variant of nfc_link_open: same wake-up and register set,
 * plus ModWidth 0x24 = 0x26 (reset value), SerialSpeedReg 0x1F = 0x1C (27.12 MHz / 29 =
 * 935172 bd) and CPU reload 1 (937500 bd); checks that 0x9F reads back 0x1C.
 */
s8 nfc_link_open_921k(int a)
{
    volatile s8 s;
    u8 cmd[3];
    u8 r[2];
    struct UartRingCfg *c = (struct UartRingCfg *)a;
    u32 i, j, retry;

    EFSIF1_CTL &= 0x3f;
    EFSIF1_CTL = 3;
    EFSIF1_BRTRD = 0xc3;
    EFSIF1_BRTRUN = 1;
    asm("nop");
    EFSIF1_CTL |= 0xc0;
    nfc_tx_fifo.buf = c->tx_buf;
    nfc_tx_fifo.end = c->tx_buf + c->tx_size;
    nfc_tx_fifo.size = c->tx_size;
    nfc_tx_fifo.wr = nfc_tx_fifo.buf;
    nfc_tx_fifo.rd = nfc_tx_fifo.buf;
    nfc_tx_fifo.count = 0;
    nfc_rx_fifo.buf = c->rx_buf;
    nfc_rx_fifo.end = c->rx_buf + c->rx_size;
    nfc_rx_fifo.size = c->rx_size;
    nfc_rx_fifo.wr = nfc_rx_fifo.buf;
    nfc_rx_fifo.rd = nfc_rx_fifo.buf;
    nfc_rx_fifo.count = 0;
    nfc_rx_fifo.overflow = 0;
    nfc_rx_fifo.status = 0;
    s = 1;
    retry = 0;
    do {
        EFSIF1_TXD = 1;
        EFSIF1_TXD = 0x27;
        for (j = 0; j <= 4999; j++)
            asm("nop");
        if (EFSIF1_STATUS & 1)
            s = EFSIF1_RXD;
        for (i = 0; i <= 19; i++) {
            EFSIF1_TXD = 0;
            EFSIF1_TXD = 0x55;
            for (j = 0; j <= 1999; j++)
                asm("nop");
            if (EFSIF1_STATUS & 1) {
                s = EFSIF1_RXD;
                if (s == 0)
                    break;
            }
        }
        if (++retry > 20)
            return -1;
    } while (s != 0);
    INT_FSIF01 = 0x38;
    asm("nop");
    INT_ESIF01 |= 0x18;
    asm("nop");
    EFSIF1_TXD = 3;
    EFSIF1_TXD = 0x81;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 4;
    EFSIF1_TXD = 0x7f;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 5;
    EFSIF1_TXD = 0x7f;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x18;
    EFSIF1_TXD = 0x84;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x26;
    EFSIF1_TXD = 0x48;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x27;
    EFSIF1_TXD = 0x88;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x15;
    EFSIF1_TXD = 0;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_TXD = 0x24;
    EFSIF1_TXD = 0x26;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 0x1f;
    EFSIF1_TXD = 0x1c;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    nfc_uart_recv(r, 1, &r[1]);
    EFSIF1_BRTRUN = 0;
    asm("nop");
    asm("nop");
    asm("nop");
    EFSIF1_BRTRD = 1;
    EFSIF1_BRTRUN = 1;
    asm("nop");
    cmd[0] = 0x9f;
    nfc_uart_send((u8 *)cmd, 1);
    nfc_uart_recv(r, 1, &r[1]);
    if ((s8)r[0] != 0x1c)
        return -1;
    nfc_tick_start();
    return 0;
}



/*
 * Close an NFC session (every protocol function ends with it): SIO1 interrupts off, flags
 * cleared, 100 ms tick stopped (nfc_tick_stop), PN512 IRQ input off; CommandReg 0x01 =
 * 0x2F (RcvOff + SoftReset: all registers back to reset values, SerialSpeed back to
 * 9.6 kbd, so ControlReg = 0 = target for the next session), CPU UART back to 9566 bd,
 * then read CommandReg (0x81) until RcvOff is set and PowerDown clear ((s & 0x30) ==
 * 0x20, up to 10 tries). Then the sleep set-up of nfc_init: DivIEn 0x82 (wake on
 * RfOn, nfc_wake_on_field_off == 0) or 0x81 (RfOff), clear ComIrq/DivIrq, RxThreshold 0x55, RFCfg
 * 0x59 (RxGain 38 dB, RF level detector threshold 1001 = about 0.24 Vpp), GsNOn 0xF4,
 * TxAuto 0x40 (Force100ASK), CommandReg 0x37 (RcvOff + PowerDown + NoCmdChange = soft
 * power-down: the RF level detector stays on and pulls IRQ), UART off.
 */
void nfc_link_close(void)
{
    volatile u8 s;
    u32 i, j;

    INT_ESIF01 &= 7;
    INT_FSIF01 = 0x38;
    nfc_tick_stop();
    INT_EP815 &= ~0x10;
    s = 0;
    EFSIF1_TXD = 1;
    EFSIF1_TXD = 0x2f;
    for (j = 0; j <= 4999; j++)
        asm("nop");
    while (EFSIF1_STATUS & 1) {
        s = EFSIF1_RXD;
        asm("nop");
    }
    EFSIF1_BRTRUN = 0;
    asm("nop");
    asm("nop");
    asm("nop");
    EFSIF1_BRTRD = 0xc3;
    EFSIF1_BRTRUN = 1;
    asm("nop");
    for (i = 0; i <= 9; i++) {
        EFSIF1_TXD = 0x81;
        for (j = 0; j <= 1999; j++)
            asm("nop");
        while (EFSIF1_STATUS & 1) {
            s = EFSIF1_RXD;
            asm("nop");
        }
        if ((s & 0x30) == 0x20)
            break;
    }
    INT_FP815 = 0x10;
    INT_EP815 |= 0x10;
    EFSIF1_TXD = 3;
    if (nfc_wake_on_field_off == 0)
        EFSIF1_TXD = 0x82;
    else
        EFSIF1_TXD = 0x81;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 4;
    EFSIF1_TXD = 0x7f;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 5;
    EFSIF1_TXD = 0x7f;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 0x18;
    EFSIF1_TXD = 0x55;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 0x26;
    EFSIF1_TXD = 0x59;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 0x27;
    EFSIF1_TXD = 0xf4;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 0x15;
    EFSIF1_TXD = 0x40;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_TXD = 1;
    EFSIF1_TXD = 0x37;
    for (j = 0; j <= 1999; j++)
        asm("nop");
    s = EFSIF1_RXD;
    EFSIF1_BRTRUN = 0;
    asm("nop");
    EFSIF1_CTL &= 0x3f;
}

/*
 * Send n bytes to the PN512: wait (99999 polls at most) until both rings are empty and
 * nothing is pending in RXD1, reset both rings, copy src into the transmit ring, write
 * the first byte to TXD1 and enable the transmit-buffer-empty interrupt (nfc_uart_tx_isr
 * sends the rest). Returns n, or 0 when the link stayed busy.
 */
int nfc_uart_send(u8 *src, int n)
{
    u32 t = 0;
    u32 i;

    while (nfc_tx_fifo.count != 0 || nfc_rx_fifo.count != 0 || (EFSIF1_STATUS & 1)) {
        t++;
        if (t > 99999)
            return 0;
    }
    INT_ESIF01 &= ~0x20;
    nfc_tx_fifo.rd = nfc_tx_fifo.buf;
    nfc_tx_fifo.wr = nfc_tx_fifo.buf;
    nfc_rx_fifo.rd = nfc_rx_fifo.buf;
    nfc_rx_fifo.wr = nfc_rx_fifo.buf;
    for (i = 0; i < n; i++) {
        *nfc_tx_fifo.wr = *src++;
        nfc_tx_fifo.wr++;
    }
    nfc_tx_fifo.count += i;
    asm volatile("psrclr 4");
    if (nfc_tx_fifo.count != 0) {
        EFSIF1_TXD = *nfc_tx_fifo.rd;
        nfc_tx_fifo.rd++;
        nfc_tx_fifo.count--;
    }
    asm volatile("psrset 4");
    INT_ESIF01 |= 0x20;
    return i;
}

/*
 * Read n answer bytes from the receive ring into dst (99999 polls per byte at most).
 * *st = nfc_rx_fifo.status (bit 7 = timeout; the UART error bits 0x1C would come from the
 * error ISR nfc_uart_err_isr, which reads the wrong channel, so in practice they are always
 * 0) | 1 when nfc_rx_fifo.overflow is set. Returns the number of bytes read. The callers
 * treat *st != 0 as "frame not usable".
 */
int nfc_uart_recv(u8 *dst, int n, u8 *st)
{
    u32 t = 0;
    u8 *p = dst;
    u32 i;

    for (i = 0; i < n; i++, p++) {
        while (nfc_rx_fifo.count == 0) {
            t++;
            if (t > 99999) {
                nfc_rx_fifo.status |= 0x80;
                *st = nfc_rx_fifo.status;
                return i;
            }
        }
        *p = *nfc_rx_fifo.rd;
        nfc_rx_fifo.rd++;
        asm volatile("psrclr 4");
        if (nfc_rx_fifo.count != 0)
            nfc_rx_fifo.count--;
        asm volatile("psrset 4");
    }
    *st = nfc_rx_fifo.status;
    if (nfc_rx_fifo.overflow != 0)
        *st |= 1;
    return i;
}


/*
 * Re-arm the field detector (via nfc_sleep_until_field): UART on at the current 9566 bd, wake the
 * PN512 (CommandReg 0x27, PageReg 0x55 echo handshake as in nfc_link_open; -1 after 20
 * rounds), then the sleep set-up: DivIEn 0x03 = 0x82 (IRQPushPull + RfOnIEn, nfc_wake_on_field_off
 * == 0) or 0x81 (RfOffIEn), ComIrq/DivIrq = 0x7F (clear), RxThreshold 0x55, RFCfg 0x59,
 * GsNOn 0xF4, TxAuto 0x40, CommandReg 0x37 (soft power-down), UART off. Returns 0.
 */
s8 nfc_rearm_field_detect(void)
{
    volatile s8 c;
    u32 k, j, i;

    INT_FP815 = 0x10;
    EFSIF1_BRTRUN = 1;
    asm("nop");
    EFSIF1_CTL |= 0xc0;
    k = 0;
    c = 1;
    do {
        EFSIF1_TXD = 1;
        EFSIF1_TXD = 0x27;
        for (i = 0; i <= 4999; i++)
            asm("nop");
        if (EFSIF1_STATUS & 1)
            c = EFSIF1_RXD;
        for (j = 0; j <= 0x13; j++) {
            EFSIF1_TXD = 0;
            EFSIF1_TXD = 0x55;
            for (i = 0; i <= 1999; i++)
                asm("nop");
            if (EFSIF1_STATUS & 1) {
                c = EFSIF1_RXD;
                if (c == 0)
                    break;
            }
        }
        if (++k > 20)
            return -1;
    } while (c != 0);
    EFSIF1_TXD = 3;
    if (nfc_wake_on_field_off == 0)
        EFSIF1_TXD = 0x82;
    else
        EFSIF1_TXD = 0x81;
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
