/*
 * Reader-mode tag access: the last raw frame, NFC-A activation, Type 2 tag identification and
 * read / write, the FeliCa poll, MIFARE write.
 *
 * Code 25305C2..2532650, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/nfc/nfc_tags.h"
#include "proto/iram.h"
#include "proto/nfc/nfc_link.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "proto/lib/nfc/nfc_dep.h"
#include "proto/lib/nfc/nfc_protocol.h"
#include "proto/lib/nfc/nfc_snep.h"

/*
 * NFC: copy the last raw frame received by the target-side detection (nfc_last_frame,
 * nfc_last_frame_len bytes) to dst.
 */
int nfc_copy_last_frame(u8 *dst, int *n)
{
    int i;

    *n = nfc_last_frame_len;
    for (i = 0; i < nfc_last_frame_len; i++)
        dst[i] = nfc_last_frame[i];
    return 0;
}

/* ISO 14443-3 Type A activation (reader side, tags and cards): configure the PN512 as a
 * 106 kbit/s Type A reader: ControlReg 0x0C = 0x10 (initiator), TxModeReg 0x12 =
 * RxModeReg 0x13 = 0 (106 kbit/s, 14443A framing, no CRC), TxControlReg 0x14 = 0x83
 * (RF field on), TxAutoReg 0x15 = 0x40 (Force100ASK), RxThreshold 0x18 = 0x85, DemodReg
 * 0x19 = 0x4D (reset value), RFCfg 0x26 = 0x79 (RxGain 48 dB), GsNOn 0x27 = 0xF4, CWGsP
 * 0x28 = 0x3F, ModGsP 0x29 = 0x11, timer 0x2A-0x2D = 0 (off), CommandReg 0x01 = 0x0C
 * (Transceive); then
 *   WUPA (0x52, not REQA 0x26: it also wakes HALTed cards; BitFraming 0x0D = 0x87 =
 *   StartSend with 7 bits) -> ATQA into *(u16 *)b;
 *   anticollision cascade level 1 (0x93 0x20, Tx/RxMode 0 = no CRC) -> 5 bytes UID part +
 *   BCC, SELECT (0x93 0x70 + UID, Tx/RxMode 0x80 = CRC on) -> SAK; if SAK & 4 (cascade
 *   bit) repeat with 0x95 (cascade level 2) for 7-byte UIDs. c = 1: do the anticollision;
 *   c = 0: p0 already holds the UID (re-select).
 * The UID is returned in p0 (bytes 4..7 = UID part, or 0 + 3 bytes + 4 bytes for the
 * double-size UID); if SAK & 0x20 (ISO 14443-4 compliant) *b is replaced by 0xEEF.
 * ATQA 0x0044 (NFC Forum Type 2 / Ultralight) is further probed by nfc_identify_type2_tag.
 * Retries up to 1000 times, closing and re-opening the front-end (nfc_link_close /
 * nfc_link_open, nfc_ticks preserved) before each new attempt; d = timeout in seconds
 * (d * 10 ticks of the 100 ms counter nfc_ticks); 3 = over-temperature, 4 = user abort,
 * -4 = timeout, 2 = no card. */
/*
 * NFC reader mode: NFC-A poll and activation (REQA, anticollision, select) of a card;
 * returns the card's ATQA-based type code in *b (0x0EEF for ISO-DEP capable cards, Type 2
 * tag codes from nfc_identify_type2_tag...).
 */
s8 nfc_activate_type_a(u8 *p0, int b, u16 c, int d)
{
    s8 r;
    u8 A[5];
    u8 B[5];
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    s8 chk;
    s8 st;
    int retry;
    int res;
    s8 sel;
    int first;
    u32 t0;
    u8 *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
    u32 i, j, k, m;
    u8 *q1, *q2, *q3;
    u32 kept;
    s8 x;

    st = 0;
    chk = 0;
    retry = 0;
    res = 0;
    first = 0;
    nfc_abort = res;
    t0 = nfc_ticks;
again:
    if (first == 1) {
        nfc_link_close();
        for (i = 0; i <= 47999; i++)
            asm("nop");
        kept = nfc_ticks;
        cfg.b1 = nfc_tx_fifo_buf;
        cfg.n1 = 0x100;
        cfg.b2 = nfc_rx_fifo_buf;
        cfg.n2 = 0x100;
        x = nfc_link_open((int)&cfg);
        if (x != 0)
            return -15;
        nfc_ticks = kept;
    }
    first = 1;
    p1 = nfc_cmd_buf;
    p1[0] = 0xc;
    p1[1] = 0x10;
    p1[2] = 0x12;
    p1[3] = 0;
    p1[4] = 0x13;
    p1[5] = 0;
    p1[6] = 0x14;
    p1[7] = 0x83;
    p1[8] = 0x15;
    p1[9] = 0x40;
    p1[10] = 0x18;
    p1[11] = 0x85;
    p1[12] = 0x19;
    p1[13] = 0x4d;
    p1[14] = 0x26;
    p1[15] = 0x79;
    p1[16] = 0x27;
    p1[17] = 0xf4;
    p1[18] = 0x28;
    p1[19] = 0x3f;
    p1[20] = 0x29;
    p1[21] = 0x11;
    p1[22] = 0x2a;
    p1[23] = 0;
    p1[24] = 0x2b;
    p1[25] = 0;
    p1[26] = 0x2c;
    p1[27] = 0;
    p1[28] = 0x2d;
    p1[29] = 0;
    p1[30] = first;
    p1[31] = 0xc;
    nfc_uart_send(p1, 0x20);
    nfc_uart_recv(p1, 0x10, (u8 *)&r);
    for (i = 0; i <= 7999; i++)
        asm("nop");
    if (retry <= 999) {
        p2 = nfc_cmd_buf;
        p2[0] = 10;
        p2[1] = 0x80;
        p2[2] = 9;
        p2[3] = 0x52;
        p2[4] = 1;
        p2[5] = 0xc;
        p2[6] = 0xd;
        p2[7] = 0x87;
        nfc_uart_send(p2, 8);
        nfc_uart_recv(p2, 4, (u8 *)&r);
        for (k = 0; k <= 999; k++) {
            q1 = nfc_cmd_buf;
            q1[0] = 0x8a;
            q1[1] = 0x88;
            q1[2] = 0x86;
            nfc_uart_send(q1, 3);
            nfc_uart_recv(q1, 3, (u8 *)&r);
            if (q1[2] & 0x40)
                return 3;
            if (nfc_cmd_buf[0] > 1 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                break;
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > d * 10)
                return -4;
            for (i = 0; i <= 99; i++)
                asm("nop");
        }
        *(u16 *)b = 0;
        if (k <= 999) {
            p3 = nfc_cmd_buf;
            p3[0] = 0x89;
            p3[1] = 0x89;
            nfc_uart_send(p3, 2);
            nfc_uart_recv(p3, 2, (u8 *)&r);
            if (r != 0) {
                retry++;
                goto again;
            }
        } else {
            retry++;
            res = -1;
            goto again;
        }
        *(u16 *)b = nfc_cmd_buf[1] << 8 | nfc_cmd_buf[0];
        res = 0;
    }
    if (retry <= 999)
        retry = 0;
    sel = 0;
    do {
        for (t0 = nfc_ticks; retry <= 999; t0 = nfc_ticks) {
            if (c == 1) {
                if (sel > 0) {
                    p4 = nfc_cmd_buf;
                    p4[0] = 0x12;
                    p4[1] = 0;
                    p4[2] = 0x13;
                    p4[3] = 0;
                    nfc_uart_send(p4, 4);
                    nfc_uart_recv(p4, 2, (u8 *)&r);
                }
                nfc_cmd_buf[0] = 10;
                nfc_cmd_buf[1] = 0x80;
                nfc_cmd_buf[2] = 9;
                if (sel == 0)
                    nfc_cmd_buf[3] = 0x93;
                else
                    nfc_cmd_buf[3] = 0x95;
                p5 = nfc_cmd_buf;
                p5[4] = 9;
                p5[5] = 0x20;
                p5[6] = 0xd;
                p5[7] = 0x80;
                nfc_uart_send(p5, 8);
                nfc_uart_recv(p5, 4, (u8 *)&r);
                for (m = 0; m <= 999; m++) {
                    q2 = nfc_cmd_buf;
                    q2[0] = 0x8a;
                    q2[1] = 0x88;
                    q2[2] = 0x86;
                    nfc_uart_send(q2, 3);
                    nfc_uart_recv(q2, 3, (u8 *)&r);
                    if (q2[2] & 0x40)
                        return 3;
                    if (nfc_cmd_buf[0] > 4 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                        break;
                    if (nfc_abort == 1)
                        return 4;
                    if (nfc_ticks - t0 > d * 10)
                        return -4;
                    for (i = 0; i <= 99; i++)
                        asm("nop");
                }
                if (m > 999)
                    goto fail1;
                for (i = 0; i <= 4; i++)
                    nfc_cmd_buf[i] = 0x89;
                p6 = nfc_cmd_buf;
                nfc_uart_send(p6, 5);
                nfc_uart_recv(p6, 5, (u8 *)&r);
                if (r != 0) {
                    retry++;
                    goto again;
                }
                for (i = 0;;) {
                    if (sel == 0)
                        A[i] = nfc_cmd_buf[i];
                    else
                        B[i] = nfc_cmd_buf[i];
                    i++;
                    res = 0;
                    if (i > 4)
                        goto L27;
                }
            } else {
                if (p0[0] == 0 && p0[1] == 0 && p0[2] == 0 && p0[3] == 0) {
                    for (i = 0; i <= 3; i++) {
                        A[i] = p0[i + 4];
                        if (i == 0)
                            chk = A[0];
                        else
                            chk ^= A[i];
                    }
                    A[4] = chk;
                    res = 0;
                } else {
                    A[0] = 0x88;
                    chk = 0x88;
                    for (i = 0; i <= 2; i++) {
                        A[i + 1] = p0[i + 1];
                        chk ^= A[i + 1];
                    }
                    A[4] = chk;
                    for (i = 0; i <= 3; i++) {
                        B[i] = p0[i + 4];
                        if (i == 0)
                            chk = B[0];
                        else
                            chk ^= B[i];
                    }
                    B[4] = chk;
                    res = 0;
                }
            }
            break;
        }
L27:
        if (retry <= 999) {
            t0 = nfc_ticks;
            retry = 0;
            p7 = nfc_cmd_buf;
            p7[0] = 10;
            p7[1] = 0x80;
            nfc_uart_send(p7, 2);
            nfc_uart_recv(p7, 1, (u8 *)&r);
            p7[0] = 9;
            if (sel == 0)
                p7[1] = 0x93;
            else
                nfc_cmd_buf[1] = 0x95;
            p8 = nfc_cmd_buf;
            p8[2] = 9;
            p8[3] = 0x70;
            p8[4] = 0x12;
            p8[5] = 0x80;
            p8[6] = 0x13;
            p8[7] = 0x80;
            nfc_uart_send(p8, 8);
            nfc_uart_recv(p8, 4, (u8 *)&r);
            for (i = 0; i <= 4; i++) {
                nfc_cmd_buf[i * 2] = 9;
                if (sel == 0)
                    nfc_cmd_buf[i * 2 + 1] = A[i];
                else
                    nfc_cmd_buf[i * 2 + 1] = B[i];
            }
            p9 = nfc_cmd_buf;
            nfc_uart_send(p9, 10);
            nfc_uart_recv(p9, 5, (u8 *)&r);
            p9[0] = 0xd;
            p9[1] = 0x80;
            nfc_uart_send(p9, 2);
            nfc_uart_recv(p9, 1, (u8 *)&r);
            for (j = 0; j <= 999; j++) {
                q3 = nfc_cmd_buf;
                q3[0] = 0x8a;
                q3[1] = 0x88;
                q3[2] = 0x86;
                nfc_uart_send(q3, 3);
                nfc_uart_recv(q3, 3, (u8 *)&r);
                if (q3[2] & 0x40)
                    return 3;
                if (nfc_cmd_buf[0] != 0 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                if (nfc_abort == 1)
                    return 4;
                if (nfc_ticks - t0 > d * 10)
                    return -4;
                for (i = 0; i <= 99; i++)
                    asm("nop");
            }
            st = 0;
            if (j <= 999) {
                p10 = nfc_cmd_buf;
                p10[0] = 0x89;
                nfc_uart_send(p10, 1);
                nfc_uart_recv(p10, 1, (u8 *)&r);
                st = p10[0];
                if (r != 0) {
                    retry++;
                    goto again;
                }
            } else {
fail1:
                retry++;
                res = -1;
                goto again;
            }
            res = 0;
            if (sel == 0) {
                for (i = 0; i <= 3; i++)
                    p0[i] = 0;
                for (i = 0; i <= 3; i++)
                    p0[i + 4] = A[i];
            } else {
                p0[0] = 0;
                for (i = 0; i <= 2; i++)
                    p0[i + 1] = A[i + 1];
                for (i = 0; i <= 3; i++)
                    p0[i + 4] = B[i];
            }
        }
        if (retry > 999)
            break;
        if (!((u8)st & 4)) {
            if ((u8)st & 0x20)
                *(u16 *)b = 0xeef;
            break;
        }
        sel++;
    } while (sel <= 1);
    x = nfc_error_code(r, 0);
    if (res == -1)
        x = 2;
    if (*(u16 *)b == 0x44)
        x = nfc_identify_type2_tag(p0, b, d);
    return x;
}

/*
 * NFC reader mode: identify a Type 2 tag by its capability container (page 3, read via
 * nfc_type2_read): BuiltinItemKeyEntry 10 = NDEF tag v1.0; data size CC[2] * 8: 48 bytes -> 0x0810,
 * 128 bytes -> 0x0812, 888 bytes -> 0x0816 (NTAG216-like); anything else -> 0x0044
 * (plain MIFARE Ultralight ATQA).
 */
s8 nfc_identify_type2_tag(u8 *p, int b, int c)
{
    char buf[16];
    s8 r;
    u32 v;

    r = nfc_type2_read((int)buf, 12, 4, p, b, c, 0);
    if (r != 0)
        return r;
    if ((u8)buf[0] == 0xe1 && (u8)buf[1] == 0x10) {
        v = (u16)buf[3] << 8 | (u16)buf[2];
        v <<= 3;
        switch (v) {
        case 0x30:
            v = 0x810;
            break;
        case 0x80:
            v = 0x812;
            break;
        case 0x378:
            v = 0x816;
            break;
        default:
            v = 0x44;
            break;
        }
    } else
        v = 0x44;
    *(u16 *)b = v;
    return 0;
}


/*
 * NFC reader mode: NFC-F (FeliCa) poll at 212 kbit/s, the second technology tried by
 * nfc_poll_tag. PN512 set-up: ControlReg 0x0C = 0x10 (initiator), TxModeReg 0x12 = 0x92
 * (CRC, 212 kbit/s, FeliCa framing), RxModeReg 0x13 = 0x96 (same + RxMultiple: keep
 * receiving, an error byte is appended to each frame), TxControl 0x14 = 0x80, TxAuto
 * 0x15 = 0x07 (InitialRFOn + Tx2/Tx1RFAutoEn: field on after initial collision
 * avoidance), RxThreshold 0x55, Demod 0x61, RFCfg 0x59, GsNOn 0xFF, CWGsP 0x3F, ModGsP
 * 0x0F, timer TMode 0x82 / TPrescaler 0xA6 / TReload 0x0030 (unused), CommandReg 0x0C
 * (Transceive). Sends SENSF_REQ LEN 06, 00, system code FF FF (any), RC 01 (ask for the
 * system code), TSN 00 (one slot) - not nfc_send_sensf_req, which is unused. A SENSF_RES of 20
 * bytes + the error byte (FIFOLevel > 0x14) with response code 01 gives: IDm (8 bytes) to
 * a, PMm (dropped), the two system-code bytes to c (in received order) and the type code
 * *(u16 *)b = 0x0FFF. d = timeout in seconds. The read/write dispatchers nfc_tag_read /
 * nfc_tag_write have no 0x0FFF case (they return 2): a FeliCa card can be detected and
 * identified, but not read or written.
 */
s8 nfc_poll_felica(int a, int b, int c, int d)
{
    s8 r;
    u8 h[8];
    int cnt;
    int res;
    u8 *p;
    u8 *q;
    u32 i, j, k;
    u32 t0;
    s8 x;
    u8 t;

    res = 0;
    nfc_abort = 0;
    cnt = 0;
    p = nfc_cmd_buf;
    p[0] = 0xc;
    p[1] = 0x10;
    p[2] = 0x12;
    p[3] = 0x92;
    p[4] = 0x13;
    p[5] = 0x96;
    p[6] = 0x14;
    p[7] = 0x80;
    p[8] = 0x15;
    p[9] = 7;
    p[10] = 0x18;
    p[11] = 0x55;
    p[12] = 0x19;
    p[13] = 0x61;
    p[14] = 0x26;
    p[15] = 0x59;
    p[16] = 0x27;
    p[17] = 0xff;
    p[18] = 0x28;
    p[19] = 0x3f;
    p[20] = 0x29;
    p[21] = 0xf;
    p[22] = 0x2a;
    p[23] = 0x82;
    p[24] = 0x2b;
    p[25] = 0xa6;
    p[26] = 0x2c;
    p[27] = 0;
    p[28] = 0x2d;
    p[29] = 0x30;
    p[30] = 1;
    p[31] = 0xc;
    nfc_uart_send(p, 0x20);
    nfc_uart_recv(p, 0x10, (u8 *)&r);
    for (k = 0; k <= 7999; k++)
        asm("nop");
    q = nfc_cmd_buf;
    q[0] = 10;
    q[1] = 0x80;
    q[2] = 9;
    q[3] = 6;
    q[4] = 9;
    q[5] = 0;
    q[6] = 9;
    q[7] = 0xff;
    q[8] = 9;
    q[9] = 0xff;
    q[10] = 9;
    q[11] = 1;
    q[12] = 9;
    q[13] = 0;
    q[14] = 13;
    q[15] = 0x80;
    nfc_uart_send(q, 0x10);
    nfc_uart_recv(q, 8, (u8 *)&r);
    t0 = nfc_ticks;
    while (cnt <= 999) {
        for (j = 0; j <= 999; j++) {
            q[0] = 0x8a;
            q[1] = 0x88;
            q[2] = 0x86;
            nfc_uart_send(q, 3);
            nfc_uart_recv(q, 3, (u8 *)&r);
            if (q[0] > 0x14 && (q[1] & 7) != 3 && (q[1] & 7) != 2 && r == 0)
                break;
            if (q[2] & 0x40)
                return 3;
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > d * 10)
                return -4;
            for (k = 0; k <= 99; k++)
                asm("nop");
        }
        if (j <= 999) {
            for (k = 0; k <= 20; k++)
                nfc_cmd_buf[k] = 0x89;
            nfc_uart_send(q, 0x15);
            nfc_uart_recv(q, 0x15, (u8 *)&r);
            if (r != 0) {
                cnt++;
            } else {
                t = q[1];
                cnt++;
                if (t == 1) {
                    for (k = 0; k <= 7; k++) {
                        ((u8 *)a)[k] = nfc_cmd_buf[k + 2];
                        h[k] = nfc_cmd_buf[k + 10];
                    }
                    *(u16 *)c = q[0x13] << 8 | q[0x12];
                    res = 0;
                    *(u16 *)b = 0xfff;
                    break;
                }
            }
        } else {
            cnt++;
            res = -1;
        }
    }
    x = nfc_error_code(r, 0);
    if (res == -1)
        x = 2;
    return x;
}


extern u8 nfc_rx_data_buf[];


/*
 * Reader mode, MIFARE Classic READ (dispatched by nfc_tag_read for ATQA 0x0004 / 0x0002):
 * c bytes from data offset b (sector trailers skipped: 3 data blocks per sector below
 * 0x600, 15 above) into a, RAM or flash (g != 0, func_0000B52). Per block: MFAuthent
 * (FIFO 60 = key A, block, key FF FF FF FF FF FF = the transport key, UID bytes d[4..7];
 * CommandReg 0x01 = 0x0E), wait for Status2 bit 3 MFCrypto1On, CommandReg = 0 (Idle),
 * wait for Command (0x81) and Error (0x86) to read 0, clear ComIrq (0x04 = 0x7F),
 * Transceive (0x0C), flush, READ 30 blk with StartSend, read the 16 bytes. tmo = timeout
 * in seconds (tmo * 10 ticks).
 */
s8 f_122C(int, u32, u32, u8 *, int, int, s8) __asm__("nfc_mifare_read");

s8 f_122C(int a, u32 b, u32 c, u8 *d, int e, int tmo, s8 g)
{
    s8 r;
    int retry;
    u8 blk;
    u32 off;
    int skip;
    u16 left;
    u16 *fp;
    u32 done;
    u32 t0;
    u32 x, i, w;
    s8 n;
    s8 rr;
    u16 v;
    u8 *s;
    u8 *buf;
    u8 *cmd;

    buf = nfc_rx_data_buf;
    cmd = nfc_cmd_buf;
    left = 0;
    fp = 0;
    retry = 0;
    if (b <= 0x5ff) {
        i = b / 0x30;
        x = (b - i * 3 * 16) >> 4;
        blk = i * 4 + x;
        off = b - (i * 3 + x) * 16;
    } else {
        i = (b - 0x600) / 0xf0;
        x = (b - i * 15 * 16 - 0x600) >> 4;
        blk = i * 16 + x + 0x80;
        off = b - (i * 15 + x) * 16 - 0x600;
    }
    skip = off;
    done = 0;
    if (g != 0)
        fp = (u16 *)a;
    while (done < c) {
        t0 = nfc_ticks;
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 0x60;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = blk;
        for (i = 0; i <= 5; i++) {
            cmd[i * 2 + 6] = 9;
            cmd[i * 2 + 7] = 0xff;
        }
        for (i = 0; i <= 3; i++) {
            nfc_cmd_buf[i * 2 + 0x12] = 9;
            nfc_cmd_buf[i * 2 + 0x13] = d[i + 4];
        }
        nfc_cmd_buf[26] = 1;
        nfc_cmd_buf[27] = 0xe;
        nfc_uart_send(nfc_cmd_buf, 0x1c);
        nfc_uart_recv(nfc_cmd_buf, 0xe, (u8 *)&r);
        while (retry <= 999) {
            nfc_cmd_buf[0] = 0x88;
            nfc_uart_send(nfc_cmd_buf, 1);
            nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
            if ((nfc_cmd_buf[0] & 8) && r == 0)
                break;
            retry++;
            for (i = 0; i <= 99; i++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > (u32)(tmo * 10))
                return -4;
        }
        nfc_cmd_buf[0] = 1;
        nfc_cmd_buf[1] = 0;
        nfc_uart_send(nfc_cmd_buf, 2);
        nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
        t0 = nfc_ticks;
        while (retry <= 999) {
            nfc_cmd_buf[0] = 0x81;
            nfc_cmd_buf[1] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&r);
            if (nfc_cmd_buf[1] & 0x40)
                return 3;
            if (nfc_cmd_buf[0] == 0 && nfc_cmd_buf[1] == 0 && r == 0)
                break;
            retry++;
            for (i = 0; i <= 99; i++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > (u32)(tmo * 10))
                return -4;
        }
        if (retry <= 999)
            retry = 0;
        t0 = nfc_ticks;
        while (retry <= 999) {
            nfc_cmd_buf[0] = 4;
            nfc_cmd_buf[1] = 0x7f;
            nfc_cmd_buf[2] = 1;
            nfc_cmd_buf[3] = 0xc;
            nfc_cmd_buf[4] = 10;
            nfc_cmd_buf[5] = 0x80;
            nfc_cmd_buf[6] = 9;
            nfc_cmd_buf[7] = 0x30;
            nfc_cmd_buf[8] = 9;
            nfc_cmd_buf[9] = blk;
            nfc_cmd_buf[10] = 0xd;
            nfc_cmd_buf[11] = 0x80;
            nfc_uart_send(nfc_cmd_buf, 0xc);
            nfc_uart_recv(nfc_cmd_buf, 6, (u8 *)&r);
            for (i = 0; i <= 999; i++) {
                nfc_cmd_buf[0] = 0x8a;
                nfc_cmd_buf[1] = 0x88;
                nfc_cmd_buf[2] = 0x86;
                nfc_uart_send(nfc_cmd_buf, 3);
                nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
                if (nfc_cmd_buf[2] & 0x40)
                    return 3;
                if (nfc_cmd_buf[0] > 0xf && (nfc_cmd_buf[1] & 7) != 3
                    && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                for (x = 0; x <= 99; x++)
                    asm("nop");
                if (nfc_abort == 1)
                    return 4;
                if (nfc_ticks - t0 > (u32)(tmo * 10))
                    return -4;
            }
            if (i > 999) {
                retry++;
                continue;
            }
            for (i = 0; i <= 999; i++) {
                for (x = 0; x <= 0xf; x++)
                    nfc_cmd_buf[x] = 0x89;
                nfc_uart_send(nfc_cmd_buf, 0x10);
                nfc_uart_recv(nfc_rx_data_buf, 0x10, (u8 *)&r);
                if (r == 0)
                    break;
            }
            if (i <= 999)
                break;
            retry++;
        }
        if (retry <= 999)
            retry = 0;
        if (retry <= 999) {
            w = c - done;
            if (w > 0x10)
                w = 0x10;
            n = w;
            if (g != 0) {
                if ((off & 1) == 0) {
                    for (i = 0; i < n - skip; i += 2) {
                        v = (buf + skip)[i] | (u16)(s8)(buf + skip)[i + 1] << 8;
                        asm volatile("psrclr 4");
                        rr = func_0000B52(fp, v, nfc_flash_base);
                        fp++;
                        asm volatile("psrset 4");
                        if (rr != 0)
                            return -6;
                    }
                } else {
                    if (c - done <= 0x10)
                        n = n + 2;
                    for (i = 0; i < n - skip - 1; i += 2) {
                        if (skip == 0) {
                            if (i == 0)
{
                                v = left;
                                v |= (u16)(s8)buf[0] << 8;
}
                            else {
                                s = buf + i;
                                v = s[-1] | (u16)(s8)s[0] << 8;
                            }
                        } else {
                            v = (buf + skip)[i] | (u16)(s8)(buf + skip)[i + 1] << 8;
                        }
                        asm volatile("psrclr 4");
                        rr = func_0000B52(fp, v, nfc_flash_base);
                        fp++;
                        asm volatile("psrset 4");
                        if (rr != 0)
                            return -6;
                    }
                    left = buf[0xf];
                }
            } else {
                for (i = 0; i < n - skip; i++)
                    ((u8 *)a + done)[i] = (buf + skip)[i];
            }
            done = done - skip + 0x10;
            blk = blk + 1;
            skip = 0;
            if (blk < 0x80) {
                if ((blk & 3) == 3)
                    blk = blk + 1;
            } else {
                if ((blk & 0xf) == 0xf)
                    blk = blk + 1;
            }
        }
        if (retry > 999) {
            r = -4;
            break;
        }
    }
    return nfc_error_code(r, 0);
}

/*
 * Reader mode, NFC Forum Type 2 (Ultralight / NTAG) READ: c bytes from byte offset b
 * (page b / 4) into a, RAM or flash (g != 0). Each READ 30 <page> (FIFO flush, (0x09,
 * byte) pairs, BitFraming 0x0D = 0x80) returns 16 bytes = 4 pages; CRC is already on
 * (Tx/RxMode 0x80 after the SELECT of nfc_activate_type_a). tmo = timeout in seconds.
 */

s8 nfc_type2_read(int a, u32 b, u32 c, u8 *d, int e, int tmo, s8 g)
{
    s8 r;
    int retry;
    u8 page;
    u32 skip0;
    u32 skip;
    u16 left;
    u16 *fp;
    u32 done;
    u32 t0;
    u32 i, w, x;
    s8 n;
    s8 rr;
    u16 v;
    u8 *s;
    u8 *buf;

    buf = nfc_rx_data_buf;
    left = 0;
    fp = 0;
    retry = 0;
    page = b >> 2;
    skip = b - page * 4;
    skip0 = skip;
    done = 0;
    if (g != 0)
        fp = (u16 *)a;
    while (done < c) {
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 0x30;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = page;
        nfc_cmd_buf[6] = 0xd;
        nfc_cmd_buf[7] = 0x80;
        nfc_uart_send(nfc_cmd_buf, 8);
        nfc_uart_recv(nfc_cmd_buf, 4, (u8 *)&r);
        t0 = nfc_ticks;
        while (retry <= 999) {
            for (i = 0; i <= 999; i++) {
                nfc_cmd_buf[0] = 0x8a;
                nfc_cmd_buf[1] = 0x88;
                nfc_cmd_buf[2] = 0x86;
                nfc_uart_send(nfc_cmd_buf, 3);
                nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
                if (nfc_cmd_buf[2] & 0x40)
                    return 3;
                if (nfc_cmd_buf[0] > 0xf && (nfc_cmd_buf[1] & 7) != 3
                    && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                for (w = 0; w <= 99; w++)
                    asm("nop");
                if (nfc_abort == 1)
                    return 4;
                if (nfc_ticks - t0 > (u32)(tmo * 10))
                    return -4;
            }
            if (i > 999) {
                retry++;
                continue;
            }
            for (i = 0; i <= 999; i++) {
                for (w = 0; w <= 0xf; w++)
                    nfc_cmd_buf[w] = 0x89;
                nfc_uart_send(nfc_cmd_buf, 0x10);
                nfc_uart_recv(nfc_rx_data_buf, 0x10, (u8 *)&r);
                if (r == 0)
                    break;
            }
            if (i <= 999)
                break;
            retry++;
        }
        if (retry <= 999)
            retry = 0;
        if (retry <= 999) {
            x = c - done;
            if (x > 0x10)
                x = 0x10;
            n = x;
            if (g != 0) {
                if ((skip0 & 1) == 0) {
                    for (i = 0; i < n - skip; i += 2) {
                        v = (buf + skip)[i] | (u16)(s8)(buf + skip)[i + 1] << 8;
                        asm volatile("psrclr 4");
                        rr = func_0000B52(fp, v, nfc_flash_base);
                        fp++;
                        asm volatile("psrset 4");
                        if (rr != 0)
                            return -6;
                    }
                } else {
                    if (c - done <= 0x10)
                        n = n + 2;
                    for (i = 0; i < n - skip - 1; i += 2) {
                        if (skip == 0) {
                            if (i == 0)
                                v = left | (u16)(s8)buf[0] << 8;
                            else {
                                s = buf + i;
                                v = s[-1] | (u16)(s8)s[0] << 8;
                            }
                        } else {
                            v = (buf + skip)[i] | (u16)(s8)(buf + skip)[i + 1] << 8;
                        }
                        asm volatile("psrclr 4");
                        rr = func_0000B52(fp, v, nfc_flash_base);
                        fp++;
                        asm volatile("psrset 4");
                        if (rr != 0)
                            return -6;
                    }
                    left = buf[0xf];
                }
            } else {
                for (i = 0; i < n - skip; i++)
                    ((u8 *)a + done)[i] = (buf + skip)[i];
            }
            done = done - skip + 0x10;
            page = page + 4;
            skip = 0;
        }
        if (retry > 999) {
            r = -4;
            break;
        }
    }
    return nfc_error_code(r, 0);
}

/*
 * Reader mode: touch card READ, after card_activate (so at 424 kbit/s), of c bytes at byte
 * offset b of file card_file_no into a (RAM, or flash when g != 0), the proprietary APDU (emu4u INS_READ 0xAD, handleRead):
 *   90 AD P1 P2 | 00 00 03 | b_hi b_mid b_lo | Le_hi Le_lo
 * with P1P2 = card_file_no (00 02 / 00 03), extended-length Lc = 3 (the 24-bit offset) and a
 * 2-byte Le = c, sent in one I-block (PCB 0A/0B + CID 0). The card answers in chained
 * I-blocks; each is acknowledged with R(ACK) (PCB AA/AB), and R(ACK) is repeated after
 * 0.5 s without an answer; a wrong block number is answered with R(ACK) and retried. The
 * data bytes (frame minus PCB/CID, and minus SW1 SW2 in the last frame) are copied to a or,
 * with g != 0, programmed as halfwords with func_0000B52 (-6 on a program error), which is
 * how spot_reader downloads the card content into flash 0x2050000. The SW (last 2 bytes)
 * goes to card_status_word and is mapped like card_write: 9000 -> 0; 6700 / 6982 / 6A80 / 6A82
 * -> -2; else -4. tmo: timeout in seconds.
 */
s8 card_read(int a, u32 b, u32 c, u8 *d, int e, int tmo, s8 g)
{
    s8 r;
    int retry;
    u16 *fp;
    u32 done;
    int last;
    u32 t0;
    u8 bn;
    s8 rl;
    u32 t1;
    u32 i, w;
    u8 n;
    u8 hdr;
    s8 rr;
    u16 v;
    u8 *p;
    u8 *buf;

    buf = nfc_rx_data_buf;
    last = 0;
    fp = 0;
    rl = 0;
    retry = 0;
    done = 0;
    if (g != 0)
        fp = (u16 *)a;
    nfc_cmd_buf[0] = 10;
    nfc_cmd_buf[1] = 0x80;
    nfc_cmd_buf[2] = 9;
    nfc_cmd_buf[3] = (s8)card_block_number | 0xa;
    bn = (s8)card_block_number;
    if ((s8)card_block_number == 0)
        card_block_number = 1;
    else
        card_block_number = 0;
    p = nfc_cmd_buf;
    p[4] = 9;
    p[5] = 0;
    p[6] = 9;
    p[7] = 0x90;
    p[8] = 9;
    p[9] = 0xad;
    p[10] = 9;
    p[11] = card_file_no >> 8;
    p[12] = 9;
    p[13] = card_file_no;
    p[14] = 9;
    p[15] = 0;
    p[16] = 9;
    p[17] = 0;
    p[18] = 9;
    p[19] = 3;
    p[20] = 9;
    p[21] = b >> 16;
    p[22] = 9;
    p[23] = b >> 8;
    p[24] = 9;
    p[25] = b;
    p[26] = 9;
    p[27] = c >> 8;
    p[28] = 9;
    p[29] = c;
    p[30] = 0xd;
    p[31] = 0x80;
    nfc_uart_send(p, 0x20);
    nfc_uart_recv(p, 0x10, (u8 *)&r);
    if (done < c + 2 && last == 0) {
        do {
            t1 = nfc_ticks;
            t0 = t1;
            i = 0;
            while (retry <= 999) {
                for (i = 0; i <= 999; i++) {
                    nfc_cmd_buf[0] = 0x8a;
                    nfc_cmd_buf[1] = 0x88;
                    nfc_cmd_buf[2] = 0x86;
                    nfc_uart_send(nfc_cmd_buf, 3);
                    nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
                    if (nfc_cmd_buf[2] & 0x40)
                        return 3;
                    rl = nfc_cmd_buf[0];
                    if (rl != 0 && (nfc_cmd_buf[1] & 7) == 1 && r == 0)
                        break;
                    for (w = 0; w <= 99; w++)
                        asm("nop");
                    if (nfc_abort == 1)
                        return 4;
                    if (nfc_ticks - t0 > (u32)(tmo * 10))
                        goto tmo;
                    if (nfc_ticks - t1 > 5) {
                        nfc_cmd_buf[0] = 10;
                        nfc_cmd_buf[1] = 0x80;
                        nfc_cmd_buf[2] = 9;
                        nfc_cmd_buf[3] = bn | 0xaa;
                        nfc_cmd_buf[4] = 9;
                        nfc_cmd_buf[5] = 0;
                        nfc_cmd_buf[6] = 0xd;
                        nfc_cmd_buf[7] = 0x80;
                        nfc_uart_send(nfc_cmd_buf, 8);
                        nfc_uart_recv(nfc_cmd_buf, 4, (u8 *)&r);
                        t1 = nfc_ticks;
                    }
                }
                if (i > 999) {
                    retry++;
                    continue;
                }
                for (i = 0; i <= 999; i++) {
                    for (w = 0; w < rl; w++)
                        nfc_cmd_buf[w] = 0x89;
                    nfc_uart_send(nfc_cmd_buf, rl);
                    nfc_uart_recv(nfc_rx_data_buf, rl, (u8 *)&r);
                    hdr = buf[0];
                    if (r == 0)
                        break;
                }
                if ((hdr & 1) != bn) {
                    nfc_cmd_buf[0] = 10;
                    nfc_cmd_buf[1] = 0x80;
                    nfc_cmd_buf[2] = 9;
                    nfc_cmd_buf[3] = bn | 0xaa;
                    nfc_cmd_buf[4] = 9;
                    nfc_cmd_buf[5] = 0;
                    nfc_cmd_buf[6] = 0xd;
                    nfc_cmd_buf[7] = 0x80;
                    nfc_uart_send(nfc_cmd_buf, 8);
                    nfc_uart_recv(nfc_cmd_buf, 4, (u8 *)&r);
                    t1 = nfc_ticks;
                    retry++;
                    continue;
                }
                if ((hdr & 0x10) == 0) {
                    last = 1;
                    n = rl - 4;
                } else
                    n = rl - 2;
                if (g != 0) {
                    for (i = 0; i < n; i += 2) {
                        v = (buf + i)[2] | (buf + i)[3] << 8;
                        asm volatile("psrclr 4");
                        rr = func_0000B52(fp, v, nfc_flash_base);
                        fp++;
                        asm volatile("psrset 4");
                        if (rr != 0)
                            return -6;
                    }
                } else {
                    for (i = 0; i < n; i++)
                        ((u8 *)a + done)[i] = (buf + i)[2];
                }
                done += n;
                if (last == 0) {
                    nfc_cmd_buf[0] = 10;
                    nfc_cmd_buf[1] = 0x80;
                    nfc_cmd_buf[2] = 9;
                    nfc_cmd_buf[3] = (s8)card_block_number | 0xaa;
                    bn = (s8)card_block_number;
                    if ((s8)card_block_number == 0)
                        card_block_number = 1;
                    else
                        card_block_number = 0;
                    nfc_cmd_buf[4] = 9;
                    nfc_cmd_buf[5] = 0;
                    nfc_cmd_buf[6] = 0xd;
                    nfc_cmd_buf[7] = 0x80;
                    nfc_uart_send(nfc_cmd_buf, 8);
                    nfc_uart_recv(nfc_cmd_buf, 4, (u8 *)&r);
                }
                break;
            }
        } while (retry <= 999 && done < c + 2 && last == 0);
    }
    card_status_word = (buf + rl)[-2] << 8 | (buf + rl)[-1];
    switch (card_status_word) {
    case 0x9000:
        goto zero;
    case 0x6700:
    case 0x6982:
    case 0x6a80:
        return -2;
    case 0x6a82:
        return -2;
    }
tmo:
    card_status_word = 0x9000;
    return -4;
zero:
    return 0;
}

/*
 * Reader mode: WRITE c bytes from a to a MIFARE Classic card (ATQA 0x0004/0x0002) at
 * data offset b (dispatched by nfc_tag_write; nfc_mifare_read is the read). Per block:
 * MFAuthent (CommandReg 0x01 = 0x0E) with key A = FF FF FF FF FF FF (fixed, not the key
 * of card_set_params) and UID d[4..7], wait for Status2 MFCrypto1On, Idle, then WRITE A0 blk
 * and the 16 data bytes, each answered by the 4-bit ACK 0x0A (ControlReg read 0x8C =
 * 0x14: Initiator + RxLastBits 4). f = timeout in seconds.
 */
s8 nfc_mifare_write(int a, int b, int c, u8 *d, int e, int f)
{
    u32 j;
    s8 r;
    int retry;
    u8 blk;
    u32 off;
    u32 t0;
    u32 i;
    u32 q;

    retry = 0;
    if ((u32)b <= 0x5ff) {
        q = (u32)b / 0x30;
        b = (u32)(b - q * 0x30) >> 4;
        blk = q * 4 + b;
    } else {
        q = ((u32)b - 0x600) / 0xf0;
        b = (u32)(b - q * 0xf0 - 0x600) >> 4;
        blk = q * 16 + b + 0x80;
    }
    for (off = 0; off < (u32)c; ) {
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 0x60;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = blk;
        for (i = 0; i <= 5; i++) {
            nfc_cmd_buf[i * 2 + 6] = (u8)(int)9;
            nfc_cmd_buf[i * 2 + 7] = 0xff;
        }
        for (i = 0; i <= 3; i++) {
            nfc_cmd_buf[i * 2 + 0x12] = 9;
            nfc_cmd_buf[i * 2 + 0x13] = d[i + 4];
        }
        nfc_cmd_buf[0x1a] = 1;
        nfc_cmd_buf[0x1b] = 0xe;
        nfc_uart_send(nfc_cmd_buf, 0x1c);
        nfc_uart_recv(nfc_cmd_buf, 0xe, (u8 *)&r);
        t0 = nfc_ticks;
        while (retry <= 999) {
            nfc_cmd_buf[0] = 0x88;
            nfc_uart_send(nfc_cmd_buf, 1);
            nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
            if ((nfc_cmd_buf[0] & 8) && r == 0)
                break;
            retry++;
            for (i = 0; i <= 99; i++)
                asm("nop");
            if (nfc_abort == 1) return 4;
            if (nfc_ticks - t0 > f * 10) return -4;
        }
        nfc_cmd_buf[0] = 1;
        nfc_cmd_buf[1] = 0;
        nfc_uart_send(nfc_cmd_buf, 2);
        nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
        t0 = nfc_ticks;
        while (retry <= 999) {
            nfc_cmd_buf[0] = 0x81;
            nfc_cmd_buf[1] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&r);
            if (nfc_cmd_buf[1] & 0x40) return 3;
            if (nfc_cmd_buf[0] == 0 && nfc_cmd_buf[1] == 0 && r == 0)
                break;
            retry++;
            for (i = 0; i <= 99; i++)
                asm("nop");
            if (nfc_abort == 1) return 4;
            if (nfc_ticks - t0 > f * 10) return -4;
        }
        if (retry <= 999)
            retry = 0;
        t0 = nfc_ticks;
        while (retry <= 999) {
            u8 *p = nfc_cmd_buf;
            p[0] = 4;
            p[1] = 0x7f;
            p[2] = 1;
            p[3] = 0xc;
            p[4] = 10;
            p[5] = 0x80;
            p[6] = 9;
            p[7] = 0xa0;
            p[8] = 9;
            p[9] = blk;
            p[10] = 0xd;
            p[11] = 0x80;
            nfc_uart_send(p, 0xc);
            nfc_uart_recv(p, 6, (u8 *)&r);
            for (j = 0; j <= 999; j++) {
                p[0] = 0x8a;
                p[1] = 0x88;
                p[2] = 0x86;
                nfc_uart_send(p, 3);
                nfc_uart_recv(p, 3, (u8 *)&r);
                if (p[2] & 0x40) return 3;
                if (nfc_cmd_buf[0] != 0 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                if (nfc_abort == 1) return 4;
                if (nfc_ticks - t0 > f * 10) return -4;
                for (i = 0; i <= 99; i++)
                    asm("nop");
            }
            p[0] = 0x8c;
            p[1] = 0x89;
            nfc_uart_send(p, 2);
            nfc_uart_recv(p, 2, (u8 *)&r);
            if (nfc_cmd_buf[0] != 0x14 || nfc_cmd_buf[1] != 10 || r != 0) {
                retry++;
                for (i = 0; i <= 99; i++)
                    asm("nop");
                continue;
            }
            for (i = 0; i <= 15; i++) {
                nfc_cmd_buf[i * 2] = 9;
                nfc_cmd_buf[i * 2 + 1] = ((u8 *)a)[off + i];
            }
            nfc_cmd_buf[32] = 0xd;
            nfc_cmd_buf[33] = 0x80;
            nfc_uart_send(p, 0x22);
            nfc_uart_recv(p, 0x11, (u8 *)&r);
            for (j = 0; j <= 999; j++) {
                nfc_cmd_buf[0] = 0x8a;
                nfc_cmd_buf[1] = 0x88;
                nfc_cmd_buf[2] = 0x86;
                nfc_uart_send(nfc_cmd_buf, 3);
                nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
                if (nfc_cmd_buf[2] & 0x40) return 3;
                if (nfc_cmd_buf[0] != 0 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                if (nfc_abort == 1) return 4;
                if (nfc_ticks - t0 > f * 10) return -4;
                for (i = 0; i <= 99; i++)
                    asm("nop");
            }
            nfc_cmd_buf[0] = 0x8c;
            nfc_cmd_buf[1] = 0x89;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&r);
            if (nfc_cmd_buf[0] == 0x14 && nfc_cmd_buf[1] == 10 && r == 0)
                break;
            retry++;
            for (i = 0; i <= 99; i++)
                asm("nop");
        }
        if (retry <= 999)
            retry = 0;
        if (retry <= 999) {
            off += 16;
            blk = blk + 1;
            if ((s8)blk >= 0) {
                if ((blk & 3) == 3)
                    blk = blk + 1;
            } else {
                if ((blk & 0xf) == 0xf)
                    blk = blk + 1;
            }
        }
        if (retry > 999) {
            r = -4;
            break;
        }
    }
    return nfc_error_code(r, 0);
}

/*
 * Reader mode: write c bytes from a to a Type 2 tag (Ultralight/NTAG: WRITE 0xA2, one
 * 4-byte page per command) from byte offset b (page b / 4); each command waits for the
 * 4-bit ACK (0x0A). f: timeout in seconds.
 */
s8 nfc_type2_write(int a, int b, int c, u8 *d, int e, int f)
{
    s8 r;
    s8 res;
    int retry;
    u8 blk;
    u32 off;
    u32 t0;
    u32 i, j;

    retry = 0;
    blk = (u32)b >> 2;
    for (off = 0; off < (u32)c; ) {
        t0 = nfc_ticks;
        while (retry <= 999) {
            nfc_cmd_buf[0] = 10;
            nfc_cmd_buf[1] = 0x80;
            nfc_cmd_buf[2] = 9;
            nfc_cmd_buf[3] = 0xa2;
            nfc_cmd_buf[4] = 9;
            nfc_cmd_buf[5] = blk;
            for (i = 0; i <= 3; i++) {
                nfc_cmd_buf[i * 2 + 6] = (u8)(int)9;
                nfc_cmd_buf[i * 2 + 7] = ((u8 *)a)[off + i];
            }
            nfc_cmd_buf[14] = 0xd;
            nfc_cmd_buf[15] = 0x80;
            nfc_uart_send(nfc_cmd_buf, 0x10);
            nfc_uart_recv(nfc_cmd_buf, 8, (u8 *)&r);
            for (j = 0; j <= 999; j++) {
                nfc_cmd_buf[0] = 0x8a;
                nfc_cmd_buf[1] = 0x88;
                nfc_cmd_buf[2] = 0x86;
                nfc_uart_send(nfc_cmd_buf, 3);
                nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
                if (nfc_cmd_buf[2] & 0x40) {
                    res = 3;
                    goto out;
                }
                if (nfc_cmd_buf[0] != 0 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                if (nfc_abort == 1) {
                    res = 4;
                    goto out;
                }
                if (nfc_ticks - t0 > f * 10) {
                    res = -4;
                    goto out;
                }
                for (i = 0; i <= 99; i++)
                    asm("nop");
            }
            nfc_cmd_buf[0] = 0x8c;
            nfc_cmd_buf[1] = 0x89;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&r);
            if (nfc_cmd_buf[0] == 0x14 && nfc_cmd_buf[1] == 10 && r == 0)
                break;
            retry++;
            for (i = 0; i <= 99; i++)
                asm("nop");
        }
        if (retry <= 999)
            retry = 0;
        if (retry <= 999) {
            off += 4;
            blk = blk + 1;
        }
        if (retry > 999) {
            r = -4;
            break;
        }
    }
    res = nfc_error_code(r, 0);
out:
    return res;
}

/*
 * The RAM variables this file defines.
 */
u8 nfc_rx_data_buf[320];
