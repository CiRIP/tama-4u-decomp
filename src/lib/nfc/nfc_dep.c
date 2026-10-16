/*
 * NFC library: card APDU sessions and NFC-DEP (detect initiator / target, send, receive).
 *
 * Code 252DAE4..252ECE0, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/nfc/nfc_dep.h"
#include "proto/iram.h"
#include "proto/nfc/nfc_link.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "proto/lib/nfc/nfc_tags.h"
#include "proto/lib/nfc/nfc_protocol.h"
#include "proto/lib/nfc/nfc_snep.h"

/*
 * NFC reader mode, touch card (ISO-DEP) only, unused (no caller, no pointer to it): select
 * the card (nfc_activate_type_a; must report 0xEEF), RATS (card_activate), then the APDU 90 F5 of
 * card_apdu_f5(a..g) with timeout h. Returns 2 for a non ISO-DEP card.
 */
s8 card_apdu_f5_session(u16 a, int b, int c, int d, int e, int f, int g, int h)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    u8 buf[8];
    u16 len;
    s8 r;

    nfc_abort = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    r = nfc_link_open((int)&cfg);
    if (r != 0)
        return -15;
    r = nfc_activate_type_a(buf, (int)&len, 1, h);
    if (r == 0) {
        if (len != 0xeef) {
            nfc_link_close();
            return 2;
        }
        r = card_activate(h);
        if (r == 0)
            r = card_apdu_f5(a, b, c, d, e, f, g, h);
    }
    nfc_link_close();
    return r;
}

/*
 * NFC reader mode: fetch the result of the last touch card (ISO-DEP) APDU: *out = ISO 7816
 * status word SW1SW2 (card_status_word, 0x9000 = OK; card_read / card_write also store 9000
 * on a timeout, so the s8 result must be checked as well), dst[0..3] = the 4 data bytes of
 * the last AUTHENTICATE answer (card_auth_answer, written only by card_activate). spot_reader
 * checks the SW and never looks at the 4 bytes.
 */
int card_last_result(u16 *out, u8 *dst)
{
    u8 i;

    *out = card_status_word;
    for (i = 0; i <= 3; i++)
        dst[i] = card_auth_answer[i];
    return 0;
}

/*
 * NFC reader mode, touch card (ISO-DEP) only, unused (no caller, no pointer to it): the
 * 8-byte key a is copied to card_auth_key with card_auth_enable = 1 (AUTHENTICATE 90 1B in
 * card_activate); select, RATS, then the APDU 90 C4 of card_apdu_c4(b, c, d) (8 bytes from
 * b, 4 bytes from c). Returns 2 when the card is not an ISO-DEP card.
 */
s8 card_apdu_c4_session(u8 *a, int b, int c, int d)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    u8 buf[8];
    u16 len;
    s8 r;
    u32 i;

    nfc_abort = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    r = nfc_link_open((int)&cfg);
    if (r != 0)
        return -15;
    for (i = 0; i <= 7; i++)
        card_auth_key[i] = a[i];
    card_auth_enable = 1;
    r = nfc_activate_type_a(buf, (int)&len, 1, d);
    if (r == 0) {
        if (len != 0xeef) {
            nfc_link_close();
            return 2;
        }
        r = card_activate(d);
        if (r == 0)
            r = card_apdu_c4(b, c, d);
    }
    nfc_link_close();
    return r;
}

/* NFC-DEP target: wait up to c seconds (c * 10 ticks of the 100 ms counter nfc_ticks,
 * which nfc_link_open has just reset) for an initiator and answer its ATR_REQ.
 * PN512 set up as an NFCIP-1 ACTIVE-mode target at 424 kbit/s (register pairs in
 * nfc_cmd_buf, PN512 datasheet 8.2): ControlReg 0x0C = 0 (target), TxModeReg 0x12 =
 * RxModeReg 0x13 = 0xA1 (CRC on, speed 010 = 424 kbit/s, framing 01 = active mode),
 * TxAutoReg 0x15 = 0x8B (AutoRFOFF + CAOn + Tx2/Tx1RFAutoEn: switch our field on, with
 * collision avoidance, once the initiator's field is off, and off again after sending),
 * RxThresholdReg 0x18 = 0x85, DemodReg 0x19 = 0x61, RFCfgReg 0x26 = 0x79 (RxGain 48 dB),
 * GsNOnReg 0x27 = 0xFF, CWGsPReg 0x28 = 0x3F, ModGsPReg 0x29 = 0x0F (driver strengths),
 * TxControlReg 0x14 = 0x80 (drivers off until TxAuto turns them on), timer TMode 0x2A =
 * 0x82 / TPrescaler 0x2B = 0xA6 (100.07 us per count) / TReload 0x2C-0x2D = 0x0020
 * (33 counts = 3.3 ms, started by TAuto at the end of each transmission; its TimerIRq is
 * never enabled nor read, so it has no effect), ModeReg 0x11 = 0x3B (reset value); then
 * FIFOLevel 0x0A = 0x80 (flush) and CommandReg 0x01 = 0x0C (Transceive).
 * Polls FIFOLevel/Status2/Error (0x8A/0x88/0x86); Error & 0x40 (TempErr) -> 3; nfc_abort
 * (user abort) -> 4. A frame counts when FIFOLevel is 0x11..0x40, Status2 ModemState is
 * not 3 (sending) and Error & 0x33 (RFErr, BufferOvfl, Parity, Protocol) is 0; the
 * listen window is re-armed every 0.5 s (5 ticks). The frame is read from the FIFO
 * (0x89) into nfc_last_frame (LEN first): if it is ATR_REQ (D4 00) and the initiator's NFCID3
 * starts with F0 40 the peer is another Tamagotchi 4U (*b = 0xF040); otherwise after 3
 * frames *b = 0xF0EF = "other device". For a Tamagotchi the ATR_RES (LEN 0x12, D5 01,
 * our NFCID3 nfc_id3, DID/BS/BR/TO/PP = 0) is sent through the FIFO (0x09) with
 * BitFraming 0x0D = 0x80 (StartSend). Returns 0, also on timeout (then *b = 0xF0EF);
 * the caller reads the peer type from *b. */

/*
 * NFC tama-to-tama, waiting side: detect another Tamagotchi acting as initiator
 * (nfc_detect_target on the other toy). Called by nfc_find_peer_target with c = 30 (2 in session type
 * 0). Configures the PN512 for active-mode NFC-DEP at 424 kbit/s as target (TxMode/RxMode
 * 0xA1, TxAuto 0x8B, timer...), starts Transceive, then polls the FIFO for a frame. A
 * received ATR_REQ (D4 00) whose NFCID3i starts with F0 40 (the marker every Tamagotchi
 * puts in its NFCID, see nfc_init) -> *b = 0xF040 and an ATR_RES with our NFCID3 is
 * sent. After 3 other frames, or no frame: *b = 0xF0EF (no Tamagotchi). Returns 0,
 * 3 (front-end error), 4 (aborted), -15. The raw frames are kept in nfc_last_frame
 * (nfc_last_frame_len = length).
 */
int nfc_detect_initiator(u8 *a, u16 *b, int c)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    s8 r;
    u8 len;
    int st;
    s8 cnt;
    int state;
    u32 t0;
    int ret;
    u8 *p;
    u8 *q;
    u32 i;
    u8 *s;

    ret = 0;
    cnt = 0;
    nfc_abort = 0;
    *b = 0xf0ef;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    ret = nfc_link_open((int)&cfg);
    if (ret != 0)
        return -15;
    for (;;) {
        p = nfc_cmd_buf;
        p[0] = 12;
        p[1] = 0;
        p[2] = 0x12;
        p[3] = 0xa1;
        p[4] = 0x13;
        p[5] = 0xa1;
        p[6] = 0x15;
        p[7] = 0x8b;
        p[8] = 0x18;
        p[9] = 0x85;
        p[10] = 0x19;
        p[11] = 0x61;
        p[12] = 0x26;
        p[13] = 0x79;
        p[14] = 0x27;
        p[15] = 0xff;
        p[16] = 0x28;
        p[17] = 0x3f;
        p[18] = 0x29;
        p[19] = 0x0f;
        p[20] = 0x14;
        p[21] = 0x80;
        p[22] = 0x2a;
        p[23] = 0x82;
        p[24] = 0x2b;
        p[25] = 0xa6;
        p[26] = 0x2c;
        p[27] = 0;
        p[28] = 0x2d;
        p[29] = 0x20;
        p[30] = 0x11;
        p[31] = 0x3b;
        nfc_cmd_buf[32] = 10;
        nfc_cmd_buf[33] = 0x80;
        nfc_cmd_buf[34] = 1;
        nfc_cmd_buf[35] = 12;
        nfc_uart_send(p, 0x24);
        nfc_uart_recv(p, 0x12, (u8 *)&r);
        { u32 j; for (j = 0; j <= 1999; j++)
            asm("nop"); }
        state = 0;
        t0 = nfc_ticks;
        q = nfc_cmd_buf;
        do {
            q[0] = 0x8a;
            q[1] = 0x88;
            q[2] = 0x86;
            nfc_uart_send(q, 3);
            nfc_uart_recv(q, 3, (u8 *)&r);
            if (q[2] & 0x40) {
                nfc_link_close();
                return 3;
            }
            nfc_last_frame_len = nfc_cmd_buf[0];
            st = nfc_cmd_buf[0];
            if (nfc_cmd_buf[0] >= 0x11 && nfc_cmd_buf[0] <= 0x40) {
                if ((nfc_cmd_buf[1] & 7) != 3) {
                    if (r == 0) {
                        if ((nfc_cmd_buf[2] & 0x33) == 0) {
                            state = 1;
                            break;
                        }
                    }
                }
            }
            for (i = 0; i <= 99; i++)
                asm("nop");
            if (nfc_abort != 0) {
                nfc_link_close();
                return 4;
            }
            if (nfc_ticks > (u32)(c * 10)) {
                nfc_link_close();
                return 0;
            }
        } while (nfc_ticks - t0 <= 4);
        if (state == 1) {
            p[0] = 0x89;
            nfc_uart_send(p, state);
            nfc_uart_recv(&len, state, (u8 *)&r);
            nfc_last_frame[0] = len;
            len--;
            for (i = 0; i < len; i++)
                nfc_cmd_buf[i] = 0x89;
            nfc_uart_send(p, len);
            nfc_uart_recv(p, len, (u8 *)&r);
            for (i = 0; i < len; i++)
                nfc_last_frame[i + 1] = nfc_cmd_buf[i];
            if (nfc_cmd_buf[0] != 0xd4)
                continue;
            if (nfc_cmd_buf[1] != 0)
                continue;
            if (nfc_cmd_buf[2] == 0xf0 && nfc_cmd_buf[3] == 0x40) {
                *b = 0xf040;
                ret = 0;
                goto out;
            }
            cnt++;
        }
        if (state == 0 && st != 0) {
            cnt++;
            p[0] = 0x89;
            nfc_uart_send(p, 1);
            nfc_uart_recv(&len, 1, (u8 *)&r);
            nfc_last_frame[0] = len;
            len--;
            for (i = 0; i < len; i++)
                nfc_cmd_buf[i] = 0x89;
            nfc_uart_send(p, len);
            nfc_uart_recv(p, len, (u8 *)&r);
            for (i = 0; i < len; i++)
                nfc_last_frame[i + 1] = nfc_cmd_buf[i];
        }
        if (cnt > 2) {
            *b = 0xf0ef;
            ret = 0;
            goto out;
        }
    }
out:
    if (*b != 0xf0ef) {
        s = nfc_cmd_buf;
        s[0] = 10;
        s[1] = 0x80;
        s[2] = 9;
        s[3] = 0x12;
        s[4] = 9;
        s[5] = 0xd5;
        s[6] = 9;
        s[7] = 1;
        s[8] = 9;
        s[9] = nfc_id3.marker0;
        s[10] = 9;
        s[11] = nfc_id3.marker1;
        s[12] = 9;
        s[13] = nfc_id3.rand0;
        s[14] = 9;
        s[15] = nfc_id3.rand1;
        s[16] = 9;
        s[17] = nfc_id3.rand2;
        s[18] = 9;
        s[19] = nfc_id3.rand3;
        s[20] = 9;
        s[21] = nfc_id3.rand4;
        s[22] = 9;
        s[23] = nfc_id3.rand5;
        s[24] = 9;
        s[25] = nfc_id3.marker2;
        s[26] = 9;
        s[27] = nfc_id3.marker3;
        s[28] = 9;
        s[29] = 0;
        s[30] = 9;
        s[31] = 0;
        nfc_cmd_buf[32] = 9;
        nfc_cmd_buf[33] = 0;
        nfc_cmd_buf[34] = 9;
        nfc_cmd_buf[35] = 0;
        nfc_cmd_buf[36] = 9;
        nfc_cmd_buf[37] = 0;
        nfc_cmd_buf[38] = 13;
        nfc_cmd_buf[39] = 0x80;
        nfc_uart_send(s, 0x28);
        nfc_uart_recv(s, 0x14, (u8 *)&r);
        for (i = 0; i <= 1999; i++)
            asm("nop");
    }
    nfc_link_close();
    return ret;
}



/*
 * NFC tama-to-tama, initiator side (nfc_find_peer_initiator, c = 30 s): set the PN512 up as an
 * NFCIP-1 active-mode initiator at 424 kbit/s (nfc_setup_active_initiator: Control 0x10, Tx/RxMode
 * 0xA1, TxAuto 0x8F, Transceive), then send ATR_REQ through the FIFO: FIFOLevel 0x0A =
 * 0x80 (flush), 17 x (0x09 FIFOData, byte) = LEN 0x11, D4 00, our NFCID3 nfc_id3 (10
 * bytes), DIDi/BSi/BRi/PPi = 0 (no general bytes), BitFraming 0x0D = 0x80 (StartSend).
 * Waits until the next 100 ms tick for an answer (FIFOLevel 0x12..0x40, ModemState not
 * TxWait/Sending, Error & 0x33 == 0; Error bit 7 WrErr -> resend); an ATR_RES (D5 01)
 * copies the peer's NFCID3 to nfc_peer_nfcid3, and an NFCID3t starting with F0 40 -> *b =
 * 0xF040 (another Tamagotchi). After 3 other frames *b = 0xF0EF. Between attempts
 * CommandReg 0x01 = 0 (Idle) and the set-up is redone. Returns 0, -4 when c seconds
 * (c * 10 ticks since nfc_link_open) pass, 3 TempErr, 4 user abort, -15 no chip.
 */
int nfc_detect_target(u8 *a, u16 *b, int c)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    s8 r;
    u8 len;
    u8 h[3];
    u32 t0;
    u8 st;
    int state;
    s8 cnt;
    int ret;
    u8 *p;
    u8 *q;
    u32 i;
    int t;
    u8 n9;

    ret = 0;
    cnt = 0;
    nfc_abort = 0;
    *b = 0xf0ef;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    ret = nfc_link_open((int)&cfg);
    if (ret != 0)
        return -15;
    n9 = 9;
    for (;;) {
        p = nfc_cmd_buf;
        nfc_setup_active_initiator();
        for (i = 0; i <= 1999; i++)
            asm("nop");
        p[0] = 10;
        p[1] = 0x80;
        p[2] = n9;
        p[3] = 0x11;
        p[4] = n9;
        p[5] = 0xd4;
        p[6] = n9;
        p[7] = 0;
        p[8] = n9;
        p[9] = nfc_id3.marker0;
        p[10] = n9;
        p[11] = nfc_id3.marker1;
        p[12] = n9;
        p[13] = nfc_id3.rand0;
        p[14] = n9;
        p[15] = nfc_id3.rand1;
        p[16] = n9;
        p[17] = nfc_id3.rand2;
        p[18] = n9;
        p[19] = nfc_id3.rand3;
        p[20] = n9;
        p[21] = nfc_id3.rand4;
        p[22] = n9;
        p[23] = nfc_id3.rand5;
        p[24] = n9;
        p[25] = nfc_id3.marker2;
        p[26] = n9;
        p[27] = nfc_id3.marker3;
        p[28] = n9;
        p[29] = 0;
        p[30] = n9;
        p[31] = 0;
        nfc_cmd_buf[32] = n9;
        nfc_cmd_buf[33] = 0;
        nfc_cmd_buf[34] = n9;
        nfc_cmd_buf[35] = 0;
        nfc_cmd_buf[36] = 13;
        nfc_cmd_buf[37] = 0x80;
        nfc_uart_send(p, 0x26);
        nfc_uart_recv(p, 0x13, (u8 *)&r);
        for (i = 0; i <= 1999; i++)
            asm("nop");
        state = 0;
        t0 = nfc_ticks;
        q = nfc_cmd_buf;
        do {
            q[0] = 0x8a;
            q[1] = 0x88;
            q[2] = 0x86;
            nfc_uart_send(q, 3);
            nfc_uart_recv(q, 3, (u8 *)&r);
            if (q[2] & 0x40) {
                nfc_link_close();
                return 3;
            }
            nfc_last_frame_len = nfc_cmd_buf[0];
            st = nfc_cmd_buf[0];
            h[0] = nfc_cmd_buf[0];
            h[1] = nfc_cmd_buf[1];
            h[2] = nfc_cmd_buf[2];
            if (h[0] >= 0x12 && h[0] <= 0x40) {
                t = nfc_cmd_buf[1] & 7;
                if (t != 3) {
                    if (t != 2) {
                        if (r == 0) {
                            if ((h[2] & 0x33) == 0) {
                                state = 1;
                                break;
                            }
                        }
                    }
                }
            }
            for (i = 0; i <= 99; i++)
                asm("nop");
            if (nfc_abort != 0) {
                nfc_link_close();
                return 4;
            }
            if (nfc_ticks > (u32)(c * 10)) {
                nfc_link_close();
                return -4;
            }
            if ((s8)q[2] < 0) {
                state = 2;
                break;
            }
        } while (nfc_ticks == t0);
        if (state == 2) {
            for (i = 0; i <= 1999; i++)
                asm("nop");
            continue;
        }
        if (state == 1) {
            p[0] = 0x89;
            nfc_uart_send(p, state);
            nfc_uart_recv(&len, state, (u8 *)&r);
            nfc_last_frame[0] = len;
            len--;
            for (i = 0; i < len; i++)
                nfc_cmd_buf[i] = 0x89;
            nfc_uart_send(p, len);
            nfc_uart_recv(p, len, (u8 *)&r);
            for (i = 0; i < len; i++)
                nfc_last_frame[i + 1] = nfc_cmd_buf[i];
            {
            u8 *q2 = nfc_cmd_buf;
            if (q2[0] == 0xd5 && q2[1] == 1) {
                for (i = 0; i <= 9; i++)
                    ((u8 *)&nfc_peer_nfcid3)[i] = nfc_cmd_buf[i + 2];
                q2 = nfc_cmd_buf;
            }
            if (q2[2] == 0xf0 && q2[3] == 0x40) {
                *b = 0xf040;
                ret = 0;
                goto out;
            }
            }
            cnt++;
        }
        if (state == 0 && st != 0) {
            t = h[1] & 7;
            if (t != 2 && t != 3)
                cnt++;
        }
        if (cnt > 2) {
            *b = 0xf0ef;
            ret = 0;
            goto out;
        }
        p[0] = 1;
        p[1] = 0;
        nfc_uart_send(p, 2);
        nfc_uart_recv(p, 1, (u8 *)&r);
        for (i = 0; i <= 4999; i++)
            asm("nop");
    }
out:
    nfc_link_close();
    return ret;
}


extern u8 nfc_rx_data_buf[];



/*
 * NFC tama-to-tama, receiving side (nfc_receive for a 0xF040 peer): the mirror of the
 * sender nfc_dep_send. Opens the front-end (nfc_link_open; the previous nfc_link_close soft
 * reset left ControlReg = 0, so the PN512 is a TARGET) and sets it up for NFCIP-1 active
 * mode at 424 kbit/s: TxMode 0x12 = RxMode 0x13 = 0xA1, TxAuto 0x15 = 0x8B (target RF
 * collision avoidance), RxThreshold 0x85, Demod 0x61, RFCfg 0x79, GsNOn 0xFF, CWGsP
 * 0x3F, ModGsP 0x0F, timer TMode 0x82 / TPrescaler 0xA6 / TReload 0x0030 (49 x 100.07 us
 * = 4.9 ms, unused), CommandReg 0x0C (Transceive). No ATR exchange here: the peers met
 * in nfc_detect_initiator / nfc_detect_target, and whichever toy sends is the initiator.
 * Each DEP_REQ (LEN, D4 06, PFB) is read from the FIFO (at most 0x40 bytes per frame);
 * the first one carries the 32-bit little-endian total length *b (c != 0 and c < *b:
 * too big, answer D5 0B = RLS_RES and return -7). The payload goes to a: programmed
 * into flash halfword by halfword (e != 0, func_0000B52 with nfc_flash_base, error -> -6) or
 * copied to RAM. Every frame is acknowledged with DEP_RES (LEN 04, D5 07, PNI); a
 * repeated PNI is acknowledged but not stored again. DSL_REQ (D4 08) or the user abort
 * ends the transfer with DSL_RES (D5 09) and returns 4. Timeouts: d seconds (d * 10
 * ticks since nfc_link_open) for the first frame, then 5 s (50 ticks) per frame; -4.
 * 3 = PN512 over-temperature (ErrorReg TempErr), -1 = UART status error, -15 no chip.
 */
int nfc_dep_receive(int a, u32 *b, u32 c, int d, s8 e)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    s8 r;
    int acc;
    u8 n;
    u8 m;
    u8 k;
    int seq;
    int pfb;
    int ret;
    u8 first;
    u8 *dst;
    int dsl;
    u32 total;
    u8 hl;
    u32 t0;
    u32 i, w;
    int tmo;
    u8 *p;

    total = 0;
    acc = total;
    ret = total;
    dsl = total;
    nfc_abort = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    ret = nfc_link_open((int)&cfg);
    if (ret != 0)
        return -15;
    p = nfc_cmd_buf;
    p[0] = 0x12;
    p[1] = 0xa1;
    p[2] = 0x13;
    p[3] = 0xa1;
    p[4] = 0x15;
    p[5] = 0x8b;
    p[6] = 0x18;
    p[7] = 0x85;
    p[8] = 0x19;
    p[9] = 0x61;
    p[10] = 0x26;
    p[11] = 0x79;
    p[12] = 0x27;
    p[13] = 0xff;
    p[14] = 0x28;
    p[15] = 0x3f;
    p[16] = 0x29;
    p[17] = 0x0f;
    p[18] = 0x2a;
    p[19] = 0x82;
    p[20] = 0x2b;
    p[21] = 0xa6;
    p[22] = 0x2c;
    p[23] = 0;
    p[24] = 0x2d;
    p[25] = 0x30;
    p[26] = 1;
    p[27] = 0xc;
    nfc_uart_send(p, 0x1c);
    nfc_uart_recv(p, 0xe, (u8 *)&r);
    for (i = 0; i <= 1999; i++)
        asm("nop");
    first = 0;
    k = 0;
    seq = 0;
    *b = total + 1;
    hl = 8;
    while (*b > total) {
        t0 = nfc_ticks;
        for (;;) {
            nfc_cmd_buf[0] = 0x8a;
            nfc_cmd_buf[1] = 0x88;
            nfc_cmd_buf[2] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 3);
            nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
            if (nfc_cmd_buf[2] & 0x40) {
                *b = total;
                nfc_link_close();
                return 3;
            }
            if (nfc_cmd_buf[0] >= hl && nfc_cmd_buf[0] <= 0x40 && (nfc_cmd_buf[1] & 7) != 3
                && r == 0 && (nfc_cmd_buf[2] & 0x37) == 0)
                break;
            for (w = 0; w <= 99; w++)
                asm("nop");
            if (nfc_abort != 0) {
                *b = total;
                dsl = 1;
                break;
            }
            if (first == 0) {
                if (nfc_ticks > (u32)(d * 10))
                    tmo = 1;
                else
                    tmo = 0;
            } else {
                if (nfc_ticks - t0 > 50)
                    tmo = 1;
                else
                    tmo = 0;
            }
            if (tmo == 1) {
                nfc_link_close();
                *b = total;
                return -4;
            }
        }
        if (dsl != 0)
            break;
        for (i = 0; i < hl; i++)
            (nfc_cmd_buf)[i] = 0x89;
        p = nfc_cmd_buf;
        nfc_uart_send(p, hl);
        nfc_uart_recv(p, hl, (u8 *)&r);
        if (p[0] > 0x40)
            continue;
        if (p[1] != 0xd4)
            continue;
        if (p[2] == 8) {
            dsl = 1;
            break;
        }
        if (nfc_cmd_buf[2] != 6)
            continue;
        if (r != 0)
            ret = -1;
        pfb = nfc_cmd_buf[3] & 3;
        n = nfc_cmd_buf[0] - hl;
        if (first == 0) {
            *b = nfc_cmd_buf[4] | nfc_cmd_buf[5] << 8 | nfc_cmd_buf[6] << 16 | nfc_cmd_buf[7] << 24;
            if (c != 0 && c < *b) {
                nfc_cmd_buf[0] = 10;
                nfc_cmd_buf[1] = 0x80;
                nfc_cmd_buf[2] = 9;
                nfc_cmd_buf[3] = 4;
                nfc_cmd_buf[4] = 9;
                nfc_cmd_buf[5] = 0xd5;
                nfc_cmd_buf[6] = 9;
                nfc_cmd_buf[7] = 0xb;
                nfc_cmd_buf[8] = 9;
                if (pfb == ((seq - 1) & 3))
                    nfc_cmd_buf[9] = pfb;
                else
                    nfc_cmd_buf[9] = seq;
                nfc_cmd_buf[10] = 0xd;
                nfc_cmd_buf[11] = 0x80;
                nfc_uart_send(nfc_cmd_buf, 0xc);
                nfc_uart_recv(nfc_cmd_buf, 6, (u8 *)&r);
                for (w = 0; w <= 1999; w++)
                    asm("nop");
                nfc_link_close();
                return -7;
            }
        }
        t0 = nfc_ticks;
        k = 0;
        dst = nfc_rx_data_buf;
        while (k < n) {
            nfc_cmd_buf[0] = 0x8a;
            nfc_cmd_buf[1] = 0x88;
            nfc_cmd_buf[2] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 3);
            nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
            if (nfc_abort != 0) {
                *b = total;
                dsl = 1;
                break;
            }
            if (first == 0) {
                if (nfc_ticks > (u32)(d * 10))
                    tmo = 1;
                else
                    tmo = 0;
            } else {
                if (nfc_ticks - t0 > 50)
                    tmo = 1;
                else
                    tmo = 0;
            }
            if (tmo == 1) {
                nfc_link_close();
                *b = total;
                return -4;
            }
            if (nfc_cmd_buf[2] & 0x40) {
                nfc_link_close();
                *b = total;
                return 3;
            }
            if (nfc_cmd_buf[0] == 0 || nfc_cmd_buf[0] > 0x40 - hl || r != 0
                || (nfc_cmd_buf[1] & 7) == 3 || (nfc_cmd_buf[1] & 7) == 2
                || (nfc_cmd_buf[2] & 0x37) != 0) {
                for (w = 0; w <= 99; w++)
                    asm("nop");
            } else {
                m = nfc_cmd_buf[0];
                for (w = 0; w < m; w++)
                    (nfc_cmd_buf)[w] = 0x89;
                nfc_uart_send(nfc_cmd_buf, m);
                nfc_uart_recv(dst, m, (u8 *)&r);
                if (r != 0)
                    ret = -1;
                dst += m;
                k += m;
            }
        }
        if (dsl != 0)
            break;
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 4;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = 0xd5;
        nfc_cmd_buf[6] = 9;
        nfc_cmd_buf[7] = 7;
        nfc_cmd_buf[8] = 9;
        if (pfb == ((seq - 1) & 3))
            nfc_cmd_buf[9] = pfb;
        else
            nfc_cmd_buf[9] = seq;
        nfc_cmd_buf[10] = 0xd;
        nfc_cmd_buf[11] = 0x80;
        nfc_uart_send(nfc_cmd_buf, 0xc);
        nfc_uart_recv(nfc_cmd_buf, 6, (u8 *)&r);
        if (seq != pfb)
            continue;
        if (e != 0) {
            for (i = 0; i < (u32)(n >> 1) + (n & 1); i++) {
                asm volatile("psrclr 4");
                acc |= func_0000B52((u16 *)(a + total) + i, ((u16 *)nfc_rx_data_buf)[i], nfc_flash_base);
                asm volatile("psrset 4");
            }
            if (acc != 0) {
                nfc_link_close();
                return -6;
            }
        } else {
            for (i = 0; i < (u32)(n >> 1) + (n & 1); i++) {
                u8 *o = (u8 *)(a + total) + i * 2;
                o[0] = ((u16 *)nfc_rx_data_buf)[i];
                o[1] = ((u16 *)nfc_rx_data_buf)[i] >> 8;
            }
        }
        total += n;
        seq = (u8)(seq + 1) & 3;
        first = 1;
        hl = 4;
    }
    if (dsl != 0) {
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 4;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = 0xd5;
        nfc_cmd_buf[6] = 9;
        nfc_cmd_buf[7] = 9;
        nfc_cmd_buf[8] = 9;
        nfc_cmd_buf[9] = seq & 3;
        nfc_cmd_buf[10] = 0xd;
        nfc_cmd_buf[11] = 0x80;
        nfc_uart_send(nfc_cmd_buf, 0xc);
        nfc_uart_recv(nfc_cmd_buf, 6, (u8 *)&r);
        ret = 4;
    }
    for (w = 0; w <= 1999; w++)
        asm("nop");
    nfc_link_close();
    return ret;
}

/* Tamagotchi-to-Tamagotchi transfer, sender side (NFC-DEP initiator, NFCIP-1 active mode
 * at 424 kbit/s; the receiver is nfc_dep_receive on the other toy). PN512 register pairs:
 * ControlReg 0x0C = 0x10 (initiator), TxMode 0x12 = RxMode 0x13 = 0xA1 (CRC, 424 kbit/s,
 * active framing), TxAuto 0x15 = 0x8F (AutoRFOFF + CAOn + InitialRFOn + Tx2/Tx1RFAutoEn:
 * initial RF collision avoidance, field on to send, off after the last bit), RxThreshold
 * 0x85, Demod 0x61, RFCfg 0x79, GsNOn 0xFF, CWGsP 0x3F, ModGsP 0x0F, timer TMode 0x82 /
 * TPrescaler 0xA6 / TReload 0x0030 (4.9 ms, unused), then CommandReg 0x01 = 0x0C
 * (Transceive); ModeReg and TxControl keep their reset values. Sends b bytes from a in
 * DEP_REQ frames (FIFO flush 0x0A = 0x80, (0x09, byte) pairs: LEN, D4 06, PFB = packet
 * number 0..3, BitFraming 0x0D = 0x80): the first frame carries the 32-bit little-endian
 * total length followed by up to 0x38 data bytes, the following ones up to 0x3C bytes
 * (64-byte FIFO).
 * After each frame it polls for the DEP_RES (D5 07 with the same PNI = ack: advance; D5 0B
 * = RLS_RES: peer released -> -7; D5 09 = DSL_RES -> 4); no answer within 2000 polls:
 * CommandReg 0x01 = 0 (Idle), TxAuto 0x8F + CommandReg 0x0C again, and resend the same
 * chunk. c = timeout in seconds (c * 10 ticks since nfc_link_open) for the first frame,
 * then 5 s (50 ticks) per frame; -4. User abort (nfc_abort) sends DSL_REQ (D4 08) and
 * returns 4. 3 = PN512 over-temperature. */
int f_252ECE0_252ECE0(void *, int, int) __asm__("nfc_dep_send");
/*
 * NFC tama-to-tama: send a buffer (a, length b, timeout c seconds) over the active-mode
 * NFC-DEP link: DEP_REQ frames D4 06 PNI of at most 64 bytes, the first one carrying the
 * total length as a 32-bit little-endian value, then 0x38/0x3C-byte data pieces; each
 * frame waits for the peer's DEP_RES.
 */
int f_252ECE0_252ECE0(void *a, int b, int c)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    s8 r;
    u32 sent;
    u32 n;
    int flag;
    int ret;
    u8 *src;
    int seq;
    int done;
    u32 t0;
    u32 k, i, cnt;
    u8 *p;
    u8 *q;
    u8 *hp;
    int to;
    int s;
    int t;

    ret = 0;
    done = 0;
    nfc_abort = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    ret = nfc_link_open((int)&cfg);
    if (ret != 0)
        return -15;
    hp = nfc_cmd_buf;
    hp[0] = 12;
    hp[1] = 0x10;
    hp[2] = 0x12;
    hp[3] = 0xa1;
    hp[4] = 0x13;
    hp[5] = 0xa1;
    hp[6] = 0x15;
    hp[7] = 0x8f;
    hp[8] = 0x18;
    hp[9] = 0x85;
    hp[10] = 0x19;
    hp[11] = 0x61;
    hp[12] = 0x26;
    hp[13] = 0x79;
    hp[14] = 0x27;
    hp[15] = 0xff;
    hp[16] = 0x28;
    hp[17] = 0x3f;
    hp[18] = 0x29;
    hp[19] = 0x0f;
    hp[20] = 0x2a;
    hp[21] = 0x82;
    hp[22] = 0x2b;
    hp[23] = 0xa6;
    hp[24] = 0x2c;
    hp[25] = 0;
    hp[26] = 0x2d;
    hp[27] = 0x30;
    hp[28] = 1;
    hp[29] = 12;
    nfc_uart_send(hp, 0x1e);
    nfc_uart_recv(hp, 0xf, (u8 *)&r);
    for (i = 0; i <= 1999; i++)
        asm("nop");
    sent = 0;
    src = a;
    seq = 0;
    flag = 0;
    t0 = nfc_ticks;
    while ((u32)b > sent) {
        if (done != 0)
            break;
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = 0xd4;
        nfc_cmd_buf[6] = 9;
        nfc_cmd_buf[7] = 6;
        nfc_cmd_buf[8] = 9;
        nfc_cmd_buf[9] = seq & 3;
        if (flag == 0) {
            nfc_cmd_buf[3] = b - sent + 8 > 0x40 ? 0x40 : b - sent + 8;
            nfc_cmd_buf[10] = 9;
            nfc_cmd_buf[11] = b;
            nfc_cmd_buf[12] = 9;
            nfc_cmd_buf[13] = (u32)b >> 8;
            nfc_cmd_buf[14] = 9;
            nfc_cmd_buf[15] = (u32)b >> 16;
            nfc_cmd_buf[16] = 9;
            nfc_cmd_buf[17] = (u32)b >> 24;
            cnt = b - sent;
            if (cnt > 0x38)
                cnt = 0x38;
            k = 0x12;
        } else {
            nfc_cmd_buf[3] = b - sent + 4 > 0x40 ? 0x40 : b - sent + 4;
            cnt = b - sent;
            if (cnt > 0x3c)
                cnt = 0x3c;
            k = 10;
        }
        n = cnt;
        for (; cnt != 0; cnt--) {
            nfc_cmd_buf[k++] = 9;
            nfc_cmd_buf[k++] = *src++;
        }
        (nfc_cmd_buf)[k++] = 13;
        (nfc_cmd_buf)[k++] = 0x80;
        nfc_uart_send(nfc_cmd_buf, k);
        nfc_uart_recv(nfc_cmd_buf, k >> 1, (u8 *)&r);
        i = 0;
        p = nfc_cmd_buf;
        for (; i <= 1999; i++) {
            p[0] = 0x8a;
            p[1] = 0x88;
            p[2] = 0x86;
            nfc_uart_send(p, 3);
            nfc_uart_recv(p, 3, (u8 *)&r);
            if (p[2] & 0x40) {
                nfc_link_close();
                return 3;
            }
            if (nfc_cmd_buf[0] >= 4 && nfc_cmd_buf[0] <= 0x40) {
                if (r == 0) {
                    t = nfc_cmd_buf[1] & 7;
                    if (t != 3) {
                        if (t != 2) {
                            if ((nfc_cmd_buf[2] & 0x37) == 0)
                                break;
                        }
                    }
                }
            }
            for (k = 0; k <= 99; k++)
                asm("nop");
            if (nfc_abort != 0) {
                done = 1;
                break;
            }
            if (flag == 0) {
                if (nfc_ticks > (u32)(c * 10))
                    to = 1;
                else
                    to = 0;
            } else {
                if (nfc_ticks - t0 > 50)
                    to = 1;
                else
                    to = 0;
            }
            if (to == 1) {
                nfc_link_close();
                return -4;
            }
        }
        if (i > 1999) {
            nfc_cmd_buf[0] = 1;
            nfc_cmd_buf[1] = 0;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
            for (k = 0; k <= 4999; k++)
                asm("nop");
            nfc_cmd_buf[0] = 0x15;
            nfc_cmd_buf[1] = 0x8f;
            nfc_cmd_buf[2] = 1;
            nfc_cmd_buf[3] = 12;
            nfc_uart_send(nfc_cmd_buf, 4);
            nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&r);
            for (i = 0; i <= 1999; i++)
                asm("nop");
            src -= n;
        } else {
            t0 = nfc_ticks;
            q = nfc_cmd_buf;
            q[0] = 0x89;
            q[1] = 0x89;
            q[2] = 0x89;
            q[3] = 0x89;
            nfc_uart_send(q, 4);
            nfc_uart_recv(q, 4, (u8 *)&r);
            k = q[3] & 3;
            if (q[1] == 0xd5 && q[2] == 0xb) {
                for (k = 0; k <= 1999; k++)
                    asm("nop");
                nfc_link_close();
                return -7;
            }
            if (nfc_cmd_buf[1] == 0xd5 && nfc_cmd_buf[2] == 9) {
                for (k = 0; k <= 1999; k++)
                    asm("nop");
                nfc_link_close();
                return 4;
            }
            if (nfc_cmd_buf[0] == 4 && nfc_cmd_buf[1] == 0xd5 && nfc_cmd_buf[2] == 7 && k == seq) {
                sent += n;
                seq = (seq + 1) & 3;
                flag = 1;
            } else {
                src -= n;
            }
            if (r != 0)
                ret = -1;
        }
    }
    if (nfc_abort != 0) {
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 4;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = 0xd4;
        nfc_cmd_buf[6] = 9;
        nfc_cmd_buf[7] = 8;
        nfc_cmd_buf[8] = 9;
        nfc_cmd_buf[9] = seq & 3;
        nfc_cmd_buf[10] = 13;
        nfc_cmd_buf[11] = 0x80;
        nfc_uart_send(nfc_cmd_buf, 12);
        nfc_uart_recv(nfc_cmd_buf, 6, (u8 *)&r);
        ret = 4;
        for (k = 0; k <= 1999; k++)
            asm("nop");
    }
    nfc_link_close();
    return ret;
}

/*
 * The RAM variables this file defines.
 */
u8 nfc_tx_fifo_buf[256];
u8 nfc_rx_fifo_buf[256];
s8 nfc_abort;
s8 nfc_last_frame_len;
u8 nfc_last_frame[340];
