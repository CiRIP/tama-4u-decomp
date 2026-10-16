/*
 * SNEP server and client over LLCP for the phone app link.
 *
 * Code 252F292..252FE2C, .rodata from 256261C.
 */
#include "proto/lib/nfc/nfc_snep.h"
#include "proto/nfc/nfc_link.h"
#include "proto/lib/nfc_hw/nfc_hw.h"
#include "proto/lib/nfc/nfc_dep.h"
#include "proto/lib/nfc/nfc_protocol.h"
#include "proto/lib/nfc/nfc_snep.h"

/* Receive a file from a phone: NFC-DEP initiator + LLCP + SNEP server ("Android Beam" in
 * reverse: the phone pushes an NDEF message to us).
 *   a = destination (flash address, or RAM when e = 0), *b = bytes received, c = maximum
 *   size (0 = no limit), d = timeout in seconds (d * 10 ticks since nfc_link_open) while
 *   waiting for the phone (nfc_receive passes 10).
 * Phase 1 (flag == 0): PN512 as passive 106 kbps Type A initiator (nfc_setup_passive_initiator), find and
 *   select the phone as a Type A target (nfc_poll_type_a, 5 s per round; leaves Tx/RxMode =
 *   0x80 = CRC on), ModeReg 0x11 = 0x7B (= reset 0x3B + DetectSync: the PN512 adds and
 *   strips the F0 start byte of NFC-DEP at 106 kbit/s), CommandReg 0x0C (Transceive),
 *   FIFO flush, then send ATR_REQ (nfc_send_atr_req(0): general bytes = LLCP magic 46 66 6D,
 *   VERSION 1.1, WKS, LTO) and wait for ATR_RES (D5 01: the phone's NFCID3 goes to
 *   nfc_peer_nfcid3). So the phone link is NFC-DEP over passive 106 kbit/s Type A, toy =
 *   initiator, phone = target. The other set-up (mode != 0: active 424 kbit/s initiator,
 *   nfc_setup_active_initiator + ATR_REQ(1)) is dead code: mode is a constant 0 (the asm compares 0
 *   with 0). For the same reason the target-side cases of the dispatch below (SENSF_REQ
 *   -> nfc_send_sensf_res, ATR_REQ -> nfc_send_atr_res, DEP_REQ, SENSF_RES 01 FE) cannot occur: a
 *   passive Type A initiator never receives them.
 * Phase 2: every received frame is read from the FIFO (chained frames: DEP_RES with
 *   PFB bit 4 = more information -> ack with DEP_REQ 0x40|PNI and append); code = bytes 1-2:
 *   0x00FF SENSF_REQ -> answer SENSF_RES (nfc_send_sensf_res); 0xD400 ATR_REQ (we are also asked)
 *   -> ATR_RES (nfc_send_atr_res); 0x0101 = Type F target found; 0xD501 ATR_RES; 0xD406 DEP_REQ
 *   and 0xD507 DEP_RES carry LLCP PDUs: byte 4-5 = DSAP(6) PTYPE(4) SSAP(6):
 *     PTYPE 0 SYMM: keep alive (nfc_llcp_send_symm_req = DEP_REQ+SYMM) or, when a SNEP PUT is being
 *       received, send the SNEP response in an I PDU (nfc_snep_send_response: 0x80 Continue after
 *       the first fragment, 0x81 Success when everything arrived) ;
 *     PTYPE 4 CONNECT to DSAP 4 (SNEP server) from SSAP 0x20 -> CC (nfc_llcp_send_cc);
 *       CONNECT to SAP 1 (SDP) -> DM (nfc_llcp_send_disc_server); 5 DISC -> DM (nfc_llcp_send_dm) and done;
 *     PTYPE 12 I PDU: the SNEP request (version, 0x02 PUT, 32-bit BE length at bytes 9-12,
 *       the NDEF message after it): nfc_snep_store_fragment strips the NDEF record header and stores
 *       the record payload (the TAMAGO item file) at a (programming the flash halfword by
 *       halfword, interrupts disabled) or in RAM; every fragment is acknowledged with RR
 *       (nfc_llcp_send_rr) and its SNEP fragment with Continue; "state" 1 = request seen,
 *       2 = large request, more fragments to come, 3 = Continue sent, 4 = complete.
 *   Any other frame: release (Command 0x01 = 0 = Idle) and return -2. 4 = user abort, -4
 *   timeout (d seconds for the initial detection, 5 s = 50 ticks between frames), 3 =
 *   PN512 over-temperature. Frames are accepted when FIFOLevel is 4..0x40, the UART
 *   status is clean, ModemState is not TxWait/Sending and Error & 0x37 (RFErr,
 *   BufferOvfl, CRC, Parity, Protocol) is 0.
 * The phone side is the stock Android NFC stack; this is why the app names the files
 * "DL_T4Ud....jpg": Beam sends them as an NDEF MIME/image record. */
/*
 * SNEP server (the toy receives data), over LLCP over NFC-DEP. Opens the front-end; as
 * initiator it configures passive NFC-A (nfc_setup_passive_initiator), polls (nfc_poll_type_a), sends
 * ATR_REQ (nfc_send_atr_req(0): LLCP 1.1); then loops reading frames from the PN512 FIFO
 * (len, D4/D5, 06/07, PFB, LLCP header...), with chained frames (PFB MI bit 0x10) acked
 * by an ACK PDU and concatenated in nfc_cmd_buf. Dispatch on the 2-byte command:
 *   0x00FF + FF 00: NFC-F poll -> SENSF_RES (nfc_send_sensf_res);
 *   D4 00: ATR_REQ from a phone acting as initiator -> ATR_RES (nfc_send_atr_res(1));
 *   0x0101 + FE: SENSF_RES from the phone -> keep its NFCID2, send ATR_REQ;
 *   D4 06 (DEP_REQ, phone is initiator) / D5 07 (DEP_RES, we are initiator): LLCP PDU
 *   type = ((b4 << 2) & 0xC) | (b5 >> 6), DSAP = b4 >> 2, SSAP = b5 & 0x3F:
 *     0 SYMM: answer SYMM, or, when data is pending, the SNEP response (nfc_snep_send_response:
 *       0x80 Continue after the first fragment of a long PUT, 0x81 Success at the end);
 *     4 CONNECT to SAP 4 from 0x20 (or to 1 from 0x21): answer CC (nfc_llcp_send_cc) and start
 *       the SNEP session (state 1); CONNECT to SDP (1) from 0x20: DM (nfc_llcp_send_dm_sdp);
 *     5 DISC: answer DM (nfc_llcp_send_dm) and return 0;
 *     12 I-PDU to SAP 4: first fragment = SNEP request (version, 0x02 PUT, 32-bit
 *       big-endian length at bytes 9..12): length > 0x7A -> state 2 (send Continue),
 *       else state 4; the payload goes to nfc_snep_store_fragment (NDEF record parsing; data to RAM
 *       or straight into flash at a), *b = bytes received so far; answer RR.
 *   D6 00 / anything else after the link is up: release (RLS?) and return -2.
 * Timeouts: d seconds before the link is up, 5 s (50 ticks) between frames once up.
 * Returns 0 = complete, 4 = aborted (nfc_abort), 3 = front-end error, -2, -4 = timeout,
 * -15 = no front-end, or the error of nfc_snep_store_fragment.
 */
int nfc_snep_server(int a, int *b, int c, int d, s8 e)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    s8 st;
    u8 hdr;
    u8 buf[320];
    u8 buf2[12];
    u16 h;
    u32 v174;
    int n;
    int v17c;
    u32 t0;
    int code;
    int seq;
    u8 len;
    u8 x1a4;
    u8 x1a8;
    int state;
    u32 total;
    int x1b4;
    int last;
    int flag;
    int mode;
    u32 lim = 50;
    s8 r;
    u32 start;
    u8 pos;
    u32 i, j;
    int cmd, sub;
    int t;

    total = 0;
    mode = 0;
    flag = 0;
    h = 0;
    nfc_abort = 0;
    *b = 0;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    r = nfc_link_open((int)&cfg);
    if (r != 0)
        return -15;
    len = 0;
    code = 0xffff;
    seq = 0;
    x1a8 = 0xff;
    x1a4 = 0xff;
    state = 0;
    for (;;) {
        t0 = nfc_ticks;
        if (mode == 0) {
            if (flag == 0) {
                nfc_setup_passive_initiator();
                for (j = 0; j <= 1999; j++)
                    asm("nop");
                start = nfc_ticks;
                do {
                    r = nfc_poll_type_a((int)buf2, (int)&h, 50);
                    if (r == 0) {
                        flag = 1;
                        break;
                    }
                    if (nfc_abort != 0) {
                        nfc_link_close();
                        return 4;
                    }
                    if (nfc_ticks > (u32)(d * 10)) {
                        nfc_link_close();
                        return -4;
                    }
                } while (nfc_ticks - start < 50);
                if (flag == 0) {
                    for (i = 0; i <= 499; i++)
                        asm("nop");
                    continue;
                }
                nfc_cmd_buf[0] = 0x11;
                nfc_cmd_buf[1] = 0x7b;
                nfc_cmd_buf[2] = 1;
                nfc_cmd_buf[3] = 0xc;
                nfc_cmd_buf[4] = 10;
                nfc_cmd_buf[5] = 0x80;
                n = nfc_uart_send(nfc_cmd_buf, 6);
                n = nfc_uart_recv(nfc_cmd_buf, 3, (u8 *)&st);
                nfc_send_atr_req(0);
                for (j = 0; j <= 1999; j++)
                    asm("nop");
            }
        } else {
            if (flag == 0) {
                nfc_setup_active_initiator();
                for (j = 0; j <= 1999; j++)
                    asm("nop");
                nfc_cmd_buf[0] = 1;
                nfc_cmd_buf[1] = 0;
                n = nfc_uart_send(nfc_cmd_buf, 2);
                n = nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&st);
                for (i = 0; i <= 4999; i++)
                    asm("nop");
                nfc_cmd_buf[0] = 10;
                nfc_cmd_buf[1] = 0x80;
                nfc_cmd_buf[2] = 1;
                nfc_cmd_buf[3] = 0xc;
                n = nfc_uart_send(nfc_cmd_buf, 4);
                n = nfc_uart_recv(nfc_cmd_buf, 2, (u8 *)&st);
                for (j = 0; j <= 1999; j++)
                    asm("nop");
                nfc_send_atr_req(1);
            }
        }
        last = 0;
        x1b4 = 0;
        pos = 0;
        do {
            last = 1;
            start = nfc_ticks;
            while (nfc_ticks - start < lim) {
                buf[0] = 0x8a;
                buf[1] = 0x88;
                buf[2] = 0x86;
                n = nfc_uart_send(buf, 3);
                n = nfc_uart_recv(buf, 3, (u8 *)&st);
                if (buf[2] & 0x40) {
                    nfc_link_close();
                    return 3;
                }
                if (buf[0] != 0)
                    code = 0xffff;
                if (buf[0] >= 4 && buf[0] <= 0x40 && st == 0 && (buf[1] & 7) != 3
                    && (buf[1] & 7) != 2 && (buf[2] & 0x37) == 0) {
                    pos = buf[0];
                    code = 0;
                    break;
                }
                for (i = 0; i <= 99; i++)
                    asm("nop");
                if (nfc_abort != 0) {
                    nfc_link_close();
                    return 4;
                }
                if (flag == 0) {
                    if (nfc_ticks > (u32)(d * 10)) {
                        nfc_link_close();
                        return -4;
                    }
                } else {
                    if (nfc_ticks - start > 50) {
                        nfc_link_close();
                        return -4;
                    }
                }
            }
            start = nfc_ticks;
            if (code == 0) {
                for (i = 0; i < pos; i++)
                    buf[i] = 0x89;
                n = nfc_uart_send(buf, pos);
                n = nfc_uart_recv(buf, pos, (u8 *)&st);
                while (buf[0] > pos) {
                    hdr = 0x8a;
                    n = nfc_uart_send(&hdr, 1);
                    n = nfc_uart_recv(&hdr, 1, (u8 *)&st);
                    for (i = pos; i < hdr + pos; i++)
                        buf[i] = 0x89;
                    n = nfc_uart_send(buf + pos, hdr);
                    n = nfc_uart_recv(buf + pos, hdr, (u8 *)&st);
                    pos += n;
                    if (nfc_abort != 0) {
                        nfc_link_close();
                        return 4;
                    }
                    if (flag == 0) {
                        if (nfc_ticks > (u32)(d * 10)) {
                            nfc_link_close();
                            return -4;
                        }
                    } else {
                        if (nfc_ticks - start > 50) {
                            nfc_link_close();
                            return -4;
                        }
                    }
                }
                if (x1b4 == 0) {
                    for (i = 0; i < pos; i++)
                        nfc_cmd_buf[i] = buf[i];
                    len = pos;
                    x1b4 = 1;
                } else {
                    for (i = 4; i < pos; i++)
                        nfc_cmd_buf[len + i - 4] = buf[i];
                    len += pos - 4;
                    nfc_cmd_buf[0] = len;
                }
                t = buf[1] << 8;
                if ((buf[2] | t) == 0xd507 && (buf[3] & 0x10))
                    last = 0;
                if (last == 0) {
                    buf[0] = 10;
                    buf[1] = 0x80;
                    buf[2] = 9;
                    buf[3] = 4;
                    buf[4] = 9;
                    buf[5] = 0xd4;
                    buf[6] = 9;
                    buf[7] = 6;
                    buf[8] = 9;
                    buf[9] = (seq & 3) | 0x40;
                    buf[10] = 0xd;
                    buf[11] = 0x80;
                    n = nfc_uart_send(buf, 12);
                    n = nfc_uart_recv(buf, 6, (u8 *)&st);
                    seq = (u8)(seq + 1) & 3;
                }
            } else {
                len = 0;
                code = 0xffff;
                seq = 0;
                x1a8 = 0xff;
                x1a4 = 0xff;
                state = 0;
                x1b4 = 0;
                pos = 0;
                if (flag == 0) {
                    if (nfc_ticks > (u32)(d * 10)) {
                    fail4:
                        nfc_link_close();
                        return -4;
                    }
                } else {
                    if (nfc_ticks - start > 50)
                        goto fail4;
                }
            }
        } while (last == 0);
        if (code == 0) {
            t = nfc_cmd_buf[1] << 8;
            code = nfc_cmd_buf[2] | t;
            flag = 1;
        }
        switch (code) {
        case 0xff:
            if (len > 5 && nfc_cmd_buf[3] == 0xff && nfc_cmd_buf[4] == 0)
                nfc_send_sensf_res();
            break;
        case 0xd400:
            if (len > 0x10) {
                for (i = 0; i <= 9; i++)
                    ((u8 *)&nfc_peer_nfcid3)[i] = nfc_cmd_buf[i + 3];
                nfc_send_atr_res(1);
            }
            break;
        case 0xd406:
            cmd = nfc_cmd_buf[4] >> 2;
            sub = nfc_cmd_buf[5] & 0x3f;
            switch (((nfc_cmd_buf[4] << 2) & 0xc) | (nfc_cmd_buf[5] >> 6)) {
            case 0:
                if (len == 6 && cmd == 0 && sub == 0)
                    nfc_llcp_send_symm_res(seq);
                break;
            case 4:
                if (len > 5 && cmd == 1 && sub == 0x21)
                    nfc_llcp_send_disc_server(seq);
                break;
            case 1:
            case 2:
            case 13:
            default:
                nfc_llcp_send_symm_res(seq);
                break;
            }
            break;
        case 0x101:
            if (len > 0x11 && nfc_cmd_buf[3] == 0xfe) {
                ((u8 *)&nfc_peer_nfcid3)[8] = 0;
                ((u8 *)&nfc_peer_nfcid3)[9] = 0;
                for (i = 0; i <= 7; i++)
                    ((u8 *)&nfc_peer_nfcid3)[i] = nfc_cmd_buf[i + 2];
                nfc_send_atr_req(1);
            }
            break;
        case 0xd501:
            for (i = 0; i <= 9; i++)
                ((u8 *)&nfc_peer_nfcid3)[i] = nfc_cmd_buf[i + 3];
            goto def507;
        case 0xd507:
            cmd = nfc_cmd_buf[4] >> 2;
            sub = nfc_cmd_buf[5] & 0x3f;
            switch (((nfc_cmd_buf[4] << 2) & 0xc) | (nfc_cmd_buf[5] >> 6)) {
            case 0:
                if (len == 6 && cmd == 0 && sub == 0) {
                    if (x1a4 != 0xff || x1a8 != 0xff) {
                        if (state == 4) {
                            nfc_snep_send_response(seq, x1a4 << 4 | (x1a8 & 0xf), 0x81);
                            for (i = 0; i <= 1999; i++)
                                asm("nop");
                            nfc_link_close();
                            return 0;
                        } else if (state == 2) {
                            nfc_snep_send_response(seq, x1a4 << 4 | (x1a8 & 0xf), 0x80);
                            x1a4 = (x1a4 + 1) & 0xf;
                            state = 3;
                        } else
                            nfc_llcp_send_symm_req(seq);
                    } else
                        nfc_llcp_send_symm_req(seq);
                }
                break;
            case 4:
                if (len > 5 && cmd == 4 && sub == 0x20) {
                    nfc_llcp_send_cc(seq);
                    x1a8 = 0;
                    x1a4 = 0;
                    state = 1;
                } else if (len > 5 && cmd == 1 && sub == 0x21) {
                    nfc_llcp_send_cc(seq);
                    x1a8 = 0;
                    x1a4 = 0;
                    state = 1;
                } else if (len > 5 && cmd == 1 && sub == 0x20)
                    nfc_llcp_send_dm_sdp(seq);
                else
                    nfc_llcp_send_symm_req(seq);
                break;
            case 5:
                if (len > 5 && cmd == 4 && sub == 0x20) {
                    nfc_llcp_send_dm(seq);
                    for (i = 0; i <= 1999; i++)
                        asm("nop");
                    nfc_link_close();
                    return 0;
                }
                nfc_llcp_send_symm_req(seq);
                break;
            case 12:
                if (len > 6 && cmd == 4 && sub == 0x20) {
                    if (state == 1) {
                        u32 v = nfc_cmd_buf[12] | nfc_cmd_buf[11] << 8 | nfc_cmd_buf[10] << 16 | nfc_cmd_buf[9] << 24;
                        state = v > 0x7a ? 2 : 4;
                        r = nfc_snep_store_fragment(a, nfc_cmd_buf, &v174, c, 0, &n, 0, e);
                        if (r != 0) {
                            nfc_link_close();
                            return r;
                        }
                        total = v174;
                        *b = n;
                        v174 = n;
                    } else {
                        r = nfc_snep_store_fragment(a, nfc_cmd_buf, &v17c, c, v174, &n, 1, e);
                        if (r != 0) {
                            nfc_link_close();
                            return r;
                        }
                        v174 += n;
                        *b = v174;
                        if (v174 >= total)
                            state = 4;
                    }
                    nfc_llcp_send_rr(seq, x1a8 = (x1a8 + 1) & 0xf, 1);
                } else
                    nfc_llcp_send_symm_req(seq);
                break;
            case 1:
            case 2:
            case 13:
            default:
            def507:
                nfc_llcp_send_symm_req(seq);
                break;
            }
            seq = (u8)(seq + 1) & 3;
            break;
        case 0xd600:
        default:
            if (flag == 0) {
                for (i = 0; i <= 499; i++)
                    asm("nop");
            } else {
                nfc_cmd_buf[0] = 1;
                nfc_cmd_buf[1] = 0;
                n = nfc_uart_send(nfc_cmd_buf, 2);
                n = nfc_uart_recv(nfc_cmd_buf, 1, (u8 *)&st);
                for (i = 0; i <= 4999; i++)
                    asm("nop");
                nfc_link_close();
                return -2;
            }
            break;
        }
        if (nfc_abort != 0) {
            nfc_link_close();
            return 4;
        }
        if (flag == 0) {
            if (nfc_ticks > (u32)(d * 10)) {
                nfc_link_close();
                return -4;
            }
        } else {
            if (nfc_ticks - t0 > 50) {
                nfc_link_close();
                return -4;
            }
        }
        for (i = 0; i <= 999; i++)
            asm("nop");
    }
}

/* Send b bytes from a to a phone: the mirror image of nfc_snep_server. Same link setup
 * (Type A reader, select the phone, ATR_REQ with LLCP magic), then LLCP as the client:
 * on SYMM send CONNECT to the phone's SNEP server (DSAP 4 from SSAP 0x20, nfc_llcp_send_connect),
 * on CC (PTYPE 6) start pushing the data in I PDUs (nfc_snep_send_put: SNEP PUT request header
 * + NDEF, x24/x28 = remaining pointer/length, x48 = N(S), x4C = N(R)), answer RR/I PDUs
 * from the phone (PTYPE 12/13) with RR (nfc_llcp_send_rr) until the phone's SNEP response
 * arrives, then DISC (nfc_llcp_send_disc) / DM (PTYPE 7 -> return 0). c = timeout in seconds
 * for the first contact (c * 10 ticks), then 5 s per frame. Same PN512 set-up as
 * nfc_snep_server (passive 106 kbit/s Type A initiator, ModeReg 0x7B); the active branch
 * (mode != 0) and the target-side cases are dead here too. */
#define B45 (nfc_cmd_buf)
/*
 * SNEP client (the toy sends data a, length b, timeout c): same link set-up as
 * nfc_snep_server, then: on the first SYMM send CONNECT to SAP 4 (nfc_llcp_send_connect); on CC (from
 * SAP 4 to 0x20) send the first I-PDU of the SNEP PUT (nfc_snep_send_put sends the next
 * fragment of the buffer x24/x28); on RR send the next fragment; on an I-PDU from the
 * server (SNEP Continue / Success) answer RR or, when everything is sent, DISC
 * (nfc_llcp_send_disc); on DM return 0 (done). Same error codes as nfc_snep_server.
 */
int nfc_snep_client(void *a, int b, int c)
{
    struct {
        u8 *b1;
        u32 n1;
        u8 *b2;
        u32 n2;
    } cfg;
    s8 r;
    u8 buf[12];
    u16 st;
    int x24;
    int x28;
    u32 t0;
    int code;
    u8 state;
    u32 len;
    u8 x48;
    u8 x4c;
    int x50;
    int flag;
    u16 mode;
    u32 n = 50;
    u32 el;
    u32 j;
    u8 *pa, *pb, *pg, *pc, *qc, *pd, *pe, *qe, *pf;
    u8 *q;
    u32 i;
    u32 start;
    int hi, lo;

    mode = flag = 0;
    st = 0;
    nfc_abort = mode;
    cfg.b1 = nfc_tx_fifo_buf;
    cfg.n1 = 0x100;
    cfg.b2 = nfc_rx_fifo_buf;
    cfg.n2 = 0x100;
    if (nfc_link_open((int)&cfg) != 0)
        return -15;
    len = 0;
    code = 0xffff;
    state = 0;
    x48 = 0xff;
    x4c = 0xff;
    x24 = (int)a;
    x28 = b;
    x50 = state;
    for (;;) {
        t0 = nfc_ticks;
        if (mode == 0) {
            if (flag == 0) {
                nfc_setup_passive_initiator();
                for (j = 0; j <= 1999; j++)
                    asm("nop");
                start = nfc_ticks;
                do {
                    if (nfc_poll_type_a((int)buf, (int)&st, 50) == 0) {
                        flag = 1;
                        break;
                    }
                    if (nfc_abort != 0) {
                        nfc_link_close();
                        return 4;
                    }
                    if (nfc_ticks > c * 10) {
                        nfc_link_close();
                        return -4;
                    }
                } while (nfc_ticks - start < 50);
                if (flag == 0) {
                    for (i = 0; i <= 499; i++)
                        asm("nop");
                    continue;
                }
                pa = nfc_cmd_buf;
                pa[0] = 0x11;
                pa[1] = 0x7b;
                pa[2] = 1;
                pa[3] = 0xc;
                pa[4] = 10;
                pa[5] = 0x80;
                nfc_uart_send(pa, 6);
                nfc_uart_recv(pa, 3, (u8 *)&r);
                nfc_send_atr_req(0);
                for (j = 0; j <= 1999; j++)
                    asm("nop");
            }
        } else {
            if (flag == 0) {
                nfc_setup_active_initiator();
                for (j = 0; j <= 1999; j++)
                    asm("nop");
                pb = nfc_cmd_buf;
                pb[0] = 1;
                pb[1] = 0;
                nfc_uart_send(pb, 2);
                nfc_uart_recv(pb, 1, (u8 *)&r);
                for (i = 0; i <= 4999; i++)
                    asm("nop");
                pg = nfc_cmd_buf;
                pg[0] = 10;
                pg[1] = 0x80;
                pg[2] = 1;
                pg[3] = 0xc;
                nfc_uart_send(pg, 4);
                nfc_uart_recv(pg, 2, (u8 *)&r);
                for (j = 0; j <= 1999; j++)
                    asm("nop");
                nfc_send_atr_req(0);
            } else {
                el = 0; /* dead store: the barrier after this if/else is where loop.c moves the found block */
            }
        }
        start = nfc_ticks;
        el = 0;
        while (el < n) {
            pc = nfc_cmd_buf;
            pc[0] = 0x8a;
            pc[1] = 0x88;
            pc[2] = 0x86;
            nfc_uart_send(pc, 3);
            nfc_uart_recv(pc, 3, (u8 *)&r);
            if (pc[2] & 0x40) {
                nfc_link_close();
                return 3;
            }
            qc = nfc_cmd_buf;
            if (qc[0] >= 4 && qc[0] <= 64 && r == 0 && (qc[1] & 7) != 3 && (qc[1] & 7) != 2
                && (qc[2] & 0x37) == 0) {
                len = qc[0];
                code = 0;
                break;
            }
            if (nfc_abort != 0) {
                nfc_link_close();
                return 4;
            }
            if (flag == 0) {
                if (nfc_ticks > c * 10) {
                    nfc_link_close();
                    return -4;
                }
            } else if (nfc_ticks - start > 50) {
                nfc_link_close();
                return -4;
            }
            el = nfc_ticks - start;
        }
        if (code == 0) {
            for (i = 0; i < len; i++)
                B45[i] = 0x89;
            pd = nfc_cmd_buf;
            nfc_uart_send(pd, len);
            nfc_uart_recv(pd, len, (u8 *)&r);
            {
                int h = pd[1] << 8;
                code = pd[2] | h;
            }
            if (pd[0] > len) {
                for (i = len; i < B45[0]; i++)
                    B45[i] = 0x89;
                pe = nfc_cmd_buf;
                qe = pe + len;
                nfc_uart_send(qe, pe[0] - len);
                nfc_uart_recv(qe, pe[0] - len, (u8 *)&r);
                len = pe[0];
            }
            flag = 1;
        } else {
            len = 0;
            code = 0xffff;
            state = 0;
            x48 = 0xff;
            x4c = 0xff;
            x24 = (int)a;
            x28 = b;
            x50 = state;
        }
        for (i = 0; i <= 1999; i++)
            asm("nop");
        switch (code) {
        case 0xff:
            if (len > 5 && nfc_cmd_buf[3] == 0xff && nfc_cmd_buf[4] == 0)
                nfc_send_sensf_res();
            break;
        case 0xd400:
            if (len > 0x10) {
                for (i = 0; i <= 9; i++)
                    ((u8 *)&nfc_peer_nfcid3)[i] = nfc_cmd_buf[i + 3];
                nfc_send_atr_res(0);
            }
            break;
        case 0xd406:
            hi = nfc_cmd_buf[4] >> 2;
            lo = nfc_cmd_buf[5] & 0x3f;
            switch (((nfc_cmd_buf[4] << 2) & 0xc) | (nfc_cmd_buf[5] >> 6)) {
            case 0:
                if (len == 6 && hi == 0 && lo == 0)
                    nfc_llcp_send_symm_res(state);
                break;
            case 4:
                if (len > 5 && hi == 1 && lo == 0x21)
                    nfc_llcp_send_disc_server(state);
                break;
            case 6:
                nfc_llcp_send_symm_res(state);
                break;
            case 7:
                nfc_llcp_send_symm_res(state);
                break;
            case 12:
                nfc_llcp_send_symm_res(state);
                break;
            case 13:
                nfc_llcp_send_symm_res(state);
                break;
            default:
                nfc_llcp_send_symm_res(state);
                break;
            }
            break;
        case 0x101:
            if (len > 0x11 && nfc_cmd_buf[3] == 0xfe) {
                nfc_peer_nfcid3.id8 = 0;
                nfc_peer_nfcid3.id9 = 0;
                for (i = 0; i <= 7; i++)
                    ((u8 *)&nfc_peer_nfcid3)[i] = nfc_cmd_buf[i + 2];
                nfc_send_atr_req(0);
            }
            break;
        case 0xd501:
            for (i = 0; i <= 9; i++)
                ((u8 *)&nfc_peer_nfcid3)[i] = nfc_cmd_buf[i + 3];
            nfc_llcp_send_symm_req(state);
            state = (state + 1) & 3;
            break;
        case 0xd507:
            hi = nfc_cmd_buf[4] >> 2;
            lo = nfc_cmd_buf[5] & 0x3f;
            switch (((nfc_cmd_buf[4] << 2) & 0xc) | (nfc_cmd_buf[5] >> 6)) {
            case 0:
                if (len == 6 && hi == 0 && lo == 0) {
                    if (x48 == 0xff && x4c == 0xff) {
                        nfc_llcp_send_connect(state);
                        x48 = 0;
                        x4c = 0;
                    } else {
                        if (x28 == 0)
                            goto dflt;
                        if (x50 != 0)
                            goto send;
                        goto dflt;
                    }
                }
                break;
            case 4:
                if (len > 5 && hi == 4 && lo == 0x20) {
                    nfc_llcp_send_cc(state);
                    break;
                }
                goto dflt;
            case 6:
                if (len > 5 && hi == 0x20 && lo == 4)
                    goto send;
                goto dflt;
            case 7:
                nfc_llcp_send_symm_req(state);
                for (i = 0; i <= 1999; i++)
                    asm("nop");
                nfc_link_close();
                return 0;
            case 12:
                if (len > 6 && hi == 0x20 && lo == 4) {
                    if (x28 == 0 && x50 != 0) {
                        nfc_llcp_send_disc(state);
                    } else {
                        x4c = (x4c + 1) & 0xf;
                        nfc_llcp_send_rr(state, x4c, 0);
                        x50 = 1;
                    }
                    break;
                }
                goto dflt;
            case 13:
                if (len > 6 && hi == 0x20 && lo == 4 && x28 != 0 && x50 != 0) {
                send:
                    nfc_snep_send_put(state, x48 << 4 | (x4c & 0xf), (int)&x24, (int)&x28);
                    x48 = (x48 + 1) & 0xf;
                    break;
                }
            default:
            dflt:
                nfc_llcp_send_symm_req(state);
                break;
            }
            state = (state + 1) & 3;
            break;
        case 0xffff:
        default:
            if (flag == 0) {
                for (i = 0; i <= 499; i++)
                    asm("nop");
            } else {
                pf = nfc_cmd_buf;
                pf[0] = 1;
                pf[1] = 0;
                nfc_uart_send(pf, 2);
                nfc_uart_recv(pf, 1, (u8 *)&r);
                for (i = 0; i <= 4999; i++)
                    asm("nop");
                nfc_link_close();
                return -2;
            }
            break;
        }
        if (nfc_abort != 0) {
            nfc_link_close();
            return 4;
        }
        if (flag == 0) {
            if (nfc_ticks > c * 10) {
                nfc_link_close();
                return -4;
            }
        } else if (nfc_ticks - t0 > 50) {
            nfc_link_close();
            return -4;
        }
        for (i = 0; i <= 999; i++)
            asm("nop");
    }
}


/*
 * The RAM variables this file defines.
 */
struct Nfcid3 nfc_peer_nfcid3;
