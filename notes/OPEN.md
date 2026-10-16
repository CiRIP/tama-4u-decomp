# Open questions

The 16 questions of the first list are answered in [findings/](findings/) (summary in README.md). What is left, each with the report that raised it.

## Needs hardware or outside material (owner's answers, 2026-10-14)

1. **The P07-low LCD controller** (lcd.md): an exact ILI9163C or a variant? Its gamma tables only take effect on an ILI9163C after F2 01, which is never sent. Owner: no 4U probably shipped with this type of LCD; left as is.
2. **Store Touch Spots** (spots.md): which technology they used (48-byte Type 2 tag or an ISO-DEP device speaking the card protocol). Owner: the spots were mostly destroyed, so the code is all there is; spots.md's reading stands.
3. **Board wiring**: the piezo between P10 and P11 (sound.md); any IR part on P03..P05 (ir.md); whether P13 / P14 drive the PN512 power / reset (nfc.md). Owner: the MCU is a chip-on-board under a blob, so the connections can't be traced; the code-based readings stand.
4. **Touch card files** (cards.md): the toy reads the registration record (use counter + the two owner slots) with P2 = 2 and the payload (TAMAGO files) with P2 = 3. emu4u indexes its files by P2 and dump_card.py reads P2 = 1..3, but the file at P2 = 1 is probably an emulator artifact (owner: emu4u creates a 0-size file there because it assumes the files are numbered in sequence in the applet install TLV). Owner's recollection: one file holds the registration count and the other the TAMAGO payloads, which is what the toy uses. Also unknown: the unreachable APDUs 90 F5 and 90 C4.

## Firmware-only

5. **Debug アイテムSEND** (sessions.md): it sends the item at flash 0x2070000 as an NFC-DEP initiator that only a peer with NFCID3 F0 40 answers, which no 4U+ in app mode or idle accepts. Its intended receiver, and what normally sits at 0x2070000.
6. **Session types 9 / 10** (sessions.md): only their phase-2 message codes exist.
7. **TAMAGO header bytes** 0x30, 0x34, 0x4C / 0x4D (0x4D = protocol version 4?), 0x72, 0x78, and the bingo / gashapon bytes 0x9E..0xA3 (tamago.md, testmode.md).
8. **IR**: the 6-bit code in packet bytes 0 / 2, and the real message kinds of the earlier models' session layer (ir.md). How the RTC 1 s tick arrives while pRTCINTMODE is left at 6 (ir.md, testmode.md).
9. **Sound ids**: names for the unlabelled ones (the animation scripts in F0 would label most) and whether the 19 ids never passed as constants are reachable (sound.md). asm/ui/ui_text_banner.s labels the sound period / sequence data as functions.
10. **Idle touch**: whether a field that stays on retriggers the touch after the detector is re-armed (spots.md). The house level-up texts (table 100, entries 0xB3..0xB6).
11. **Test mode** (testmode.md): why the power-down step sleeps twice; which device answers as peer 0xF0F0 in the link test.

## Firmware bugs found (harmless in practice)

* nfc_uart_err_isr reads the UART ch.0 status instead of ch.1, so receive errors are never recorded (nfc.md).
* snd_ch3_off clears T16 ch.1's control register instead of ch.3's; never reached. snd_voice_load tests the wrong value for the transposition bound (sound.md).
* flash_fill and flash_program_bytes return -1 on a flash error with interrupts still off (testmode.md).
* The 30 s phone wait in comm_exchange_target is ignored by nfc_receive, which runs up to 6 x 10 s SNEP sessions (nfc.md).
