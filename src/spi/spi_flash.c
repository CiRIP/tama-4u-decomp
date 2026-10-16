/*
 * Vestigial SPI flash driver (enable, chip select, byte transfer, read id / status, sector
 * commands, page read / program); never called.
 *
 * Code 250DC30..250DF3C, .rodata from 255C7F8. No .rodata or call evidence ties its functions
 * to a neighbour: its boundaries are a guess.
 */
#include "proto/spi/spi_flash.h"
#include "proto/tmr/tmr_timeouts.h"
#include "hw.h"

void spi_init_hook(void)
{
}

void func_250DC32(void)
{
}

/*
 * SPI module enable (SPI_CTL1 bit 0). Only used by the vestigial SPI-flash code.
 */
void spi_enable(void)
{
    SPI_CTL1 |= 1;
}

/*
 * SPI module disable (SPI_CTL1 bit 0).
 */
void spi_disable(void)
{
    SPI_CTL1 &= ~1;
}

/*
 * P64 low: chip select of the (vestigial) SPI flash asserted.
 */
void spi_cs_assert(void)
{
    P6_P6D &= ~0x10;
}

/*
 * P64 high: chip select of the (vestigial) SPI flash released.
 */
void spi_cs_release(void)
{
    P6_P6D |= 0x10;
}

void spi_send_byte(int a)
{
}

void spi_send_bytes(u8 *a, int b)
{
}

int spi_recv_byte(void)
{
}

void spi_recv_bytes(int a, int b)
{
}

/*
 * Vestigial SPI-flash setup: LCDC IRAM select = 1, SPI clock on (spi_clock_on), SPI enable.
 * The byte-transfer routines of this group are empty stubs: there is no SPI flash on the
 * board; the NOR flash on the bus holds all data.
 */
void spi_flash_setup(void)
{
    LCDC_IRAM = 1;
    spi_clock_on();
    spi_init_hook();
    spi_enable();
}

/*
 * Vestigial SPI-flash code: JEDEC "read ID" command 0x9F (CS = P64; the transfer
 * routines spi_send_byte/250DC66/250DC6A are empty stubs, so nothing is sent).
 */
void spi_flash_read_id(int p0)
{
    spi_cs_assert();
    spi_send_byte(0x9f);
    spi_recv_bytes(p0, 3);
    spi_cs_release();
}

/*
 * Vestigial SPI-flash code: write enable (0x06) and poll the status (0x05) for 5 s.
 */
void spi_flash_write_enable(void)
{
    int fail = 0;
    volatile u8 s;

    spi_cs_assert();
    spi_send_byte(6);
    spi_cs_release();
    tmr_t5_start_timeout(5000);
    for (;;) {
        s = spi_flash_read_status();
        if (s & 2)
            break;
        if (tmr_flags & 0x20) {
            fail = 1;
            break;
        }
    }
    tmr_flags &= ~0x20;
    if (fail)
        return;
    spi_cs_assert();
    spi_send_byte(0x60);
    spi_cs_release();
    tmr_t5_start_timeout(5000);
    for (;;) {
        s = spi_flash_read_status();
        if (!(s & 1))
            break;
        if (tmr_flags & 0x20)
            break;
    }
    tmr_flags &= ~0x20;
}

/*
 * Vestigial SPI-flash code: command a with a 2-byte zero argument (erase?), then poll.
 */
void spi_flash_command_zero_arg(int a)
{
    u8 buf[3];
    volatile u8 st;
    int r;

    buf[0] = a;
    buf[1] = 0;
    buf[2] = 0;
    r = 0;
    spi_cs_assert();
    spi_send_byte(6);
    spi_cs_release();
    tmr_t5_start_timeout(3000);
    for (;;) {
        st = spi_flash_read_status();
        if (st & 2)
            break;
        if (tmr_flags & 0x20) {
            r = 1;
            break;
        }
    }
    tmr_flags &= ~0x20;
    if (r == 0) {
        spi_cs_assert();
        spi_send_byte(0x52);
        spi_send_bytes(buf, 3);
        spi_cs_release();
        tmr_t5_start_timeout(3000);
        for (;;) {
            st = spi_flash_read_status();
            if (!(st & 1))
                break;
            if (tmr_flags & 0x20)
                break;
        }
        tmr_flags &= ~0x20;
    }
}

/*
 * Vestigial SPI-flash code: sector command at address a (0x20 erase?), then poll.
 */
void spi_flash_sector_command(u16 a)
{
    u8 buf[3];
    volatile u8 st;
    int r;

    buf[0] = a >> 8;
    buf[1] = a;
    buf[2] = 0;
    r = 0;
    spi_cs_assert();
    spi_send_byte(6);
    spi_cs_release();
    tmr_t5_start_timeout(2000);
    for (;;) {
        st = spi_flash_read_status();
        if (st & 2)
            break;
        if (tmr_flags & 0x20) {
            r = 1;
            break;
        }
    }
    tmr_flags &= ~0x20;
    if (r == 0) {
        spi_cs_assert();
        spi_send_byte(0x20);
        spi_send_bytes(buf, 3);
        spi_cs_release();
        tmr_t5_start_timeout(2000);
        for (;;) {
            st = spi_flash_read_status();
            if (!(st & 1))
                break;
            if (tmr_flags & 0x20)
                break;
        }
        tmr_flags &= ~0x20;
    }
}

/*
 * Vestigial SPI-flash code: read 0x100 bytes at page addr (command 0x03) into p.
 */
void spi_flash_read_page(u16 addr, int p)
{
    u8 buf[3];

    buf[0] = addr >> 8;
    buf[1] = addr;
    buf[2] = 0;
    spi_cs_assert();
    spi_send_byte(3);
    spi_send_bytes(buf, 3);
    spi_recv_bytes(p, 0x100);
    spi_cs_release();
}

/*
 * Vestigial SPI-flash code: program the page a from p (0x02), then poll.
 */
void spi_flash_program_page(u16 a, int p)
{
    u8 buf[3];
    volatile u8 st;
    int r;

    buf[0] = a >> 8;
    buf[1] = a;
    buf[2] = 0;
    spi_cs_assert();
    spi_send_byte(6);
    r = 0;
    spi_cs_release();
    tmr_t5_start_timeout(100);
    for (;;) {
        st = spi_flash_read_status();
        if (st & 2)
            break;
        if (tmr_flags & 0x20) {
            r = 1;
            break;
        }
    }
    tmr_flags &= ~0x20;
    if (r == 0) {
        spi_cs_assert();
        spi_send_byte(2);
        spi_send_bytes(buf, 3);
        spi_send_bytes((u8 *)p, 0x100);
        spi_cs_release();
        tmr_t5_start_timeout(100);
        for (;;) {
            st = spi_flash_read_status();
            if (!(st & 1))
                break;
            if (tmr_flags & 0x20)
                break;
        }
        tmr_flags &= ~0x20;
    }
}

/*
 * CMU gated clocks (pCMU_GATEDCLK1, byte accesses): turn the SPI clock on (bit 6) and
 * clear bit 19 (bit 3 of byte 0x301B06): unclear whether that is the timer-1 clock (Fable)
 * or the A/D clock (Opus). Part of the vestigial SPI-flash group (250DC50..250DF3C, setup
 * spi_flash_setup); the SPI block is never used for data.
 */
void spi_clock_on(void)
{
    CMU_PROTECT = 0x96;
    CMU_GATEDCLK1_B0 |= 0x40;
    CMU_GATEDCLK1_B2 &= ~0x08;
    CMU_PROTECT = 0;
}

void func_250DF38(void)
{
}

void func_250DF3A(void)
{
}

/*
 * Vestigial SPI-flash code: read the status register (0x05). Always returns 0 because the
 * transfer stubs do nothing.
 */
int spi_flash_read_status(void)
{
    u8 r;

    spi_cs_assert();
    spi_send_byte(5);
    r = spi_recv_byte();
    spi_cs_release();
    return r;
}
