/*
 * NFC reader-mode API: sleep until field, abort, card parameters, poll / read / write a tag.
 *
 * Code 252D75E..252D9C0, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/nfc/nfc_reader.h"
#include "proto/nfc/nfc_link.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_dep.h"
#include "proto/lib/nfc/nfc_tags.h"
#include "proto/lib/nfc/nfc_protocol.h"
#include "proto/lib/nfc/nfc_snep.h"

/*
 * NFC: return nfc_unused_word (not called).
 */
void nfc_get_unused_word(u32 *p)
{
    *p = nfc_unused_word;
}

/*
 * Put the NFC front-end back to "sleep until RF field": nfc_rearm_field_detect (asm) does the UART
 * wake/config/power-down; k selects the PN512 DivIEn interrupt that will pull the IRQ pin
 * (P94, ISR nfc_irq_isr sets nfc_field_irq): k = 0 RfOn (wake when a reader/phone field
 * appears, nfc_wake_on_field_off = 0, DivIEn 0x82), k = 1 RfOff (nfc_wake_on_field_off = 1, DivIEn 0x81). The chip
 * is left in soft power-down (CommandReg 0x37), where the RF level detector keeps running
 * (PN512 datasheet 11.3). nfc_wake_on_field_off also selects the DivIEn value nfc_link_close writes at
 * the end of each session. Returns -15 if the chip does not answer.
 */
int nfc_sleep_until_field(u8 k)
{
    int r = 0;

    if (k == 0)
        nfc_wake_on_field_off = 0;
    else
        nfc_wake_on_field_off = 1;
    if (nfc_rearm_field_detect() != 0)
        r = -15;
    return r;
}

/*
 * NFC: request abort of the running transfer (nfc_abort = 1; every protocol loop polls it
 * and returns 4). Called from the timer-3 100 ms tick ISR (nfc_tick_isr) when nfc_cancel_request
 * (abort request from the C button) is set, and via nfc_user_abort.
 */
s8 nfc_request_abort(void)
{
    nfc_abort = 1;
    return 0;
}

/*
 * NFC reader mode: set the touch card (ISO-DEP) parameters of the next card access:
 * card_file_no = a = P1P2 of the READ 90 AD / WRITE 90 8D APDUs, i.e. the card file number
 * (spot_reader uses 2 = the 0x3C-byte registration file, 3 = the content file; emu4u
 * resolves P2 as the file index, P1 is 0), card_auth_key = the 8-byte key sent with
 * AUTHENTICATE 90 1B by card_activate after RATS (always D_255D378), card_auth_enable = c (1 = send
 * AUTHENTICATE). It sends nothing itself: every nfc_tag_read / nfc_tag_write access
 * re-activates the card and authenticates again.
 */
s8 card_set_params(int a, u8 *src, u8 c)
{
    u8 i;

    card_file_no = a;
    for (i = 0; i <= 7; i++)
        card_auth_key[i] = src[i];
    card_auth_enable = c;
    return 0;
}

/*
 * NFC reader mode: poll for a tag/card for up to d seconds (d * 10 ticks of the 100 ms NFC
 * tick nfc_ticks; nfc_abort = user-abort flag). Wakes the PN512 (nfc_link_open with the two
 * 256-byte UART ring buffers nfc_tx_fifo_buf/D_00044E8; -15 if the chip does not answer), then
 * alternates ISO 14443 Type A polling (nfc_activate_type_a: REQA, anticollision, select; the UID
 * goes to *a (the 12-byte card header buffer), the ATQA / type code to *(u16 *)b, 0xEEF when
 * the card is ISO-DEP capable) and NFC-F polling (nfc_poll_felica, c passed through) until one
 * returns something other than -4 (timeout); the PN512 is then powered down again
 * (nfc_link_close). Returns 0 = found, 4 = aborted, -4 = timeout, -15 = no front-end.
 */
s8 nfc_poll_tag(int a, int b, int c, int d)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    u32 t0;
    s8 r;

    nfc_abort = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    r = nfc_link_open((int)&cfg);
    if (r != 0)
        return -15;
    t0 = nfc_ticks;
    for (;;) {
        r = nfc_activate_type_a((u8 *)a, b, 1, 1);
        if (r != -4)
            break;
        if (nfc_ticks - t0 >= d * 10)
            break;
        r = nfc_poll_felica(a, b, c, 1);
        if (r != -4)
            break;
        if (nfc_ticks - t0 >= d * 10)
            break;
    }
    nfc_link_close();
    return r;
}

/*
 * NFC reader mode: READ c bytes from offset / block b of a tag into a (RAM, or flash when
 * g = 1: passed through to the block functions, e.g. card_read programs the flash
 * directly). Not a write: nfc_type2_read sends the Type 2 READ command 0x30, and
 * nfc_identify_type2_tag reads the capability container with it; nfc_tag_write is the write. d =
 * UID header from nfc_poll_tag, *e = ATQA (card type), f = timeout in seconds (f * 10
 * ticks of the 100 ms NFC tick, notes/README.md item 2). Opens the front-end
 * (nfc_link_open, -15 if it does not answer), re-selects the card (nfc_activate_type_a), then by
 * type:
 *   ATQA 0x0004 / 0x0002 : MIFARE Classic (1K/4K?) (nfc_mifare_read, sector authentication);
 *   0x0044 / 0x0810 / 0x0812 / 0x0816 : Type 2 tags, MIFARE Ultralight / NTAG (see
 *     nfc_identify_type2_tag) (nfc_type2_read, READ pages);
 *   0xEEF (SAK bit 5 = ISO-DEP) : the touch card: RATS + PPS + AUTHENTICATE 90 1B
 *     (card_activate), the proprietary READ APDU 90 AD (card_read, file card_file_no, byte
 *     offset b) in chunks of at most 0x7FFE bytes, S(DESELECT) (card_deselect);
 *   anything else: 2 = unsupported card.
 * Closes the front-end (nfc_link_close) at the end.
 */
s8 nfc_tag_read(int a, int b, int c, u8 *d, int e, int f, s8 g)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    u32 len;
    s8 r;

    nfc_abort = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    r = nfc_link_open((int)&cfg);
    if (r != 0)
        return -15;
    r = nfc_activate_type_a(d, (int)&len, 0, f);
    if (r != 0)
        goto out;
    if (*(u16 *)e == 4 || *(u16 *)e == 2) {
        r = nfc_mifare_read(a, b, c, d, e, f, g);
        goto out;
    }
    if (*(u16 *)e == 0x44 || *(u16 *)e == 0x810 || *(u16 *)e == 0x812 || *(u16 *)e == 0x816) {
        r = nfc_type2_read(a, b, c, d, e, f, g);
        goto out;
    }
    r = 2;
    if (*(u16 *)e != 0xeef)
        goto out;
    r = card_activate(f);
    if (r != 0)
        goto out;
    if ((u32)c <= 0x7ffe)
        r = card_read(a, b, c, d, e, f, g);
    else {
        r = card_read(a, b, 0x7ffe, d, e, f, g);
        if (r == 0)
            r = card_read(a + 0x7ffe, b + 0x7ffe, c - 0x7ffe, d, e, f, g);
    }
    ((void (*)(int))card_deselect)(f);
out:
    nfc_link_close();
    return r;
}

/*
 * NFC reader mode: write c bytes from a to offset b of a tag; same dispatch on the ATQA /
 * card type (*e) as the read function nfc_tag_read: MIFARE Classic nfc_mifare_write (key A
 * auth 0x60, WRITE 0xA0), Ultralight/NTAG nfc_type2_write (WRITE 0xA2), the touch card
 * (ISO-DEP) after RATS + AUTHENTICATE (card_activate) card_write (proprietary WRITE APDU
 * 90 8D to file card_file_no at byte offset b, in 0x7FFE chunks), then S(DESELECT)
 * (card_deselect). Returns 2 for an unknown card type, -15 when the PN512 does not answer.
 */
s8 nfc_tag_write(int a, int b, int c, u8 *d, int e, int f)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    u32 len;
    s8 r;

    nfc_abort = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    r = nfc_link_open((int)&cfg);
    if (r != 0)
        return -15;
    r = nfc_activate_type_a(d, (int)&len, 0, f);
    if (r != 0)
        goto out;
    if (*(u16 *)e == 4 || *(u16 *)e == 2) {
        r = nfc_mifare_write(a, b, c, d, e, f);
        goto out;
    }
    if (*(u16 *)e == 0x44 || *(u16 *)e == 0x810 || *(u16 *)e == 0x812 || *(u16 *)e == 0x816) {
        r = nfc_type2_write(a, b, c, d, e, f);
        goto out;
    }
    r = 2;
    if (*(u16 *)e != 0xeef)
        goto out;
    r = card_activate(f);
    if (r != 0)
        goto out;
    if ((u32)c <= 0x7ffe)
        r = card_write(a, b, c, d, e, f);
    else {
        r = card_write(a, b, 0x7ffe, d, e, f);
        if (r == 0)
            r = card_write(a + 0x7ffe, b + 0x7ffe, c - 0x7ffe, d, e, f);
    }
    ((void (*)(int))card_deselect)(f);
out:
    nfc_link_close();
    return r;
}

/*
 * The RAM variables this file defines.
 */
u16 card_file_no;
u8 card_auth_key[8];
u32 nfc_unused_word = 0x01000000;
u8 nfc_wake_on_field_off = 0;
u8 card_block_number = 0;
u8 card_auth_enable = 0;
