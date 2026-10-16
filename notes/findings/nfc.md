# NFC: the PN512 code against the datasheet (OPEN.md items 1, 9, 12, 16)

Sources: docs/PN512.pdf (NXP PN512 data sheet rev. 5.3, section and table numbers below),
docs/id001557.pdf (S1C33E07 Technical Manual rev. 1.4), the Epson 33E07 demo BIOS headers
(docs/33E07_Demo_Ver1.2_Release/include/BIOS/io_sio.h, io_itc.h), and the C/asm of
0x252C5A6..0x2535200. "Verified" = read in the code and checked against the documents.

## Short answers

* **TxMode/RxMode 0xA1 = 424 kbit/s ACTIVE mode: confirmed** (PN512 Tables 51-54: bit 7
  CRC on, bits 6-4 = 010 = 424 kbit, bits 1-0 = 01 = Active Communication mode). README item 4
  can be closed. TxAuto 0x8B (target) / 0x8F (initiator, adds InitialRFOn) are the NFCIP-1
  active-mode RF collision-avoidance settings (Table 58), which confirms it independently.
* **Modes actually used** (verified):

  | Use | Functions | PN512 set-up |
  |---|---|---|
  | Sleep / wake on field | nfc_init, nfc_link_close, nfc_rearm_field_detect (nfc_sleep_until_field) | soft power-down (CommandReg 0x37), RF level detector, IRQ on RfOn (DivIEn 0x82) or RfOff (0x81) |
  | Tamagotchi to Tamagotchi | detect: nfc_detect_target (initiator) / nfc_detect_initiator (target); data: nfc_dep_send (sender = initiator) / nfc_dep_receive (receiver = target) | NFCIP-1 **active**, **424 kbit/s**, NFC-DEP (ATR, then DEP_REQ/DEP_RES, DSL, RLS) |
  | Phone / "4U app" | nfc_snep_server (SNEP server), nfc_snep_client (SNEP client) | NFCIP-1 **passive 106 kbit/s**, toy = initiator with Type A polling (nfc_setup_passive_initiator, nfc_poll_type_a), ModeReg 0x7B (DetectSync = F0 start byte), NFC-DEP + LLCP + SNEP |
  | Tags / cards (touch spots) | nfc_poll_tag / 252D882 / 252D9C0 -> nfc_activate_type_a, 253122C, 25320AE, 2531864, 2532650, 2532BF8, 2531BD8, 253284A | ISO 14443A reader 106 kbit/s (WUPA, anticollision, MIFARE Classic with MFAuthent, Type 2 READ/WRITE); ISO-DEP after RATS + PPS at **424 kbit/s Type A** (Tx/RxMode 0xA0); FeliCa reader poll 212 kbit/s (nfc_poll_felica, Tx 0x92 / Rx 0x96) |
  | Card emulation (AutoColl / Config command) | none | never used: CommandReg is never written 0x01 (Config) or 0x0D (AutoColl) |

  No passive target, no FeliCa target in practice (see item 16).
* **Item 12**: nfc_uart_err_isr is the EFSIO **ch.1** receive-error ISR and reads 0x300B02 =
  EFSIF0_STATUS (**ch.0**). Bug confirmed, harmless (details below).
* **Item 16**: SENSF_RES (nfc_send_sensf_res) = `12 01 | 01 FE F0 40 r r r r | C0 C1 C2 C3 C4 C5 C6 C7`,
  no system code. It is never sent in practice (dead target branch of the phone link).
* **Host interface**: PN512 UART mode on S1C33E07 EFSIO ch.1, 8N1, 9566 bd at power-up and
  for the sleep set-up, 468750 bd (PN512 459661 bd) during sessions.

## 1. The serial link (verified)

EFSIO register map (io_sio.h; manual V.1): ch.0 at 0x300B00..09, ch.1 at 0x300B10..19
(TXD1 0x300B10, RXD1 0x300B11, STATUS1 0x300B12, CTL1 0x300B13, IRDA1 0x300B14, BRTRUN1
0x300B15, BRTRD1 0x300B16/17), ISO7816 regs 0x300B1A..1F, ch.2 at 0x300B20. STATUS bits:
0 RDBF, 1 TDBE, 2 OER, 3 PER, 4 FER, 5 TEND, 7-6 RXDNUM. ITC 0x300276 / 0x300286 (enable /
cause flag): bits 0-2 = ch.0 err/rx/tx, bits 3-5 = ch.1 err/rx/tx (io_itc.h).

* nfc_init sets P04/P05 to SIN1/SOUT1 (pP0_47_CFP 0x3003A1 |= 5), CTL1 = 3 (8-bit async,
  internal clock, 1 stop, no parity), then |= 0xC0 (TXEN1, RXEN1), IRDA1 = 0x10 (DIVMD1 =
  1/8 sampling clock, normal interface).
* Baud (manual V.1.2, async: BRTRD = fBRCLK x DIVMD / (2 x bps) - 1, fBRCLK = MCLK):
  bps = MCLK / (16 x (BRTRD + 1)) with MCLK = 30 MHz:

  | BRTRD1 | CPU bd | PN512 SerialSpeedReg | PN512 bd (Table 147 / eq. 2) | used by |
  |---|---|---|---|---|
  | 0xC3 | 9566 | 0xEB (reset) | 27.12e6 / 44 / 64 = 9631 | start-up, close, sleep set-up |
  | 3 | 468750 | 0x3A | 27.12e6 / 59 = 459661 (-2.0 %) | nfc_link_open (every session) |
  | 1 | 937500 | 0x1C | 27.12e6 / 29 = 935172 | nfc_link_open_921k, unused |

  The 2 % mismatch at "460.8k" is within 8N1 tolerance. SoftReset (CommandReg 0x2F in
  nfc_link_close) puts SerialSpeedReg back to 9.6 kbd (18.3.1.11), hence the CPU goes back to 0xC3.
* Protocol (PN512 9.3.3, Tables 149-151): LSB first, no parity. Address byte = bit 7 R/W
  (1 = read), bit 6 reserved 0, bits 5-0 register address 0x00..0x3F (all four pages are
  addressed directly; PageReg is never used). Write = address + data; the PN512 **echoes the
  address byte** (Table 150). Read = address byte; the PN512 answers the value. So:
  * `0x09, b` = write FIFODataReg (frame bytes), `0x0A, 0x80` = FIFOLevelReg FlushBuffer,
    `0x0D, 0x80` = BitFramingReg StartSend, `0x0D, 0x87` = StartSend with TxLastBits 7
    (REQA/WUPA short frame), `0x01, 0x0C` = CommandReg Transceive, `0x01, 0x00` = Idle.
  * `0x8A / 0x88 / 0x86` = read FIFOLevel / Status2 / Error, the polling triple. A frame is
    taken when FIFOLevel is in range, Status2 ModemState (bits 2-0) is not 2 (TxWait) / 3
    (Sending), the UART status is clean and Error & 0x37 (or 0x33) is 0 (RFErr, BufferOvfl,
    CRCErr, ParityErr, ProtocolErr; CollErr 0x08 is ignored because it is always 1 at
    212/424k, Table 30). Error bit 6 TempErr -> return 3 everywhere.
  * `0x89` = read FIFOData, `0x81` = read CommandReg, `0x8C` = read ControlReg (RxLastBits:
    0x14 = Initiator + 4 bits = MIFARE ACK), `0x9F` = read SerialSpeedReg.
  * After writing n pairs the code reads n bytes back (the echoes) and drops them.
* Wake-up handshake (nfc_link_open / 252C8CA / 252CFD8): CommandReg = 0x27 (RcvOff,
  PowerDown 0, NoCmdChange), then "write PageReg 0x00 = 0x55" until the echo 0x00 arrives.
  This follows datasheet 15.2 (send 0x55, then access register 0 until the chip answers).
* IRQ pin: P94 port interrupt -> nfc_irq_isr sets nfc_field_irq. DivIEn bit 7 IRQPushPull = 1,
  ComIEn never written (reset 0x80 = IRqInv) -> push-pull, active low. Only RfOnIEn /
  RfOffIEn are ever enabled; all protocol work is polled, no PN512 interrupt source is used
  during transfers.

## 2. Item 12: the error ISR (verified, impact inferred, high confidence)

nfc_uart_err_isr (vector of ITC flag 0x300286 bit 3 = ch.1 receive error) clears its flag and
does `nfc_rx_fifo.status |= REG8(0x300B02) & 0x1C`. 0x300B02 is EFSIF0_STATUS (ch.0); the NFC link
is ch.1 (0x300B12). Ch.0 is never touched anywhere else in the firmware (no other access to
0x300B00..09), so its error bits stay 0 and nothing is ever added to `st`. The ch.1 error
flags OER/PER/FER are never cleared (they clear only by writing 0, manual V.1.3.3 / V.1.4.3),
but errors do not stop reception (manual: "the receive operation is continued"). Effect: the
UART error reporting of nfc_uart_recv is dead (only the timeout bit 0x80 and the flag bit can
show up); a corrupted byte on the link is not reported at the UART level and is only caught
by the frame checks (PN512 ErrorReg, length, content, flash-write result). A real bug, but
one without visible consequence. Comment fixed in src/lib/nfc_hw/nfc_hw.c and include/globals.h.

## 3. Register sets (verified; datasheet section 8.2)

Values written as `register = value`; see the per-function comments for order.

* **Sleep** (nfc_init end, nfc_link_close, nfc_rearm_field_detect): DivIEn 0x82 (IRQPushPull +
  RfOnIEn) or 0x81 (RfOffIEn) by nfc_wake_on_field_off; ComIrq = DivIrq = 0x7F (Set bit 0 = clear all);
  RxThreshold 0x55; RFCfg 0x59 (RFLevelAmp 0, RxGain 101 = 38 dB, RFLevel 1001 = about
  0.24 Vpp, Table 155); GsNOn 0xF4; TxAuto 0x40 (Force100ASK); TxControl 0x80 (only in
  nfc_init); CommandReg 0x37 = RcvOff + PowerDown + NoCmdChange (soft power-down; the RF
  level detector keeps working, its amplifier is off, 11.3).
* **Bring-up** (nfc_init): CommandReg 0x22 = RcvOff + **Generate RandomID** (18.3.1.3),
  poll CommandReg until Idle, FIFO flush, ControlReg 0x20 = WrNFCIDtoFIFO, read 10 bytes into
  nfc_id3, force bytes 0/1 and 8/9 to F0 40. **Our NFCID3 is random**, regenerated at each
  bring-up (start-up, or after a failure via nfc_power_up), not a fixed chip ID as the
  earlier notes said.
* **Session open** (nfc_link_open): DivIEn 0x81, clear IRQs, RxThreshold 0x84, RFCfg 0x48,
  GsNOn 0x88, TxAuto 0x00 = the reset values (undoing the sleep set-up), SerialSpeed 0x3A.
* **Active 424k target** (nfc_detect_initiator; nfc_dep_receive the same without Control/TxControl/Mode
  and reload 0x30): Control 0x00 (target), TxMode = RxMode 0xA1, TxAuto 0x8B (AutoRFOFF,
  CAOn, Tx2/Tx1RFAutoEn), RxThreshold 0x85, Demod 0x61, RFCfg 0x79 (RxGain 48 dB, RFLevel
  1001), GsNOn 0xFF, CWGsP 0x3F, ModGsP 0x0F, TxControl 0x80, TMode 0x82, TPrescaler 0xA6,
  TReload 0x0020, Mode 0x3B, FIFO flush, Command 0x0C.
* **Active 424k initiator** (nfc_setup_active_initiator, used by nfc_detect_target; nfc_dep_send inline copy
  without TxControl/Mode, reload 0x30): as above with Control 0x10 and TxAuto 0x8F (adds
  InitialRFOn).
* **Passive 106k Type A initiator for the phone** (nfc_setup_passive_initiator): Control 0x10, Tx/RxMode
  0x00, TxAuto 0xCF (AutoRFOFF, Force100ASK, CAOn, InitialRFOn, Tx2/Tx1RFAutoEn),
  RxThreshold 0x85, Demod 0x61, RFCfg 0x79, GsNOn 0xF4, CWGsP 0x3F, TxControl 0x83 (field
  on), Mode 0x3B, ModGsP 0x11, timer 0, Command 0x0C. nfc_poll_type_a: REQA 0x26 / 0x87,
  anticollision with Tx/RxMode 0, SELECT with Tx/RxMode 0x80. Then nfc_snep_server / 252FE2C:
  **Mode 0x7B = 0x3B + DetectSync** (bit 6: "waits for F0h ... F0h is added as a Sync-byte,
  only for 106 kbit NFCIP-1", Table 50): the NFC-DEP start byte. The old comment "TxWaitRF /
  PolMFin" was wrong.
* **Type A reader for tags** (nfc_activate_type_a): Control 0x10, Tx/RxMode 0, TxControl 0x83, TxAuto
  0x40, RxThreshold 0x85, Demod 0x4D, RFCfg 0x79, GsNOn 0xF4, CWGsP 0x3F, ModGsP 0x11, timer 0,
  Command 0x0C; **WUPA 0x52** (not REQA as the comment said), anticollision/SELECT as above.
* **ISO-DEP** (card_activate): RATS E0 50, ATS must contain "TNPS064", **PPS D0 11 0A** (DSI =
  DRI = 2 -> 424 kbit/s both ways; the old comment called it S(DESELECT)-like), then Control
  0x10, **Tx/RxMode 0xA0** (CRC, 424 kbit/s, 14443A framing), TxAuto 0, RxThreshold 0x85,
  Demod 0x4D, RFCfg 0x79, ModWidth 0x0A, GsNOn 0xF4, CWGsP 0x3F, ModGsP 0x11, Command 0x0C.
  The optional APDU is `90 1B 00 00 08 <8 bytes> 04`, not `00 A4 04 00` (SELECT) as the old
  comment said (touch-card details: OPEN.md item 3).
* **MIFARE Classic** (nfc_mifare_read read, nfc_mifare_write write): MFAuthent (CommandReg 0x0E,
  18.3.1.10) with key A **FF FF FF FF FF FF** (fixed), wait for Status2 MFCrypto1On. The old
  comment of nfc_mifare_write called it a read with the card_set_params key: it is the write
  (A0 + 16 bytes, 4-bit ACK 0x0A) and the key is fixed.
* **FeliCa reader** (nfc_poll_felica): Control 0x10, TxMode 0x92 (CRC, 212k, FeliCa framing),
  RxMode 0x96 (+ RxMultiple), TxControl 0x80, TxAuto 0x07 (InitialRFOn + RFAutoEn),
  RxThreshold 0x55, Demod 0x61, RFCfg 0x59, GsNOn 0xFF, CWGsP 0x3F, ModGsP 0x0F, timer as
  active mode with reload 0x30, Command 0x0C. SENSF_REQ `06 00 FF FF 01 00` (RC 01 = return
  the system code, one slot). Result: IDm, system code, type 0x0FFF; the read/write
  dispatchers have no 0x0FFF case. nfc_send_sensf_req (`06 00 FF FF 00 01`) is unused.

## 4. PN512 timer settings (verified)

TModeReg 0x82 = TAuto 1, non-gated, no auto-restart, TPrescaler_Hi 2; TPrescalerReg 0xA6
-> TPrescaler = 0x2A6 = 678; DemodReg 0x61 / 0x4D have TPrescalEven = 0, so (section 14)
fTimer = 13.56 MHz / (2 x 678 + 1) = 9992.6 Hz = **100.07 us per count**.

| TReload | time (reload + 1 counts) | where |
|---|---|---|
| 0x0020 | 33 x 100.07 us = **3.30 ms** | nfc_detect_initiator, nfc_setup_active_initiator |
| 0x0030 | 49 x 100.07 us = **4.90 ms** | nfc_dep_receive, nfc_dep_send, nfc_poll_felica |
| timer regs 0 | timer off (TAuto 0) | nfc_setup_passive_initiator, nfc_activate_type_a |

TAuto starts the timer at the end of each transmission, but TimerIEn is never set (ComIEn is
never written) and ComIrqReg (0x84) is never read, so the timer has **no effect** on the
code: all waiting is done with the 100 ms tick nfc_ticks and poll counters. The values look
like copied reference settings (a 3.3 / 4.9 ms response timeout).

## 5. Item 9: timeouts (verified)

nfc_link_open calls nfc_tick_start, which sets nfc_ticks = 0; every protocol function opens the
front-end itself, so `nfc_ticks > x * 10` means **x seconds since this function opened the
link**. Inter-frame limits of 50 ticks are **5 s**, not 500 ms.

| Function | argument | first contact | between frames |
|---|---|---|---|
| nfc_detect_initiator (Tama target detect) | c: 30 (2 in session type 0), from nfc_find_peer_target | c s; listen window re-armed every 0.5 s; timeout returns 0 with peer 0xF0EF | - |
| nfc_detect_target (Tama initiator detect) | c = 30 (nfc_find_peer_initiator) | c s, -4 | ATR_REQ resent each 100 ms tick |
| nfc_dep_send / nfc_dep_receive (Tama data) | n = 10 (F70b; 8 in type 0 receive) | n s | 5 s |
| nfc_snep_server (SNEP server) | 10, fixed in nfc_receive; up to 6 attempts | 10 s (Type A poll rounds of 5 s) | 5 s |
| nfc_snep_client (SNEP client) | c from nfc_send (n = 10); up to 3 attempts | c s | 5 s |
| nfc_poll_type_a | c in ticks: 50 = 5 s | | |
| reader functions (nfc_activate_type_a, 2530F1A, 2532BF8, 253122C, 2531864, 2532650...) | seconds (x * 10) | | |

The "30 s phone wait": F70b comm_exchange_target sets n = 30 for an 0xF0F0 peer, but nfc_receive
ignores c on the phone path, so the effective wait is up to 6 SNEP sessions of 10 s plus
short delays (about 60 s). The 30 s waits that are real are nfc_find_peer_target -> nfc_detect_initiator
(waiting for a Tamagotchi initiator) and nfc_find_peer_initiator -> nfc_detect_target. nfc_short_delay (the
"0x28 / 0x14 ticks" of nfc_send / 2529F0C) is a busy loop of 24000 x 4 instructions, about
3-5 ms (estimate), not a 100 ms tick. Wrong "1/10 s" / "500 ms" comments fixed in lib/nfc/nfc_dep,
lib/nfc/nfc_dep, lib/nfc/nfc_protocol, lib/nfc/nfc_snep (both functions), lib/nfc/nfc_tags ("500 ms re-init" was a
re-open of the front-end, no fixed delay), F69 and F70b.

## 6. Item 16: NFC-F SENSF_RES (verified content; "never sent" inferred from the RF set-up, high confidence)

nfc_send_sensf_res writes `LEN 0x12, 01, 01 FE, nfc_id3[0..5], C0 C1 C2 C3 C4 C5 C6 C7` to the
FIFO and sends it: IDm (NFCID2) = **01 FE F0 40 r2 r3 r4 r5** (01 FE = NFC-DEP capable; F0 40
= the Tamagotchi marker; r = random bytes from Generate RandomID), PMm = **C0..C7**, no
system code/RD (18-byte form). The PMm C0..C7 (with 01 FE NFCID2) matches the sample values
of NXP's PN53x NFC-DEP target examples (inference).

Callers: only the `0x00FF` case of the frame dispatch in nfc_snep_server and nfc_snep_client, for
a received `06 00 FF FF 00 xx` (SENSF_REQ, system code FFFF, RC 00). Both functions run
the PN512 exclusively as a passive 106 kbit/s Type A initiator: their `mode` is a constant
0 (asm: `ld.w %r4,0x0; cmp %r4,%r4; jrne` at the top of the loop), so the active-mode branch
(nfc_setup_active_initiator, ATR_REQ(1)) is dead. In that set-up the receiver uses 106k 14443A framing and
the phone is the target, so a FeliCa SENSF_REQ (212/424k) can never be received and the
SENSF_RES is never sent. The same holds for the other target-side cases (ATR_REQ ->
nfc_send_atr_res, DEP_REQ, SENSF_RES 01 FE -> ATR_REQ(1)). They are leftovers of a generic NFC-DEP
state machine. The toy is never a FeliCa (or Type A) card/target: AutoColl and Config are
never issued.

## 7. Other comment corrections in this pass

* nfc_activate_type_a: WUPA 0x52, not REQA; no "500 ms" re-init.
* nfc_setup_passive_initiator: ModeReg 0x3B has CRCPreset 11 (FFFF, CalcCRC only), not 6363.
* card_activate: PPS (424k), not DESELECT; APDU bytes; timeout in seconds.
* nfc_poll_felica: real request is `06 00 FF FF 01 00` at 212 kbit/s (comment named the unused
  nfc_send_sensf_req frame).
* nfc_mifare_write: write, not read; fixed key FF x 6.
* New comments: nfc_irq_isr, 252C5CE, 252C8CA, 252CBEA, 252CE54, 252CF4C, 252CFD8, the
  three SIO ISRs, nfc_init, nfc_dep_receive, nfc_mifare_read, nfc_type2_read, card_read,
  nfc_id3.

## Remaining unknowns

* Exact length of nfc_short_delay / the nop loops (flash wait states of the S1C33E07 set-up).
* Whether the toy-to-toy DEP has any defined DID/NAD handling beyond what is coded (the DSL
  "DID" byte is the PNI) - fine as long as both sides run this code.
* The APDU semantics of the touch cards (90 1B / 90 AD / 90 8D) belong to OPEN.md item 3.
* Whether P13/P14 drive the PN512 NRSTPD (nfc_hw_power_on) was not checked against the board.
