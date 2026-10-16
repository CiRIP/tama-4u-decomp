/*
 * Protocol frames: ISO-DEP card commands, FeliCa SENSF, NFC-DEP ATR, LLCP and SNEP frames,
 * PN512 initiator set-ups.
 *
 * Code 253284A..25350DE, no .rodata. No .rodata or call evidence ties its functions to a
 * neighbour: its boundaries are a guess.
 */
#include "proto/lib/nfc/nfc_protocol.h"
#include "proto/lib/nfc/nfc_snep.h"
#include "proto/iram.h"
#include "proto/nfc/nfc_link.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_reader.h"
#include "proto/lib/nfc/nfc_dep.h"

/*
 * Reader mode: touch card WRITE of c bytes from a at byte offset b of file card_file_no, the
 * proprietary APDU (emu4u INS_WRITE 0x8D, handleWrite):
 *   90 8D P1 P2 | 00 Lc_hi Lc_lo | b_hi b_mid b_lo | data[c]
 * with P1P2 = card_file_no (00 02 / 00 03), Lc = c + 3 in the extended-length form, the 24-bit
 * offset and the data, no Le. Sent in I-blocks (PCB 0A/0B + CID 0) chained by PCB bit 4:
 * 0x32 data bytes in the first block (after the 4 + 3 + 3 header bytes), 0x3C in the next
 * ones, so every frame is 64 bytes with CRC (the FSD of RATS). The status word goes to
 * card_status_word: 9000 -> 0; 6700 (wrong length), 6982 (security status: not authenticated),
 * 6A80 (wrong data: beyond the end of the file in emu4u), 6A82 (file not found) -> -2;
 * anything else or a timeout -> -4. f: timeout in seconds.
 */


s8 card_write(int a, int b, int c, u8 *d, int e, int f)
{
    s8 resp[4];
    s8 chunk;
    int retries;
    u32 off;
    int done;
    int first;
    int last;
    u32 t0;
    u32 i, j;
    int n;
    u32 v;
    u8 h;
    s8 max;

    n = 0;
    done = 0;
    first = 1;
    last = 0;
    retries = 0;
    off = 0;
    while (retries <= 999 && off < (u32)c && done == 0) {
        max = first == 1 ? 0x32 : 0x3c;
        if ((u32)c - off > max)
            chunk = max;
        else {
            chunk = (u32)c - off;
            last = 1;
        }
        t0 = nfc_ticks;
        while (retries <= 999) {
            nfc_cmd_buf[0] = 10;
            nfc_cmd_buf[1] = 0x80;
            nfc_cmd_buf[2] = 9;
            h = card_block_number | 10;
            nfc_cmd_buf[3] = h;
            if (last == 0)
                nfc_cmd_buf[3] = h | 0x10;
            if (card_block_number == 0)
                card_block_number = 1;
            else
                card_block_number = 0;
            nfc_cmd_buf[4] = 9;
            nfc_cmd_buf[5] = 0;
            i = 0;
            if (first == 1) {
                nfc_cmd_buf[6] = 9;
                nfc_cmd_buf[7] = 0x90;
                nfc_cmd_buf[8] = 9;
                nfc_cmd_buf[9] = 0x8d;
                nfc_cmd_buf[10] = 9;
                nfc_cmd_buf[11] = card_file_no >> 8;
                nfc_cmd_buf[12] = 9;
                nfc_cmd_buf[13] = card_file_no;
                nfc_cmd_buf[14] = 9;
                v = (u32)c + 3;
                nfc_cmd_buf[15] = v >> 16;
                nfc_cmd_buf[16] = 9;
                nfc_cmd_buf[17] = v >> 8;
                nfc_cmd_buf[18] = 9;
                nfc_cmd_buf[19] = v;
                nfc_cmd_buf[20] = 9;
                nfc_cmd_buf[21] = (u32)b >> 16;
                nfc_cmd_buf[22] = 9;
                nfc_cmd_buf[23] = (u32)b >> 8;
                nfc_cmd_buf[24] = 9;
                nfc_cmd_buf[25] = b;
                for (; i < chunk; i++) {
                    nfc_cmd_buf[i * 2 + 0x1a] = 9;
                    nfc_cmd_buf[i * 2 + 0x1b] = ((u8 *)a)[off + i];
                }
                nfc_cmd_buf[chunk * 2 + 0x1a] = 0xd;
                nfc_cmd_buf[chunk * 2 + 0x1b] = 0x80;
                nfc_uart_send(nfc_cmd_buf, chunk * 2 + 0x1c);
                nfc_uart_recv(nfc_cmd_buf, chunk + 0xe, (u8 *)resp);
            } else {
                for (; i < chunk; i++) {
                    nfc_cmd_buf[i * 2 + 6] = 9;
                    nfc_cmd_buf[i * 2 + 7] = ((u8 *)a)[off + i];
                }
                nfc_cmd_buf[chunk * 2 + 6] = 0xd;
                nfc_cmd_buf[chunk * 2 + 7] = 0x80;
                nfc_uart_send(nfc_cmd_buf, chunk * 2 + 8);
                nfc_uart_recv(nfc_cmd_buf, chunk + 4, (u8 *)resp);
            }
            for (i = 0; i <= 999; i++) {
                nfc_cmd_buf[0] = 0x8a;
                nfc_cmd_buf[1] = 0x88;
                nfc_cmd_buf[2] = 0x86;
                nfc_uart_send(nfc_cmd_buf, 3);
                nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)resp);
                if (resp[0] != 0) {
                    retries++;
                    continue;
                }
                {
                    if (nfc_cmd_buf[2] & 0x40)
                        return 3;
                    n = (s8)nfc_cmd_buf[0];
                    if (n != 0 && (nfc_cmd_buf[1] & 7) == 1 && resp[0] == 0)
                        break;
                    for (j = 0; j < 100; j++)
                        asm("nop");
                    if (nfc_abort == 1)
                        return 4;
                    if (nfc_ticks - t0 > f * 10)
                        goto fail;
                }
            }
            if (i > 999) {
                retries++;
                continue;
            }
            for (i = 0; i <= 999; i++) {
                for (j = 0; j < n; j++)
                    nfc_cmd_buf[j] = 0x89;
                nfc_uart_send(nfc_cmd_buf, n);
                nfc_uart_recv(nfc_cmd_buf, n, (u8 *)resp);
                if (resp[0] == 0)
                    break;
            }
            first = 0;
            if (n > 3)
                done = 1;
            off += chunk;
            break;
        }
    }

    card_status_word = nfc_cmd_buf[n - 2] << 8 | nfc_cmd_buf[n - 1];
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
fail:
    card_status_word = 0x9000;
    return -4;
zero:
    return 0;
}

/* ISO 14443-4 activation of a selected Type 4 card (the touch card, notes/findings/cards.md):
 * RATS (E0 50: FSDI 5 = FSD 64 bytes, CID 0) with CRC on; ATS bytes 5..11 must read
 * "TNPS064" (the historical bytes after TL T0 TA TB TC), else -2: only that card product is
 * accepted. Then PPS (PPSS D0 = CID 0, PPS0 11 = PPS1 follows, PPS1 0A = DSI 2 / DRI 2, i.e.
 * 424 kbit/s both ways) which must be echoed with D0; then the PN512 is switched to Type A
 * at 424 kbit/s: ControlReg 0x0C = 0x10, TxModeReg 0x12 = RxModeReg 0x13 = 0xA0 (CRC on,
 * speed 010 = 424 kbit/s, framing 00 = ISO 14443A), TxAuto 0x15 = 0, RxThreshold 0x85,
 * Demod 0x4D, RFCfg 0x79, ModWidthReg 0x24 = 0x0A (Miller pulse 11/fc = 0.8 us for 424k),
 * GsNOn 0xF4, CWGsP 0x3F, ModGsP 0x11, CommandReg 0x0C (Transceive). If card_auth_enable is set, send in an I-block
 * (PCB 0A/0B with CID 0, block number card_block_number) the AUTHENTICATE APDU
 *   90 1B 00 00 08 <8-byte key card_auth_key> 04
 * (emu4u: INS_AUTHENTICATE 0x1B; the key is always D_255D378). There is no ISO 7816 SELECT
 * anywhere: the applet must be the card's default selected application. The answer is
 * 4 data bytes + SW: the data go to card_auth_answer, the SW to card_status_word, and neither is checked
 * (spot_reader never reads them; emu4u answers 0A 0B 0C 0D 9000); only a status-only
 * answer (no data, e.g. a refused key) gives -2. a = timeout in seconds (100 ms ticks). */
/*
 * NFC reader mode: ISO 14443-4 activation of the selected card: RATS (E0 50), ATS check
 * "TNPS064", PPS to 424 kbit/s, then the AUTHENTICATE APDU 90 1B (see above). Needed for the
 * 0x0EEF (ISO-DEP) cards.
 */
s8 card_activate(int a)
{
    s8 buf[4];
    int k;
    u32 t0;
    u32 i, j;
    s8 r;
    int n;
    int t;

    k = 0;
    card_block_number = k;
    t0 = nfc_ticks;
    while (k <= 999) {
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 0xe0;
        nfc_cmd_buf[4] = 0x26;
        nfc_cmd_buf[5] = 0x79;
        nfc_cmd_buf[6] = 0x28;
        nfc_cmd_buf[7] = 0x3f;
        nfc_cmd_buf[8] = 9;
        nfc_cmd_buf[9] = 0x50;
        nfc_cmd_buf[10] = 0xd;
        nfc_cmd_buf[11] = 0x80;
        nfc_uart_send(nfc_cmd_buf, 12);
        nfc_uart_recv(nfc_cmd_buf, 6, (u8 *)buf);
        for (i = 0; i <= 999; i++) {
            nfc_cmd_buf[0] = 0x8a;
            nfc_cmd_buf[1] = 0x88;
            nfc_cmd_buf[2] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 3);
            nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)buf);
            if (nfc_cmd_buf[2] & 0x40)
                return 3;
            if (nfc_cmd_buf[0] > 11) {
                t = nfc_cmd_buf[1] & 7;
                if (t != 3) {
                    if (t != 2) {
                        if (buf[0] == 0)
                            break;
                    }
                }
            }
            for (j = 0; j < 100; j++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > a * 10)
                return -4;
        }
        if (i > 999)
            return -4;
        for (i = 0; i <= 999; i++) {
            for (j = 0; j < 12; j++)
                (nfc_cmd_buf)[j] = 0x89;
            nfc_uart_send(nfc_cmd_buf, 12);
            nfc_uart_recv(nfc_cmd_buf, 12, (u8 *)buf);
            if (buf[0] == 0)
                break;
        }
        if (i <= 999) {
            if (nfc_cmd_buf[5] == 0x54 && nfc_cmd_buf[6] == 0x4e && nfc_cmd_buf[7] == 0x50
                && nfc_cmd_buf[8] == 0x53 && nfc_cmd_buf[9] == 0x30 && nfc_cmd_buf[10] == 0x36
                && nfc_cmd_buf[11] == 0x34)
                break;
            return -2;
        }
        k++;
    }
    t0 = nfc_ticks;
    while (k <= 999) {
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 0xd0;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = 0x11;
        nfc_cmd_buf[6] = 9;
        nfc_cmd_buf[7] = 10;
        nfc_cmd_buf[8] = 0xd;
        nfc_cmd_buf[9] = 0x80;
        nfc_uart_send(nfc_cmd_buf, 10);
        nfc_uart_recv(nfc_cmd_buf, 5, (u8 *)buf);
        for (i = 0; i <= 999; i++) {
            nfc_cmd_buf[0] = 0x8a;
            nfc_cmd_buf[1] = 0x88;
            nfc_cmd_buf[2] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 3);
            nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)buf);
            if (nfc_cmd_buf[2] & 0x40)
                return 3;
            if (nfc_cmd_buf[0] != 0) {
                if ((nfc_cmd_buf[1] & 7) == 1) {
                    if (buf[0] == 0)
                        break;
                }
            }
            for (j = 0; j < 100; j++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > a * 10)
                return -4;
        }
        if (i > 999)
            return -4;
        for (i = 0; i <= 999; i++) {
            for (j = 0; j < 1; j++)
                (nfc_cmd_buf)[j] = 0x89;
            nfc_uart_send(nfc_cmd_buf, 1);
            nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)buf);
            if (buf[0] == 0)
                break;
        }
        if (i <= 999) {
            if (nfc_cmd_buf[0] == 0xd0)
                break;
            return -2;
        }
        k++;
    }
    r = 0;
    while (k <= 999) {
        nfc_cmd_buf[0] = 12;
        nfc_cmd_buf[1] = 0x10;
        nfc_cmd_buf[2] = 0x12;
        nfc_cmd_buf[3] = 0xa0;
        nfc_cmd_buf[4] = 0x13;
        nfc_cmd_buf[5] = 0xa0;
        nfc_cmd_buf[6] = 0x15;
        nfc_cmd_buf[7] = 0;
        nfc_cmd_buf[8] = 0x18;
        nfc_cmd_buf[9] = 0x85;
        nfc_cmd_buf[10] = 0x19;
        nfc_cmd_buf[11] = 0x4d;
        nfc_cmd_buf[12] = 0x26;
        nfc_cmd_buf[13] = 0x79;
        nfc_cmd_buf[14] = 0x24;
        nfc_cmd_buf[15] = 10;
        nfc_cmd_buf[16] = 0x27;
        nfc_cmd_buf[17] = 0xf4;
        nfc_cmd_buf[18] = 0x28;
        nfc_cmd_buf[19] = 0x3f;
        nfc_cmd_buf[20] = 0x29;
        nfc_cmd_buf[21] = 0x11;
        nfc_cmd_buf[22] = 1;
        nfc_cmd_buf[23] = 12;
        nfc_uart_send(nfc_cmd_buf, 0x18);
        nfc_uart_recv(nfc_cmd_buf, 12, (u8 *)buf);
        break;
    }
    for (i = 0; i < 2000; i++)
        asm("nop");
    if (k <= 999)
        k = 0;
    t0 = nfc_ticks;
    while (k <= 999) {
        if (card_auth_enable == 0)
            break;
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = card_block_number | 10;
        if (card_block_number == 0)
            card_block_number = 1;
        else
            card_block_number = 0;
        nfc_cmd_buf[4] = 9;
        nfc_cmd_buf[5] = 0;
        nfc_cmd_buf[6] = 9;
        nfc_cmd_buf[7] = 0x90;
        nfc_cmd_buf[8] = 9;
        nfc_cmd_buf[9] = 0x1b;
        nfc_cmd_buf[10] = 9;
        nfc_cmd_buf[11] = 0;
        nfc_cmd_buf[12] = 9;
        nfc_cmd_buf[13] = 0;
        nfc_cmd_buf[14] = 9;
        nfc_cmd_buf[15] = 8;
        for (i = 0; i <= 7; i++) {
            nfc_cmd_buf[i * 2 + 0x10] = 9;
            nfc_cmd_buf[i * 2 + 0x11] = card_auth_key[i];
        }
        nfc_cmd_buf[0x20] = 9;
        nfc_cmd_buf[0x21] = 4;
        nfc_cmd_buf[0x22] = 0xd;
        nfc_cmd_buf[0x23] = 0x80;
        nfc_uart_send(nfc_cmd_buf, 0x24);
        nfc_uart_recv(nfc_cmd_buf, 0x12, (u8 *)buf);
        for (i = 0; i <= 999; i++) {
            nfc_cmd_buf[0] = 0x8a;
            nfc_cmd_buf[1] = 0x88;
            nfc_cmd_buf[2] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 3);
            nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)buf);
            if (nfc_cmd_buf[2] & 0x40)
                return 3;
            n = (s8)nfc_cmd_buf[0];
            if (nfc_cmd_buf[0] > 7) {
                t = nfc_cmd_buf[1] & 7;
                if (t != 3) {
                    if (t != 2) {
                        if (buf[0] == 0)
                            break;
                    }
                }
            }
            for (j = 0; j < 100; j++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > a * 10)
                return -4;
        }
        for (j = 0; j < n; j++)
            nfc_cmd_buf[j] = 0x89;
        nfc_uart_send(nfc_cmd_buf, n);
        nfc_uart_recv(nfc_cmd_buf, n, (u8 *)buf);
        if (n == 4) {
            card_status_word = nfc_cmd_buf[2] << 8 | nfc_cmd_buf[3];
            r = -2;
            break;
        }
        for (j = 0; j <= 3; j++)
            card_auth_answer[j] = nfc_cmd_buf[j + 2];
        card_status_word = nfc_cmd_buf[6] << 8 | nfc_cmd_buf[7];
        r = 0;
        break;
    }
    if (k > 999)
        r = -4;
    return r;
}

/*
 * NFC reader mode: ISO 14443-4 S(DESELECT) (C2) at the end of an ISO-DEP card session.
 */
int card_deselect(int a)
{
    s8 buf[4];
    int ret = 0;
    int k = 0;
    u32 t0 = nfc_ticks;
    u32 i, j;
    u8 *p;
    int t;
    s8 c80;

    do {
        p = nfc_cmd_buf;
        p[0] = 10;
        c80 = 0x80;
        p[1] = c80;
        p[2] = 9;
        p[3] = 0xc2;
        p[4] = 13;
        p[5] = c80;
        nfc_uart_send(p, 6);
        nfc_uart_recv(p, 3, (u8 *)buf);
        for (i = 0; i <= 999; i++) {
            p[0] = 0x8a;
            p[1] = 0x88;
            p[2] = 0x86;
            nfc_uart_send(p, 3);
            nfc_uart_recv(p, 3, (u8 *)buf);
            if (p[2] & 0x40)
                return 3;
            if (nfc_cmd_buf[0] != 0) {
                t = nfc_cmd_buf[1] & 7;
                if (t != 3) {
                    if (t != 2) {
                        if (buf[0] == 0)
                            break;
                    }
                }
            }
            for (j = 0; j < 100; j++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > a * 10)
                return -4;
        }
        if (i > 999)
            return -4;
        for (i = 0; i <= 999; i++) {
            for (j = 0; j < 1; j++)
                (nfc_cmd_buf)[j] = 0x89;
            nfc_uart_send(nfc_cmd_buf, 1);
            nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)buf);
            if (buf[0] == 0)
                break;
        }
        if (i <= 999)
            break;
        k++;
    } while (k <= 999);
    if (k > 999)
        ret = -4;
    return ret;
}

/*
 * Reader mode, touch card: APDU 90 F5 P1 P2 00 (P1P2 = file card_file_no, Le 00), expecting
 * 12 data bytes + 9000, returned as *o1 = 24-bit value (bytes 0..2), *o2..*o4 = bytes 3, 4,
 * 5, *o5 = 24-bit (6..8), *o6 = 24-bit (9..11); another SW -> -2. Probably a "get file
 * info" (sizes / offsets of the file; guess). Only reached through the unused
 * card_apdu_f5_session; not implemented by emu4u (it would answer 6D00).
 */
int card_apdu_f5(int unused, int o1, int o2, int o3, int o4, int o5, int o6, int a)
{
    u8 *q;
    s8 buf[4];
    int ret = 0;
    int k = 0;
    u32 t0 = nfc_ticks;
    u32 i, j;
    u8 *p;
    int t;
    int n;
    u8 *hp = nfc_cmd_buf;

    do {
        hp[0] = 10;
        hp[1] = 0x80;
        hp[2] = 9;
        hp[3] = card_block_number | 10;
        if (card_block_number == 0)
            card_block_number = 1;
        else
            card_block_number = 0;
        hp[4] = 9;
        hp[5] = 0;
        hp[6] = 9;
        hp[7] = 0x90;
        hp[8] = 9;
        hp[9] = 0xf5;
        hp[10] = 9;
        hp[11] = card_file_no >> 8;
        hp[12] = 9;
        hp[13] = card_file_no;
        hp[14] = 9;
        hp[15] = 0;
        hp[16] = 13;
        hp[17] = 0x80;
        nfc_uart_send(hp, 18);
        nfc_uart_recv(hp, 9, (u8 *)buf);
        i = 0;
        p = nfc_cmd_buf;
        for (; i <= 999; i++) {
            p[0] = 0x8a;
            p[1] = 0x88;
            p[2] = 0x86;
            nfc_uart_send(p, 3);
            nfc_uart_recv(p, 3, (u8 *)buf);
            if (p[2] & 0x40)
                return 3;
            n = (s8)nfc_cmd_buf[0];
            if (n > 15) {
                t = nfc_cmd_buf[1] & 7;
                if (t != 3) {
                    if (t != 2) {
                        if (buf[0] == 0)
                            break;
                    }
                }
            }
            for (j = 0; j < 100; j++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > a * 10)
                return -4;
        }
        if (i > 999) {
            k++;
            continue;
        }
        {
            for (i = 0; i <= 999; i++) {
                for (j = 0; j < n; j++)
                    (nfc_cmd_buf)[j] = 0x89;
                nfc_uart_send(nfc_cmd_buf, n);
                nfc_uart_recv(nfc_cmd_buf, n, (u8 *)buf);
                if (buf[0] == 0)
                    break;
            }
            q = nfc_cmd_buf;
            if (q[14] != 0x90 || q[15] != 0) {
                card_status_word = q[2] << 8 | q[3];
                return -2;
            }
            card_status_word = nfc_cmd_buf[15] | nfc_cmd_buf[14] << 8;
            if (i <= 999)
                break;
        }
        k++;
    } while (k <= 999);
    if (k <= 999)
        k = 0;
    if (k <= 999) {
        *(u32 *)o1 = nfc_cmd_buf[2] << 16 | nfc_cmd_buf[3] << 8 | nfc_cmd_buf[4];
        *(u8 *)o2 = nfc_cmd_buf[5];
        *(u8 *)o3 = nfc_cmd_buf[6];
        *(u8 *)o4 = nfc_cmd_buf[7];
        *(u32 *)o5 = nfc_cmd_buf[8] << 16 | nfc_cmd_buf[9] << 8 | nfc_cmd_buf[10];
        *(u32 *)o6 = nfc_cmd_buf[11] << 16 | nfc_cmd_buf[12] << 8 | nfc_cmd_buf[13];
    }
    if (k > 999)
        ret = -4;
    return ret;
}

/*
 * Reader mode, touch card: APDU 90 C4 00 00 0C <8 bytes from a> <4 bytes from b>, status
 * only expected (9000 -> 0, else -2 with the SW in card_status_word); c = timeout in seconds. Only
 * reached through the unused card_apdu_c4_session. Not implemented by emu4u (it would answer 6D00);
 * an 8-byte key followed by 4 bytes suggests a key change or the second half of a
 * challenge-response authentication (guess).
 */


int card_apdu_c4(int a, int b, int c)
{
    s8 buf[4];
    int ret = 0;
    int k = 0;
    u32 t0 = nfc_ticks;
    u32 i, j;
    u8 *p;
    int t;
    int n;
    u8 hi, lo;
    int v;
    u8 *hp;

    do {
        hp = nfc_cmd_buf;
        hp[0] = 10;
        hp[1] = 0x80;
        hp[2] = 9;
        hp[3] = card_block_number | 10;
        if (card_block_number == 0)
            card_block_number = 1;
        else
            card_block_number = 0;
        hp[4] = 9;
        hp[5] = 0;
        hp[6] = 9;
        hp[7] = 0x90;
        hp[8] = 9;
        hp[9] = 0xc4;
        hp[10] = 9;
        hp[11] = 0;
        hp[12] = 9;
        hp[13] = 0;
        hp[14] = 9;
        hp[15] = 12;
        for (i = 0; i <= 7; i++) {
            u8 *q = nfc_cmd_buf + i * 2;
            q[0x10] = 9;
            q[0x11] = ((u8 *)a)[i];
        }
        for (i = 0; i <= 3; i++) {
            u8 *q = nfc_cmd_buf + i * 2;
            q[0x20] = 9;
            q[0x21] = ((u8 *)b)[i];
        }
        nfc_cmd_buf[40] = 13;
        nfc_cmd_buf[41] = 0x80;
        nfc_uart_send(hp, 0x2a);
        nfc_uart_recv(hp, 0x15, (u8 *)buf);
        p = nfc_cmd_buf;
        for (i = 0; i <= 999; i++) {
            p[0] = 0x8a;
            p[1] = 0x88;
            p[2] = 0x86;
            nfc_uart_send(p, 3);
            nfc_uart_recv(p, 3, (u8 *)buf);
            if (p[2] & 0x40)
                return 3;
            n = (s8)nfc_cmd_buf[0];
            if (n > 3) {
                t = nfc_cmd_buf[1] & 7;
                if (t != 3) {
                    if (t != 2) {
                        if (buf[0] == 0)
                            break;
                    }
                }
            }
            for (j = 0; j < 100; j++)
                asm("nop");
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > c * 10)
                return -4;
        }
        if (i > 999) {
            k++;
            continue;
        }
        for (i = 0; i <= 999; i++) {
            for (j = 0; j < n; j++)
                (nfc_cmd_buf)[j] = 0x89;
            nfc_uart_send(nfc_cmd_buf, n);
            nfc_uart_recv(nfc_cmd_buf, n, (u8 *)buf);
            if (buf[0] == 0)
                break;
        }
        hi = nfc_cmd_buf[2];
        v = hi << 8;
        lo = nfc_cmd_buf[3];
        card_status_word = lo | v;
        if (hi != 0x90 || lo != 0)
            return -2;
        if (i <= 999)
            break;
        k++;
    } while (k <= 999);
    if (k > 999)
        ret = -4;
    return ret;
}

/*
 * NFC: convert the UART status a (> 0 -> -1) and the PN512 ErrorReg bits b (0x40 TempErr
 * -> 3 over-temperature, 0x10 BufferOvfl -> -5, any other error bit -> -2; the highest
 * set bit wins) into the NFC result code, 0 when none.
 */
s8 nfc_error_code(s8 a, s8 b)
{
    s8 r = 0;

    if (a != 0) {
        if (a > 0)
            r = -1;
    }
    if (b != 0) {
        if ((b & 0x01) > 0) r = -2;
        if ((b & 0x02) > 0) r = -2;
        if ((b & 0x04) > 0) r = -2;
        if ((b & 0x08) > 0) r = -2;
        if ((b & 0x10) > 0) r = -5;
        if ((b & 0x20) > 0) r = -2;
        if ((b & 0x40) > 0) r = 3;
        if ((b & 0x80) > 0) r = -2;
    }
    return r;
}

/*
 * NFC reader mode: send a FeliCa polling command SENSF_REQ (LEN 06, 00, system code FF FF
 * = any, RC 00 = no request data, TSN 01 = two time slots) through the FIFO with
 * StartSend. Unused (no caller): nfc_poll_felica builds its own request (RC 01, TSN 00).
 */
void nfc_send_sensf_req(void)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 6;
    p[4] = 9;
    p[5] = 0;
    p[6] = 9;
    p[7] = 0xff;
    p[8] = 9;
    p[9] = 0xff;
    p[10] = 9;
    p[11] = 0;
    p[12] = 9;
    p[13] = 1;
    p[14] = 13;
    p[15] = 0x80;
    nfc_uart_send(p, 16);
    nfc_uart_recv(p, 8, buf);
}

/*
 * NFC target: answer an NFC-F / FeliCa poll with SENSF_RES: LEN 0x12, response code 01,
 * NFCID2 (IDm) = 01 FE ("NFC-DEP capable") + nfc_id3[0..5] = 01 FE F0 40 r r r r (the
 * F0 40 Tamagotchi marker and 4 bytes of the random NFCID3 made by nfc_init), PMm =
 * C0 C1 C2 C3 C4 C5 C6 C7 (the sample PMm of NXP's NFC-DEP target examples), no system
 * code (18-byte form). Written as (0x09 FIFOData, byte) pairs after a FIFO flush (0x0A =
 * 0x80), sent with BitFraming 0x0D = 0x80; the PN512 is not reconfigured here.
 * Called only from the frame dispatch of nfc_snep_server / nfc_snep_client, for a received
 * SENSF_REQ with system code FF FF and RC 00 (frame 06 00 FF FF 00 TSN). In practice it is
 * never sent: those functions only ever run the PN512 as a passive 106 kbit/s Type A
 * initiator (nfc_setup_passive_initiator; the active branch with nfc_setup_active_initiator is dead, mode is always
 * 0), a set-up that can neither receive a 212/424 kbit/s FeliCa frame nor be polled by
 * the phone, which is the target. A leftover of a generic NFC-DEP target state machine.
 */
void nfc_send_sensf_res(void)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 0x12;
    p[4] = 9;
    p[5] = 1;
    p[6] = 9;
    p[7] = 1;
    p[8] = 9;
    p[9] = 0xfe;
    p[10] = 9;
    p[11] = nfc_id3.marker0;
    p[12] = 9;
    p[13] = nfc_id3.marker1;
    p[14] = 9;
    p[15] = nfc_id3.rand0;
    p[16] = 9;
    p[17] = nfc_id3.rand1;
    p[18] = 9;
    p[19] = nfc_id3.rand2;
    p[20] = 9;
    p[21] = nfc_id3.rand3;
    p[22] = 9;
    p[23] = 0xc0;
    p[24] = 9;
    p[25] = 0xc1;
    p[26] = 9;
    p[27] = 0xc2;
    p[28] = 9;
    p[29] = 0xc3;
    p[30] = 9;
    p[31] = 0xc4;
    nfc_cmd_buf[32] = 9;
    nfc_cmd_buf[33] = 0xc5;
    nfc_cmd_buf[34] = 9;
    nfc_cmd_buf[35] = 0xc6;
    nfc_cmd_buf[36] = 9;
    nfc_cmd_buf[37] = 0xc7;
    nfc_cmd_buf[38] = 13;
    nfc_cmd_buf[39] = 0x80;
    nfc_uart_send(p, 40);
    nfc_uart_recv(p, 20, buf);
}

/*
 * NFC-DEP initiator (we talk to a phone): send ATR_REQ = LEN 0x1E, D4 00, NFCID3i =
 * nfc_id3 (10 bytes), DIDi 0, BSi 0, BRi 0, PPi = 0x32 (a == 0: LRi = 254 bytes,
 * general bytes present) or 0x02 (64 bytes), then the LLCP general bytes: magic 46 66 6D
 * ("Ffm"), TLV 01 01 11 (VERSION 1.1; 0x10 = 1.0 when a != 0), 03 02 00 13 (WKS: SAPs 0,
 * 1, 4 = LLC link mgmt, SDP, SNEP; 0x0001 when a != 0), 04 01 96 (LTO = 150 x 10 ms =
 * 1.5 s, the LLCP unit). Sent at 106 kbit/s passive after nfc_poll_type_a selected the
 * phone and ModeReg was set to 0x7B (DetectSync: the PN512 adds the NFC-DEP start byte F0;
 * Tx/RxMode 0x80 = CRC on, 14443A framing). a = 1 only on paths that are never taken
 * (the active-mode branch and the SENSF_RES case of nfc_snep_server).
 */
void nfc_send_atr_req(s8 a)
{
    u8 buf[4];
    u8 *p;

    nfc_cmd_buf[0] = 10;
    nfc_cmd_buf[1] = 0x80;
    nfc_cmd_buf[2] = 9;
    nfc_cmd_buf[3] = 0x1e;
    nfc_cmd_buf[4] = 9;
    nfc_cmd_buf[5] = 0xd4;
    nfc_cmd_buf[6] = 9;
    nfc_cmd_buf[7] = 0;
    nfc_cmd_buf[8] = 9;
    nfc_cmd_buf[9] = nfc_id3.marker0;
    nfc_cmd_buf[10] = 9;
    nfc_cmd_buf[11] = nfc_id3.marker1;
    nfc_cmd_buf[12] = 9;
    nfc_cmd_buf[13] = nfc_id3.rand0;
    nfc_cmd_buf[14] = 9;
    nfc_cmd_buf[15] = nfc_id3.rand1;
    nfc_cmd_buf[16] = 9;
    nfc_cmd_buf[17] = nfc_id3.rand2;
    nfc_cmd_buf[18] = 9;
    nfc_cmd_buf[19] = nfc_id3.rand3;
    nfc_cmd_buf[20] = 9;
    nfc_cmd_buf[21] = nfc_id3.rand4;
    nfc_cmd_buf[22] = 9;
    nfc_cmd_buf[23] = nfc_id3.rand5;
    nfc_cmd_buf[24] = 9;
    nfc_cmd_buf[25] = nfc_id3.marker2;
    nfc_cmd_buf[26] = 9;
    nfc_cmd_buf[27] = nfc_id3.marker3;
    nfc_cmd_buf[28] = 9;
    nfc_cmd_buf[29] = 0;
    nfc_cmd_buf[30] = 9;
    nfc_cmd_buf[31] = 0;
    nfc_cmd_buf[32] = 9;
    nfc_cmd_buf[33] = 0;
    nfc_cmd_buf[34] = 9;
    nfc_cmd_buf[35] = a == 0 ? 0x32 : 2;
    nfc_cmd_buf[36] = 9;
    nfc_cmd_buf[37] = 0x46;
    nfc_cmd_buf[38] = 9;
    nfc_cmd_buf[39] = 0x66;
    nfc_cmd_buf[40] = 9;
    nfc_cmd_buf[41] = 0x6d;
    nfc_cmd_buf[42] = 9;
    nfc_cmd_buf[43] = 1;
    nfc_cmd_buf[44] = 9;
    nfc_cmd_buf[45] = 1;
    nfc_cmd_buf[46] = 9;
    if (a == 0)
        nfc_cmd_buf[47] = 0x11;
    else
        nfc_cmd_buf[47] = 0x10;
    nfc_cmd_buf[48] = 9;
    nfc_cmd_buf[49] = 3;
    nfc_cmd_buf[50] = 9;
    nfc_cmd_buf[51] = 2;
    nfc_cmd_buf[52] = 9;
    nfc_cmd_buf[53] = 0;
    nfc_cmd_buf[54] = 9;
    nfc_cmd_buf[55] = a == 0 ? 0x13 : 1;
    p = nfc_cmd_buf;
    nfc_cmd_buf[56] = 9;
    nfc_cmd_buf[57] = 4;
    nfc_cmd_buf[58] = 9;
    nfc_cmd_buf[59] = 1;
    nfc_cmd_buf[60] = 9;
    nfc_cmd_buf[61] = 0x96;
    nfc_cmd_buf[62] = 13;
    nfc_cmd_buf[63] = 0x80;
    nfc_uart_send(p, 0x40);
    nfc_uart_recv(p, 0x20, buf);
}

/*
 * NFC-DEP target: send ATR_RES (answer to a phone's ATR_REQ when the phone starts the
 * link): LEN 0x1F, D5 01, NFCID3t = nfc_id3 (10 bytes), DIDt 0, BSt 0, BRt 0, TO = 0x0E
 * (waiting time), PPt = 0x32 / 0x02, then the same LLCP general bytes as nfc_send_atr_req:
 * magic 46 66 6D, VERSION 1.1 (1.0 when a != 0), WKS 0x0013 / 0x0001, LTO 0x96.
 * Only reachable from the D4 00 case of nfc_snep_server / nfc_snep_client, which cannot occur
 * while the toy is the passive initiator (see nfc_send_sensf_res): never sent in practice.
 */
void nfc_send_atr_res(s8 a)
{
    u8 buf[4];
    u8 *p;

    nfc_cmd_buf[0] = 10;
    nfc_cmd_buf[1] = 0x80;
    nfc_cmd_buf[2] = 9;
    nfc_cmd_buf[3] = 0x1f;
    nfc_cmd_buf[4] = 9;
    nfc_cmd_buf[5] = 0xd5;
    nfc_cmd_buf[6] = 9;
    nfc_cmd_buf[7] = 1;
    nfc_cmd_buf[8] = 9;
    nfc_cmd_buf[9] = nfc_id3.marker0;
    nfc_cmd_buf[10] = 9;
    nfc_cmd_buf[11] = nfc_id3.marker1;
    nfc_cmd_buf[12] = 9;
    nfc_cmd_buf[13] = nfc_id3.rand0;
    nfc_cmd_buf[14] = 9;
    nfc_cmd_buf[15] = nfc_id3.rand1;
    nfc_cmd_buf[16] = 9;
    nfc_cmd_buf[17] = nfc_id3.rand2;
    nfc_cmd_buf[18] = 9;
    nfc_cmd_buf[19] = nfc_id3.rand3;
    nfc_cmd_buf[20] = 9;
    nfc_cmd_buf[21] = nfc_id3.rand4;
    nfc_cmd_buf[22] = 9;
    nfc_cmd_buf[23] = nfc_id3.rand5;
    nfc_cmd_buf[24] = 9;
    nfc_cmd_buf[25] = nfc_id3.marker2;
    nfc_cmd_buf[26] = 9;
    nfc_cmd_buf[27] = nfc_id3.marker3;
    nfc_cmd_buf[28] = 9;
    nfc_cmd_buf[29] = 0;
    nfc_cmd_buf[30] = 9;
    nfc_cmd_buf[31] = 0;
    nfc_cmd_buf[32] = 9;
    nfc_cmd_buf[33] = 0;
    nfc_cmd_buf[34] = 9;
    nfc_cmd_buf[35] = 0xe;
    nfc_cmd_buf[36] = 9;
    nfc_cmd_buf[37] = a == 0 ? 0x32 : 2;
    nfc_cmd_buf[38] = 9;
    nfc_cmd_buf[39] = 0x46;
    nfc_cmd_buf[40] = 9;
    nfc_cmd_buf[41] = 0x66;
    nfc_cmd_buf[42] = 9;
    nfc_cmd_buf[43] = 0x6d;
    nfc_cmd_buf[44] = 9;
    nfc_cmd_buf[45] = 1;
    nfc_cmd_buf[46] = 9;
    nfc_cmd_buf[47] = 1;
    nfc_cmd_buf[48] = 9;
    if (a == 0)
        nfc_cmd_buf[49] = 0x11;
    else
        nfc_cmd_buf[49] = 0x10;
    nfc_cmd_buf[50] = 9;
    nfc_cmd_buf[51] = 3;
    nfc_cmd_buf[52] = 9;
    nfc_cmd_buf[53] = 2;
    nfc_cmd_buf[54] = 9;
    nfc_cmd_buf[55] = 0;
    nfc_cmd_buf[56] = 9;
    nfc_cmd_buf[57] = a == 0 ? 0x13 : 1;
    p = nfc_cmd_buf;
    nfc_cmd_buf[58] = 9;
    nfc_cmd_buf[59] = 4;
    nfc_cmd_buf[60] = 9;
    nfc_cmd_buf[61] = 1;
    nfc_cmd_buf[62] = 9;
    nfc_cmd_buf[63] = 0x96;
    nfc_cmd_buf[64] = 13;
    nfc_cmd_buf[65] = 0x80;
    nfc_uart_send(p, 0x42);
    nfc_uart_recv(p, 0x21, buf);
}

/*
 * NFC-DEP target: DEP_RES supervisory PDU (D5 07 80 = ATN answer).
 */
void nfc_send_dep_res_atn(void)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 4;
    p[4] = 9;
    p[5] = 0xd5;
    p[6] = 9;
    p[7] = 7;
    p[8] = 9;
    p[9] = 0x80;
    p[10] = 13;
    p[11] = 0x80;
    nfc_uart_send(p, 12);
    nfc_uart_recv(p, 6, buf);
}

/*
 * LLCP, we are the NFC-DEP initiator: send DEP_REQ (D4 06, PFB = PNI k & 3) carrying an
 * LLCP SYMM PDU (00 00: DSAP 0, PTYPE 0, SSAP 0): the initiator-side keep-alive that
 * gives the phone a chance to talk.
 */
void nfc_llcp_send_symm_req(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 6;
    p[4] = 9;
    p[5] = 0xd4;
    p[6] = 9;
    p[7] = 6;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0;
    p[12] = 9;
    p[13] = 0;
    p[14] = 13;
    p[15] = 0x80;
    nfc_uart_send(p, 16);
    nfc_uart_recv(p, 8, buf);
}

/*
 * LLCP, we are the NFC-DEP target: send DEP_RES (D5 07, PFB = information PDU with packet
 * number k & 3) carrying an LLCP SYMM PDU (00 00): the target-side keep-alive answer.
 * (FIFO flush, 6-byte frame, StartSend.)
 */
void nfc_llcp_send_symm_res(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 6;
    p[4] = 9;
    p[5] = 0xd5;
    p[6] = 9;
    p[7] = 7;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0;
    p[12] = 9;
    p[13] = 0;
    p[14] = 13;
    p[15] = 0x80;
    nfc_uart_send(p, 16);
    nfc_uart_recv(p, 8, buf);
}

/*
 * LLCP: send CONNECT from our SAP 0x20 to SAP 4 (SNEP) (bytes 11 20): start of a SNEP
 * client session (we push data to the phone).
 */
void nfc_llcp_send_connect(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 6;
    p[4] = 9;
    p[5] = 0xd4;
    p[6] = 9;
    p[7] = 6;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0x11;
    p[12] = 9;
    p[13] = 0x20;
    p[14] = 13;
    p[15] = 0x80;
    nfc_uart_send(p, 16);
    nfc_uart_recv(p, 8, buf);
}

/*
 * LLCP: send DEP_REQ (D4 06, PFB k) with a CC (connection complete) PDU: header 81 84 =
 * DSAP 0x20 (the phone's SSAP), PTYPE 6, SSAP 4 (our SNEP server), parameter TLV 02 02 00 00
 * (MIUX = 0: 128-byte MIU). Answer to the phone's CONNECT in nfc_snep_server.
 */
void nfc_llcp_send_cc(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 10;
    p[4] = 9;
    p[5] = 0xd4;
    p[6] = 9;
    p[7] = 6;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0x81;
    p[12] = 9;
    p[13] = 0x84;
    p[14] = 9;
    p[15] = 2;
    p[16] = 9;
    p[17] = 2;
    p[18] = 9;
    p[19] = 0;
    p[20] = 9;
    p[21] = 0;
    p[22] = 13;
    p[23] = 0x80;
    nfc_uart_send(p, 24);
    nfc_uart_recv(p, 12, buf);
}

/*
 * LLCP: send DM (disconnected mode) 81 C1 02 from SAP 1 to SAP 0x20, reason 2: the
 * phone tried to connect to the service discovery SAP 1, which we don't offer.
 */
void nfc_llcp_send_dm_sdp(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 7;
    p[4] = 9;
    p[5] = 0xd4;
    p[6] = 9;
    p[7] = 6;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0x81;
    p[12] = 9;
    p[13] = 0xc1;
    p[14] = 9;
    p[15] = 2;
    p[16] = 13;
    p[17] = 0x80;
    nfc_uart_send(p, 18);
    nfc_uart_recv(p, 9, buf);
}


extern s8 nfc_snep_put_template[];


void nfc_snep_send_put(u8 seq, u8 b, u8 **pp, u32 *len)
{
    s8 r;
    u32 n, chunk, off, j;

    if (b == 0) {
        n = *len + 0x39;
        if (n > 0x40)
            n = 0x40;
        chunk = n - 0x39;
    } else {
        n = *len + 7;
        if (n > 0x40)
            n = 0x40;
        chunk = n - 7;
    }
    nfc_cmd_buf[0] = 10;
    nfc_cmd_buf[1] = 0x80;
    nfc_cmd_buf[2] = 9;
    nfc_cmd_buf[3] = n;
    nfc_cmd_buf[4] = 9;
    nfc_cmd_buf[5] = 0xd4;
    nfc_cmd_buf[6] = 9;
    nfc_cmd_buf[7] = 6;
    nfc_cmd_buf[8] = 9;
    nfc_cmd_buf[9] = seq & 3;
    if (b == 0) {
        off = b;
        j = b;
        nfc_snep_put_template[5] = (nfc_snep_put_template[10] + *len + 6) >> 24;
        nfc_snep_put_template[6] = (nfc_snep_put_template[10] + *len + 6) >> 16;
        nfc_snep_put_template[7] = (nfc_snep_put_template[10] + *len + 6) >> 8;
        nfc_snep_put_template[8] = *len + nfc_snep_put_template[10] + 6;
        nfc_snep_put_template[11] = *len >> 24;
        nfc_snep_put_template[12] = *len >> 16;
        nfc_snep_put_template[13] = *len >> 8;
        nfc_snep_put_template[14] = *len;
        for (; j <= 0x34; j++) {
            nfc_cmd_buf[off + 10] = 9;
            nfc_cmd_buf[off + 11] = nfc_snep_put_template[j];
            off += 2;
        }
        nfc_cmd_buf[0xf] = b;
        while (j + 4 < n) {
            nfc_cmd_buf[off + 10] = 9;
            nfc_cmd_buf[off + 11] = **pp;
            j++;
            (*pp)++;
            off += 2;
        }
    } else {
        off = 0;
        for (j = off; j <= 2; j++) {
            nfc_cmd_buf[off + 10] = 9;
            nfc_cmd_buf[off + 11] = nfc_snep_put_template[j];
            off += 2;
        }
        nfc_cmd_buf[0xf] = b;
        while (j + 4 < n) {
            nfc_cmd_buf[off + 10] = 9;
            nfc_cmd_buf[off + 11] = **pp;
            j++;
            (*pp)++;
            off += 2;
        }
    }
    nfc_cmd_buf[off + 10] = 0xd;
    nfc_cmd_buf[off + 11] = 0x80;
    {
        u32 l = off + 12;
        nfc_uart_send(nfc_cmd_buf, l);
        l >>= 1;
        nfc_uart_recv(nfc_cmd_buf, l, (u8 *)&r);
    }
    *len -= chunk;
}

extern u8 nfc_snep_response_template[];

void nfc_snep_send_response(int seq, u8 b, u8 c)
{
    s8 r;
    u32 off, j;

    nfc_cmd_buf[0] = 10;
    nfc_cmd_buf[1] = 0x80;
    nfc_cmd_buf[2] = 9;
    nfc_cmd_buf[3] = 0xd;
    nfc_cmd_buf[4] = 9;
    nfc_cmd_buf[5] = 0xd4;
    nfc_cmd_buf[6] = 9;
    nfc_cmd_buf[7] = 6;
    nfc_cmd_buf[8] = 9;
    nfc_cmd_buf[9] = seq & 3;
    off = 0;
    for (j = off; j <= 8; j++) {
        nfc_cmd_buf[off + 10] = 9;
        nfc_cmd_buf[off + 11] = nfc_snep_response_template[j];
        off += 2;
    }
    nfc_cmd_buf[0xf] = b;
    nfc_cmd_buf[0x13] = c;
    nfc_cmd_buf[off + 10] = 0xd;
    nfc_cmd_buf[off + 11] = 0x80;
    {
        u32 len = off + 12;
        nfc_uart_send(nfc_cmd_buf, len);
        len >>= 1;
        nfc_uart_recv(nfc_cmd_buf, len, (u8 *)&r);
    }
}

/*
 * LLCP: send DEP_REQ (PFB k & 3) with an RR (receive ready) PDU acknowledging I PDUs up
 * to N(R) = m & 0xF. s == 0: we are the SNEP client (13 60 = DSAP 4 <- SSAP 0x20); s != 0:
 * we are the SNEP server (83 44 = DSAP 0x20 <- SSAP 4).
 */
void nfc_llcp_send_rr(int k, u8 m, s8 s)
{
    u8 buf[4];

    nfc_cmd_buf[0] = 10;
    nfc_cmd_buf[1] = 0x80;
    nfc_cmd_buf[2] = 9;
    nfc_cmd_buf[3] = 7;
    nfc_cmd_buf[4] = 9;
    nfc_cmd_buf[5] = 0xd4;
    nfc_cmd_buf[6] = 9;
    nfc_cmd_buf[7] = 6;
    nfc_cmd_buf[8] = 9;
    nfc_cmd_buf[9] = k & 3;
    nfc_cmd_buf[10] = 9;
    nfc_cmd_buf[12] = 9;
    if (s == 0) {
        nfc_cmd_buf[11] = 0x13;
        nfc_cmd_buf[13] = 0x60;
    } else {
        nfc_cmd_buf[11] = 0x83;
        nfc_cmd_buf[13] = 0x44;
    }
    nfc_cmd_buf[14] = 9;
    nfc_cmd_buf[15] = m & 0xf;
    nfc_cmd_buf[16] = 13;
    nfc_cmd_buf[17] = 0x80;
    nfc_uart_send(nfc_cmd_buf, 18);
    nfc_uart_recv(nfc_cmd_buf, 9, buf);
}

/*
 * LLCP: send DISC from SAP 0x20 to SAP 4 (end of our SNEP client session).
 */
void nfc_llcp_send_disc(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 6;
    p[4] = 9;
    p[5] = 0xd4;
    p[6] = 9;
    p[7] = 6;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0x11;
    p[12] = 9;
    p[13] = 0x60;
    p[14] = 13;
    p[15] = 0x80;
    nfc_uart_send(p, 16);
    nfc_uart_recv(p, 8, buf);
}

/*
 * LLCP (target side): send DISC 81 44 from SAP 4 to SAP 0x20 (refuse/close).
 */
void nfc_llcp_send_disc_server(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 6;
    p[4] = 9;
    p[5] = 0xd5;
    p[6] = 9;
    p[7] = 7;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0x81;
    p[12] = 9;
    p[13] = 0x44;
    p[14] = 13;
    p[15] = 0x80;
    nfc_uart_send(p, 16);
    nfc_uart_recv(p, 8, buf);
}

/*
 * LLCP: send DM 81 C4 00 from SAP 4 to SAP 0x20 = answer to a DISC (disconnected).
 */
void nfc_llcp_send_dm(int k)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 9;
    p[3] = 7;
    p[4] = 9;
    p[5] = 0xd4;
    p[6] = 9;
    p[7] = 6;
    p[8] = 9;
    p[9] = k & 3;
    p[10] = 9;
    p[11] = 0x81;
    p[12] = 9;
    p[13] = 0xc4;
    p[14] = 9;
    p[15] = 0;
    p[16] = 13;
    p[17] = 0x80;
    nfc_uart_send(p, 18);
    nfc_uart_recv(p, 9, buf);
}

s8 nfc_snep_store_fragment(int dst, u8 *rec, int *plen, u32 max, int pos, int *pn, s8 cont, s8 flash)
{
    s8 r;
    u32 len;
    u32 n;
    s8 *data;
    u32 i;
    int odd;
    u16 *fp;
    u16 v;

    len = rec[0];
    r = 0;
    if (cont == 0) {
        u32 pl, tl, hl;

        pl = rec[12] | rec[11] << 8 | rec[10] << 16 | (s8)rec[9] << 24;
        if (rec[13] & 0x10)
            tl = rec[15];
        else
            tl = rec[18] | rec[17] << 8 | rec[16] << 16 | (s8)rec[15] << 24;
        hl = pl - tl + 13;
        n = len - hl;
        data = (s8 *)rec + hl;
        *plen = tl;
        nfc_snep_payload_len = tl;
        if (max != 0 && max < tl)
            return -7;
    } else {
        n = len - 7;
        data = (s8 *)rec + 7;
        *plen = 0;
    }
    if (flash != 0) {
        odd = pos & 1;
        if (odd == 1) {
            fp = (u16 *)(dst + pos - 1);
            for (i = 0; i <= n - 1; i += 2) {
                if (i == 0)
                    v = nfc_snep_odd_byte | (u16)data[0] << 8;
                else
                    v = (u8)data[i - 1] | (u16)data[i] << 8;
                asm volatile("psrclr 4");
                r = func_0000B52(fp, v, nfc_flash_base);
                fp++;
                asm volatile("psrset 4");
                if (r != 0)
                    return -6;
            }
            if ((n & 1) == 0)
                nfc_snep_odd_byte = data[n - 1];
        } else {
            fp = (u16 *)(dst + pos);
            for (i = 0; i < n - 1; i += 2) {
                v = (u8)data[i] | (u16)data[i + 1] << 8;
                asm volatile("psrclr 4");
                r = func_0000B52(fp, v, nfc_flash_base);
                fp++;
                asm volatile("psrset 4");
                if (r != 0)
                    return -6;
            }
            odd = n & 1;
            if (odd == 1)
                nfc_snep_odd_byte = data[n - 1];
        }
        if (pos + n >= nfc_snep_payload_len && (odd = (pos + n) & 1) == 1) {
            v = nfc_snep_odd_byte;
            asm volatile("psrclr 4");
            r = func_0000B52(fp, v, nfc_flash_base);
            asm volatile("psrset 4");
            if (r != 0)
                return -6;
        }
    } else {
        for (i = 0; i < n; i++)
            ((u8 *)dst + pos)[i] = data[i];
    }
    *pn = n;
    return r;
}

/* Type A activation of a PHONE (NFC-DEP target) for the LLCP/SNEP functions: REQA (0x26
 * this time, 7 bits) -> ATQA to *(u16 *)b, anticollision/select cascade 1 and 2 (0x93 /
 * 0x95), UID to a, 0xEEF when SAK & 0x20. Same protocol as nfc_activate_type_a without the
 * chip re-init and with a plain timeout c (in NFC ticks of 100 ms; the callers pass 50 =
 * 5 s). PN512 side: REQA with BitFraming 0x0D = 0x87 (StartSend, 7 bits),
 * anticollision with Tx/RxMode 0x12/0x13 = 0 (no CRC), SELECT with Tx/RxMode = 0x80 (CRC
 * on, 106 kbit/s), which stays set for the NFC-DEP exchange that follows. The SAK
 * (bit 5 -> 0xEEF) is not looked at by the LLCP callers. Result mapped by
 * nfc_error_code. */
/*
 * NFC-A poll as initiator (106 kbit/s): REQA (0x26, 7-bit frame) until an ATQA arrives
 * (stored in *b), then anticollision + select for cascade levels 1 and 2 (93 20 / 95 20,
 * 93 70 / 95 70 with the UID), within c ticks of 100 ms. Used to find a phone acting as an
 * NFC-DEP target before sending ATR_REQ. Returns 0, 3 (front-end error), 4 (abort),
 * -4 (timeout), -1.
 */
s8 nfc_poll_type_a(int a, int b, int c)
{
    s8 r;
    u8 h1[8];
    u8 h2[8];
    u32 t0;
    int cnt;
    int res;
    s8 kk;
    u32 i, j;
    s8 t;
    s8 x;
    u8 v;

    t = 0;
    cnt = 0;
    res = 0;
    nfc_abort = 0;
    t0 = nfc_ticks;
    while (cnt <= 999) {
        nfc_cmd_buf[0] = 10;
        nfc_cmd_buf[1] = 0x80;
        nfc_cmd_buf[2] = 9;
        nfc_cmd_buf[3] = 0x26;
        nfc_cmd_buf[4] = 1;
        nfc_cmd_buf[5] = 0xc;
        nfc_cmd_buf[6] = 0xd;
        nfc_cmd_buf[7] = 0x87;
        nfc_uart_send(nfc_cmd_buf, 8);
        nfc_uart_recv(nfc_cmd_buf, 4, (u8 *)&r);
        for (j = 0; j <= 999; j++) {
            nfc_cmd_buf[0] = 0x8a;
            nfc_cmd_buf[1] = 0x88;
            nfc_cmd_buf[2] = 0x86;
            nfc_uart_send(nfc_cmd_buf, 3);
            nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
            if (nfc_cmd_buf[2] & 0x40)
                return 3;
            if (nfc_cmd_buf[0] > 1 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                break;
            if (nfc_abort == 1)
                return 4;
            if (nfc_ticks - t0 > c)
                return -4;
            for (i = 0; i <= 99; i++)
                asm("nop");
        }
        *(u16 *)b = 0;
        if (j <= 999) {
            nfc_cmd_buf[0] = 0x89;
            nfc_cmd_buf[1] = 0x89;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&r);
            if (r != 0) {
                cnt++;
                continue;
            }
        } else {
            cnt++;
            res = -1;
            continue;
        }
        *(u16 *)b = nfc_cmd_buf[1] << 8 | nfc_cmd_buf[0];
        res = 0;
        break;
    }
    if (cnt <= 999)
        cnt = 0;
    for (kk = 0; kk <= 1; kk++) {
        while (cnt <= 999) {
            if (kk > 0) {
                nfc_cmd_buf[0] = 0x12;
                nfc_cmd_buf[1] = 0;
                nfc_cmd_buf[2] = 0x13;
                nfc_cmd_buf[3] = 0;
                nfc_uart_send(nfc_cmd_buf, 4);
                nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&r);
            }
            nfc_cmd_buf[0] = 10;
            nfc_cmd_buf[1] = 0x80;
            nfc_cmd_buf[2] = 9;
            if (kk == 0)
                nfc_cmd_buf[3] = 0x93;
            else
                nfc_cmd_buf[3] = 0x95;
            nfc_cmd_buf[4] = 9;
            nfc_cmd_buf[5] = 0x20;
            nfc_cmd_buf[6] = 0xd;
            nfc_cmd_buf[7] = 0x80;
            nfc_uart_send(nfc_cmd_buf, 8);
            nfc_uart_recv(nfc_cmd_buf, 4, (u8 *)&r);
            for (j = 0; j <= 999; j++) {
                nfc_cmd_buf[0] = 0x8a;
                nfc_cmd_buf[1] = 0x88;
                nfc_cmd_buf[2] = 0x86;
                nfc_uart_send(nfc_cmd_buf, 3);
                nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
                if (nfc_cmd_buf[2] & 0x40)
                    return 3;
                if (nfc_cmd_buf[0] > 4 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                if (nfc_abort == 1)
                    return 4;
                if (nfc_ticks - t0 > c)
                    return -4;
                for (i = 0; i <= 99; i++)
                    asm("nop");
            }
            if (j <= 999) {
                for (i = 0; i <= 4; i++)
                    nfc_cmd_buf[i] = 0x89;
                nfc_uart_send(nfc_cmd_buf, 5);
                nfc_uart_recv(nfc_cmd_buf, 5, (u8 *)&r);
                if (r != 0) {
                    cnt++;
                    continue;
                }
                {
                    i = 0;
                    for (;;) {
                        if (kk == 0)
                            h1[i] = nfc_cmd_buf[i];
                        else
                            h2[i] = nfc_cmd_buf[i];
                        i++;
                        res = 0;
                        if (i > 4)
                            break;
                    }
                    break;
                }
            } else {
                cnt++;
                res = -1;
            }
        }
        if (cnt > 999)
            break;
        cnt = 0;
        while (cnt <= 999) {
            nfc_cmd_buf[0] = 10;
            nfc_cmd_buf[1] = 0x80;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
            nfc_cmd_buf[0] = 9;
            if (kk == 0)
                nfc_cmd_buf[1] = 0x93;
            else
                nfc_cmd_buf[1] = 0x95;
            nfc_cmd_buf[2] = 9;
            nfc_cmd_buf[3] = 0x70;
            nfc_cmd_buf[4] = 0x12;
            nfc_cmd_buf[5] = 0x80;
            nfc_cmd_buf[6] = 0x13;
            nfc_cmd_buf[7] = 0x80;
            nfc_uart_send(nfc_cmd_buf, 8);
            nfc_uart_recv(nfc_cmd_buf, 4, (u8 *)&r);
            for (i = 0; i <= 4; i++) {
                nfc_cmd_buf[i * 2] = 9;
                if (kk == 0)
                    nfc_cmd_buf[i * 2 + 1] = h1[i];
                else
                    nfc_cmd_buf[i * 2 + 1] = h2[i];
            }
            nfc_uart_send(nfc_cmd_buf, 10);
            nfc_uart_recv(nfc_cmd_buf, 5, (u8 *)&r);
            nfc_cmd_buf[0] = 0xd;
            nfc_cmd_buf[1] = 0x80;
            nfc_uart_send(nfc_cmd_buf, 2);
            nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
            for (j = 0; j <= 999; j++) {
                nfc_cmd_buf[0] = 0x8a;
                nfc_cmd_buf[1] = 0x88;
                nfc_cmd_buf[2] = 0x86;
                nfc_uart_send(nfc_cmd_buf, 3);
                nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&r);
                if (nfc_cmd_buf[2] & 0x40)
                    return 3;
                if (nfc_cmd_buf[0] != 0 && (nfc_cmd_buf[1] & 7) != 3 && (nfc_cmd_buf[1] & 7) != 2 && r == 0)
                    break;
                if (nfc_abort == 1)
                    return 4;
                if (nfc_ticks - t0 > c)
                    return -4;
                for (i = 0; i <= 99; i++)
                    asm("nop");
            }
            t = 0;
            if (j <= 999) {
                nfc_cmd_buf[0] = 0x89;
                nfc_uart_send(nfc_cmd_buf, 1);
                nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&r);
                t = nfc_cmd_buf[0];
                if (r != 0) {
                    cnt++;
                    continue;
                }
            } else {
                cnt++;
                res = -1;
                continue;
            }
            {
                {
                    res = 0;
                    if (kk == 0) {
                        for (i = 0; i <= 3; i++)
                            ((u8 *)a)[i] = 0;
                        for (i = 0; i <= 3; i++)
                            ((u8 *)a)[i + 4] = h1[i];
                    } else {
                        ((u8 *)a)[0] = 0;
                        for (i = 0; i <= 2; i++)
                            ((u8 *)a)[i + 1] = h1[i + 1];
                        for (i = 0; i <= 3; i++)
                            ((u8 *)a)[i + 4] = h2[i];
                    }
                    break;
                }
            }
        }
        if (cnt > 999)
            break;
        if ((u8)t & 4)
            continue;
        if ((u8)t & 0x20)
            *(u16 *)b = 0xeef;
        break;
    }
    x = nfc_error_code(r, 0);
    if (res == -1)
        x = 2;
    return x;
}

/*
 * PN512 register set for the PASSIVE 106 kbit/s NFC-A (ISO 14443 Type A) initiator, used
 * by nfc_snep_server / nfc_snep_client before polling a phone (the phone is the passive NFC-DEP
 * target): FIFOLevel 0x0A = 0x80 (flush), ControlReg 0x0C = 0x10 (initiator), TxModeReg
 * 0x12 = RxModeReg 0x13 = 0 (106 kbit/s, ISO 14443A framing, no CRC yet; nfc_poll_type_a
 * turns CRC on for SELECT), TxAutoReg 0x15 = 0xCF (AutoRFOFF + Force100ASK + CAOn +
 * InitialRFOn + Tx2/Tx1RFAutoEn: initial RF collision avoidance, 100 % ASK), RxThreshold
 * 0x85, Demod 0x61, RFCfg 0x79, GsNOn 0xF4, CWGsP 0x3F, TxControlReg 0x14 = 0x83 (both
 * antenna drivers on = RF field on), ModeReg 0x11 = 0x3B (reset value: TxWaitRF, RxWaitRF,
 * CRCPreset 11 = FFFF for CalcCRC only; the RF CRC preset follows Tx/RxMode), ModGsP 0x11,
 * timer registers 0x2A-0x2D = 0 (timer off), then CommandReg 0x01 = 0x0C (Transceive).
 */
void nfc_setup_passive_initiator(void)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 10;
    p[1] = 0x80;
    p[2] = 0xc;
    p[3] = 0x10;
    p[4] = 0x12;
    p[5] = 0;
    p[6] = 0x13;
    p[7] = 0;
    p[8] = 0x15;
    p[9] = 0xcf;
    p[10] = 0x18;
    p[11] = 0x85;
    p[12] = 0x19;
    p[13] = 0x61;
    p[14] = 0x26;
    p[15] = 0x79;
    p[16] = 0x27;
    p[17] = 0xf4;
    p[18] = 0x28;
    p[19] = 0x3f;
    p[20] = 0x14;
    p[21] = 0x83;
    p[22] = 0x11;
    p[23] = 0x3b;
    p[24] = 0x29;
    p[25] = 0x11;
    p[26] = 0x2a;
    p[27] = 0;
    p[28] = 0x2b;
    p[29] = 0;
    p[30] = 0x2c;
    p[31] = 0;
    nfc_cmd_buf[32] = 0x2d;
    nfc_cmd_buf[33] = 0;
    nfc_cmd_buf[34] = 1;
    nfc_cmd_buf[35] = 0xc;
    nfc_uart_send(p, 0x24);
    nfc_uart_recv(p, 0x12, buf);
}

/*
 * PN512 setup as NFCIP-1 active-mode NFC-DEP initiator at 424 kbit/s, used by
 * nfc_detect_target (tama-to-tama peer detection). Also called from the mode != 0 branches of
 * nfc_snep_server / nfc_snep_client, which are never taken (their mode is a constant 0).
 * Register pairs (PN512 datasheet 8.2): ControlReg 0x0C = 0x10 (Initiator), TxModeReg
 * 0x12 = RxModeReg 0x13 = 0xA1 (CRC on, speed 010 = 424 kbit/s, framing 01 = active
 * mode), TxAutoReg 0x15 = 0x8F (AutoRFOFF + CAOn + InitialRFOn + Tx2/Tx1RFAutoEn),
 * RxThresholdReg 0x18 = 0x85, DemodReg 0x19 = 0x61 (TPrescalEven 0), RFCfgReg 0x26 =
 * 0x79 (RxGain 48 dB), GsNOnReg 0x27 = 0xFF, CWGsPReg 0x28 = 0x3F, ModGsPReg 0x29 =
 * 0x0F, TxControlReg 0x14 = 0x80, TModeReg 0x2A = 0x82 (TAuto, prescaler high 2) /
 * TPrescalerReg 0x2B = 0xA6 (prescaler 0x2A6: 13.56 MHz / 1357 = 100.07 us per count) /
 * TReloadReg 0x2C-0x2D = 0x0020 (33 counts = 3.3 ms; TimerIRq never used), ModeReg 0x11
 * = 0x3B (reset value), then FIFOLevel 0x0A = 0x80 (flush) and CommandReg 0x01 = 0x0C
 * (Transceive).
 */
void nfc_setup_active_initiator(void)
{
    u8 buf[4];
    u8 *p = nfc_cmd_buf;

    p[0] = 0xc;
    p[1] = 0x10;
    p[2] = 0x12;
    p[3] = 0xa1;
    p[4] = 0x13;
    p[5] = 0xa1;
    p[6] = 0x15;
    p[7] = 0x8f;
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
    p[19] = 0xf;
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
    nfc_cmd_buf[35] = 0xc;
    nfc_uart_send(p, 0x24);
    nfc_uart_recv(p, 0x12, buf);
}

/*
 * The RAM variables this file defines.
 */
u8 nfc_snep_odd_byte;
u32 nfc_snep_payload_len;
u8 nfc_cmd_buf[0x140];
u8 card_auth_answer[4];
u16 card_status_word;
s8 nfc_snep_put_template[56] = {
    0x13, 0x20, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x01, 0xC2, 0x26, 0x00, 0x00, 0x00, 0x01,
    'a', 'p', 'p', 'l', 'i', 'c', 'a', 't', 'i', 'o', 'n', '/',
    'j', 'p', '.', 'c', 'o', '.', 'b', 'a', 'n', 'd', 'a', 'i', '.',
    't', 'a', 'm', 'a', 'g', 'o', 't', 'c', 'h', 'i', 'a', 'p', 'p',
    0x00, 0x00, 0x00,
};
u8 nfc_snep_response_template[12] = {
    0x83, 0x04, 0x01, 0x10, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
