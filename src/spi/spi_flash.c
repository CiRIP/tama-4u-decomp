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

int spi_recv_byte(void)
{
}

/*
 * Vestigial SPI-flash setup: LCDC IRAM select = 1, SPI clock on (spi_clock_on), SPI enable.
 * The byte-transfer routines of this group are empty in the original (`ret`), so the calls to
 * them are left out here: only the chip select toggles remain. There is no SPI flash on the
 * board; the NOR flash on the bus holds all data.
 */
void spi_flash_setup(void)
{
    LCDC_IRAM = 1;
    spi_clock_on();
    spi_enable();
}

/*
 * Vestigial SPI-flash code: JEDEC "read ID" (CS = P64; the original sends the command 0x9F
 * and reads 3 bytes into its argument through the empty transfer routines).
 */
void spi_flash_read_id(void)
{
    spi_cs_assert();
    spi_cs_release();
}

/*
 * Vestigial SPI-flash code: write enable (command 0x06 in the original), poll the status for
 * 5 s, then command 0x60 (chip erase?) and poll again.
 */
void spi_flash_write_enable(void)
{
    int fail = 0;
    volatile u8 s;

    spi_cs_assert();
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
 * Vestigial SPI-flash code: write enable, then (in the original) command 0x52 with the 3-byte
 * argument {its argument, 0, 0} (block erase?), polling the status after each.
 */
void spi_flash_command_zero_arg(void)
{
    volatile u8 st;
    int r;

    r = 0;
    spi_cs_assert();
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
 * Vestigial SPI-flash code: write enable, then (in the original) the sector command 0x20
 * (erase?) with the address {a >> 8, a, 0} of its argument a, polling the status after each.
 */
void spi_flash_sector_command(void)
{
    volatile u8 st;
    int r;

    r = 0;
    spi_cs_assert();
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
 * Vestigial SPI-flash code: page read; the original sends the command 0x03 and the address
 * {addr >> 8, addr, 0} of its argument addr, and reads 0x100 bytes into its argument p.
 */
void spi_flash_read_page(void)
{
    spi_cs_assert();
    spi_cs_release();
}

/*
 * Vestigial SPI-flash code: write enable, then page program; the original sends the command
 * 0x02, the address {a >> 8, a, 0} of its argument a and 0x100 bytes from its argument p,
 * polling the status after each.
 */
void spi_flash_program_page(void)
{
    volatile u8 st;
    int r;

    spi_cs_assert();
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

/*
 * Vestigial SPI-flash code: read the status register (the original sends the command 0x05
 * through the empty transfer routine). The read spi_recv_byte is an empty stub too.
 */
int spi_flash_read_status(void)
{
    u8 r;

    spi_cs_assert();
    r = spi_recv_byte();
    spi_cs_release();
    return r;
}
