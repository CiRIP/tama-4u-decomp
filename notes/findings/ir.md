# The legacy IR link (OPEN.md item 11)

Answer in short: the 4U+ firmware still contains the complete physical and packet layer of
the infrared link of the earlier models (bit-banged on P03/P04/P05, a 62 kHz carrier,
pulse-distance code, 20-byte packets with an 8-bit sum in byte 19), plus several
application-level session drivers. None of it runs in normal play, and it could not work
anyway: the receive interrupt is not in the vector table, and the session-level transfer
routine ir_transfer that ir_exchange depends on is an empty `ret` (this source leaves it,
ir_transfer_abort and their calls out).

Levels used below: **V** = verified in the code or the S1C33E07 manual (id001557.pdf), **I** =
inferred, with a confidence level.

## 1. Pins and hardware

| pin | IR role | evidence |
|---|---|---|
| P05 (SOUT1) | TX: IR LED drive | V: the carrier routine writes 0xE0 / 0x00 to P0 data; in the IR port setup ir_port_setup (IOC0 = 0x28) P03 and P05 are the only outputs, so bit 5 is the one that is driven. |
| P04 (SIN1) | RX: receiver output, active low, one pulse per light pulse | V: port-input interrupt FPT4. SPT4 = 00 (0x3003C1 is never written, reset value) selects P04. pPINTEL 0x17 makes FPT4 edge triggered, and pPINTPOL 0 makes it the falling edge (sys_gpio_init, ir_port_setup). Its ISR ir_edge_isr counts the edges. |
| P03 | transceiver shutdown, active high (I, medium-high) | Boot drives P03 high (P0D = 0xDF). ir_power_up drives it low and waits about 124 µs before receiving. After every transmission, ir_send_packet / ir_send_ping leave it high again (0x1F), and ir_power_down (0x18) does the same. The next receive must then re-power it: ir_xfer_started = 1 makes ir_recv_packet call ir_rx_start first. This matches the SD pin of an IrDA transceiver module. |

P04/P05 are the SIN1/SOUT1 pins of serial ch.1. On the 4U+ that UART goes to the PN512
(pP0_47_CFP = 5). The IR code switches the pins back to GPIO with pP0_47_CFP = 0
(ir_power_up, ir_port_setup). The wiring is therefore that of an IrDA transceiver on the
ch.1 UART (TXD = SOUT1, RXD = SIN1, SD on a GPIO) (I, medium). Whether a 4U+ board carries
such a part is not known. The code itself does not need one: on the 4U+ the IR routines
would toggle the PN512's serial input.

Side effect (V): the IR routines write the whole P0 data byte (0x00 / 0xE0 / 0x18 / 0x1F).
After main sets IOC0 = 0xA8, P07 is an output too, so it would toggle with the carrier.

## 2. Timer and RAM routines (hand-written assembly)

T16 ch.5 is the IR clock. Clock control 0x3007EA: bit 3 = on, bits 2-0 = MCLK/1, 2, 4, 16,
64, 256, 1024, 4096 (manual table IV.1.5.2). The counter resets on compare B, so a period
is CRB + 1 counts. MCLK = OSC3 = 30 MHz: sys_clock_osc3 writes pCMU_CLKCNTL = 0x770003, which
selects OSC3 with OSC3DIV = /1. All of this is V.

* tmr_t5_start_tick(a, b) / tmr_t5_start_raw(a, b): CRA = a - 1, CRB = a, clock control b | 8 or b,
  compare-A interrupt only (bit 7 of pINT_E16T45). Vector 51 = tmr_t5_cmpa_vector (`pushn r2;
  jp [0xD94]`).
* sys_copy_ram_routines copies two routines into internal RAM, each up to and including the 0x0000
  halfword (the `nop`) that ends it. ir_carrier_burst goes to 0xD58..0xD8D. tmr_t5_tick_handler, the
  real compare-A handler (tmr_t5_ticks++, clear the flag with a halfword write of 0x80 to
  0x300284, popn r2, reti), goes to 0xD94..0xDB1. Until this copy runs, 0xD94 holds zeros.
  comm_prepare (the communication menu entry) runs it, so the RAM handler is live: the
  delays tmr_t5_pause / tmr_t5_pause_restore and tmr_t5_wait_period depend on it. Only the 0xD58 copy is
  IR-only.
* ir_carrier_burst is the carrier generator, called as `call [0xD58]` with r6 = n pulses. It
  clears tmr_t5_ticks, reads the port address from ir_port_ptr (0x300380, set by sys_clock_osc3),
  and then on each tick alternately stores 0xE0 (`ld.w %r9,0x20` sign-extends to
  0xFFFFFFE0) and 0x00. That gives n pulses of one tick high and one tick low. It leaves
  tmr_t5_ticks = 0 at the last tick, so the caller can time the following space. The
  `xld.w %r8,0x3007ae` is dead (overwritten).
* ir_detect_start (start-burst detector) and ir_decode_bytes (byte decoder) poll the T16 tick
  tmr_t5_ticks together with the two counters of the P04 ISR: ir_edge_count (edges since the
  last check, used as a "carrier present in this tick" flag) and ir_edge_count2 (edges in the
  current burst).

### Timing at 30 MHz (V for the register values; the µs figures are computed)

| item | setting | value |
|---|---|---|
| TX tick | tmr_t5_start_raw(0xF1, 8): 242 x MCLK/1 | 8.07 µs |
| carrier | 2 ticks per pulse, 50 % | **62.0 kHz** |
| start mark | 0x170 = 368 pulses | 5.94 ms |
| start space | DE4 > 0xB8, plus 1 tick | about 1.50 ms |
| bit mark | 0x12 = 18 pulses | 290 µs |
| space "0" / "1" | DE4 > 0x36 / > 0x64, plus 1 tick | 452 µs / 823 µs (bit 742 / 1113 µs) |
| stop mark | 0x2E = 46 pulses | 742 µs |
| ping (ir_send_ping) | 10 pulses only | 161 µs |
| packet | 20 bytes, LSB first | about 127-186 ms |
| RX sample tick | tmr_t5_start_tick(0x73, 3): 116 x MCLK/16 | 61.9 µs |

Receive acceptance (ir_detect_start, error code in ir_rx_status):
* 1 = no edge within 500 ticks (31 ms). ir_recv_packet retries 10 times.
* 0x11 = start burst longer than 98 ticks.
* 0x10 = more than 0x171 edges in the start burst.
* 2 = burst too short. The burst ends after 3 silent ticks and needs more than 0x168
  (360) edges.
* 3 = no edge within about 24 ticks after the burst.

Bits (ir_decode_bytes):
* A mark must have 14-24 edges within 8 ticks, otherwise ir_result = 5.
* A space longer than 10 ticks (619 µs) is a 1. Longer than 24 ticks is error 6.
* Bytes are shifted in MSB-ward, so LSB first. After 20 bytes ir_result = 0x14.

ir_recv_packet then checks byte 19 against ir_checksum (error 0x12). It sets 0x10 / 0x11
when ir_result was still 0, and returns ir_result (0x14 = ok). While a packet is sent or
received, pRTCINTMODE is set to 6 (1 s cycle, RTCIEN = 0) and to 7 afterwards.

The receiver counts every carrier pulse: 360-369 edges for the 368-pulse start mark, 14-24
for an 18-pulse bit mark. A demodulating (TSOP-style) receiver would give one edge per
burst, so the receiver must output the raw pulses, as IrDA transceivers do (I, high).

### IrDA or raw IR

The physical part fits an IrDA transceiver module (see above). The link protocol is not
IrDA: there is no SIR 3/16-bit pulse, no UART framing (the on-chip ch.1 UART, which has an
IrDA mode, is not used for this), and no IrLAP. It is a carrier-modulated pulse-distance
code in the NEC family (V for the code, I for the module).

The Tamagotchi Connection IR link was described by N. Silvanovich as "Nearly NEC", and
re-implemented in Tamagometer (https://resmer.co.za/ch/posts/tamagometer/,
https://github.com/zacharesmer/tamagometer). It uses the same scheme, with different
timings: about 9.5 ms + 6 ms leader, 600-690 µs marks, 650 / 1250 µs spaces. Its last
byte is the 8-bit sum of the others, and the name sits in the bytes after the header. The
4U+ code is the same family with faster timing. The Japanese iD / P's line it most likely
comes from has not been checked against these numbers.

## 3. Packet format (20 bytes: ir_tx_packet ours, ir_rx_packet the peer's)

The two buffers are adjacent: 0xDBC..0xDCF and 0xDD0..0xDE3, followed by tmr_t5_ticks.

| byte | content (from the builders ir_point_gift_packets, ir_test_hello, ir_test_packets) |
|---|---|
| 0 | low nibble 0xD = signature (a receiver rejects anything else). High nibble = bits 0-3 of a 6-bit code n. Bits 4-5 of n are in byte 2, bits 6-7. Sent: 0x8D -> n = 0x38 (56), 0xCD -> n = 0x3C (60). The IR test screens show n as two digits. TAMAGO records carry the same packed code at 0x4C/0x4D (ir_check_rx_record: low nibble 0xD, n > 0x2A) and 0xF8/0xF9 (item_is_special_builtin: n == 0x3C). Probably a model or protocol generation number (I, medium). |
| 1 | packet kind (below) |
| 2 | bits 0-5: ir_owner_id_lo = owner_id & 0x3F (part of the random owner id); bits 6-7: see byte 0 |
| 3 | 0x3D / 0x3F (bit 1 = pet_get_gender()), or (character flag << 6) \| character id bits (ir_test_hello) |
| 4 | bits 0-5: ir_owner_id_hi = (owner_id >> 8) & 0x3F. Bit 6 is read by ir_exchange (f_2500A9C), bits 6-7 = 0xC0 select the 0x27D5400 bank (ir_rx_packet.owner_id_hi). |
| 5.. | the owner name owner_name. 16 bytes are copied, so it runs over bytes 16-18, the checksum byte (recomputed) and one byte past the packet (tmr_t5_ticks). In ir_test_hello byte 10 is then replaced by ir_count_tier(x00c). |
| 11 | x00b: role / agreed game. Point gift: 1 = giver, 2 = receiver; marriage: game index. |
| 12-13 | x00c / x00d: amount (u16 LE) in the point gift; x00c = side in the photo |
| 14-15 | x00e / x00f: result or item index, bit 7 = flag |
| 16-17 | x010 / x011. A responder whose P06 reads high rejects a peer with x010 == 0x38 or x011 != 1 (ir_point_gift_packets). Our own 0x38 at x010 is overwritten by the name copy. |
| 19 | 8-bit sum of bytes 0..18 (ir_checksum, set by ir_send_packet / ir_send_ping) |

Packet kinds:
* 0 / 1 = hello / hello reply (ir_point_gift_packets, ir_test_hello, ir_test_packets).
* 0x70 / 0x71 = point gift offer / acknowledge (ir_point_gift_packets: x00b role, x00c/x00d
  amount; ir_apply_point_gift applies it). A peer 0x70 in ir_exchange likewise grants 1000
  points. The earlier comment called 0x70 "refusal", which was wrong.
* 0xB2 / 0xB3 = ir_test_exchange's exchange.
* 0x20 / 0x21 = visit start, 0x22 / 0x23 = visit end (visit_start, ir_visit_pair_check).
* Kinds used by ir_exchange (I, medium): 2/3 greeting, 4, 8/9 gift with item, 10/11 play
  together, 12/13 photo, 14/15 proposal, 5 marriage. ir_next_packet_kind computes "kind + 1" as
  the answer but never stores it.

## 4. Session layer

* ir_session(mode) sets comm_exchange_kind = mode and runs ir_session_run. It repeats once when
  ir_session_flags asks for it: bit 1 = stamp-card retry, bit 0 = marriage question
  comm_ask_marry.
* ir_session_run:
  1. Copy the RAM routines, set ir_port_ptr = P0, handle a running visit, mask the button
     interrupts and poll P00-P02 instead (btn_disable / btn_history_init / btn_history_sample),
     power the transceiver up (ir_rx_start), clear both buffers.
  2. Poll on a 6.3 ms tick (tmr_t5_start_raw(0x2D, 0xF)) until ui_timeout reaches 0 (10 s; 20 s
     in mode 5).
  3. If edges were counted (ir_edge_count, i.e. the peer pinged), we are the responder:
     comm_is_initiator = 0.
  4. Otherwise a fresh B press sends the 10-pulse ping (ir_send_ping) and we are the
     initiator: comm_is_initiator = 1. The comment in F7b said the opposite.
  5. In both cases ir_exchange runs the exchange. C cancels.
  6. ir_answer_only = 1 (set only by ir_menu_hidden_session) forbids initiating: the toy can only answer.
* ir_exchange drives everything through ir_transfer(&st, &sub, timeout) and
  ir_transfer_abort, and on the initiator side gets its data from ir_send_data_stub. All three are
  stubs in this firmware: `ret`, `ret`, `return 0` (V). st and sub are therefore
  uninitialised stack bytes. A session reaching ir_exchange fails, aborts, or loops
  forever (st == 0x10 -> `again`). The other application drivers that use the transfer
  are dead the same way: ir_receive_record_step (ir_session_receive_only) and ir_test_exchange (ir_test_session, also
  ir_send_data_stub_b = `return 0`).
* ir_point_gift_packets (point gift), ir_test_hello and ir_test_packets (IR tests that show the peer's
  code n) call the packet layer directly. As code they are complete.
* The P04 interrupt itself cannot be taken: vector 68 (port input 4) is sys_isr_halt_a
  (`halt` then loop forever), not ir_edge_isr. ir_edge_isr, and its setup ir_port_setup
  (the only writer of the priority register pINT_PP45L), are referenced nowhere (V). The
  priority register is undefined after reset, so if an edge did arrive with EP4 enabled
  (ir_rx_start sets it), the toy would hang. So ir_edge_count / DB8 never move: a toy can
  neither detect a peer's ping nor receive a packet.

## 5. Reachability of every entry

The code references were searched in src/ and asm/ by name, and the 32-bit word
references with a scan of the image. Result (V):

| entry | referenced by | how it could be reached |
|---|---|---|
| ir_menu_hidden_session (mode 4, answer-only) | menu tables comm_main_menu_items[4] and comm_main_menu_debug_items[6] (words at 0x253815C, 0x25381CC) | See below. Never in normal play. |
| ir_session5_unused (mode 5) | nothing | not reachable |
| ir_session3_unused (mode 3) | nothing | not reachable |
| ir_session / ir_session_run / ir_exchange | only the three above / ir_session / ir_session_run | as their callers |
| point gift ir_point_gift -> ir_point_gift_session -> ir_point_gift_packets | ir_point_gift only from ir_point_gift_menu_items (100$/500$/1000$, used by ir_menu_point_gift) and D_25380B0 ("あげる" -> ir_menu_point_gift, "もらう" -> ir_point_gift). ir_menu_point_gift is only in D_25380B0. | D_25380B0 is referenced by nothing, so not reachable. |
| ir_session_receive_only -> ir_receive_record_step | nothing | not reachable |
| ir_test_entry -> ir_test_session (-> ir_test_exchange), ir_test_packets | nothing | not reachable |
| ir_test_hello | nothing | not reachable |
| ir_port_setup, ir_edge_isr, ir_count_low | nothing | not reachable |

The menu driver ui_menu_run never moves the cursor past count - 1: the A press wraps to 0.
comm_main_menu sets idx = 0 on entry, but at `again` it re-selects the table from P06 and
keeps idx. So:

* comm_main_menu_debug_items[6] (count 6) is never reachable.
* comm_main_menu_items[4] (count 4) is reachable only through this sequence:
  1. Enter the communication menu with P06 low (the debug variant).
  2. Move to entry 4 or 5 ("デバックセット" / "アイテムSEND") and choose it.
  3. The handler returns 0 (always for comm_menu_debug_set; for comm_menu_item_send when the NFC
     session fails).
  4. P06 is high when the menu loop restarts.
  5. The normal table is now shown with idx = 4 (empty page). B calls comm_menu_enable(4),
     which returns 2 (non-zero, so accepted) with P06 high, then comm_main_menu_items[4] =
     ir_menu_hidden_session.

P06 is pulled up (pMISC_PUP0 = 0xEF) and is the hidden test strap. A retail toy therefore
never gets there.

Even when it is reached, ir_menu_hidden_session does almost nothing: ir_answer_only = 1, no ping is ever
seen, so the toy waits about 10 s on the menu screen (C cancels). Then visit_end runs and
the menu comes back. Nothing is transmitted, because answer-only mode never sends.

Any other entry would need a patched table or a downloaded app (native code, see README)
jumping to the address. The other entries have no reference at all.

What stays live from this group (V):
* sys_copy_ram_routines: the RAM copy at every communication menu entry.
* The delays tmr_t5_pause (a x about 31 ms, halt-wait on T16 ch.5) and tmr_t5_pause_restore.
* btn_wait_press_or_timeout: wait for a button or a timeout.
* btn_history_init / btn_history_sample: button polling, used by the NFC code.
* btn_disable / btn_enable: mask / unmask the buttons.
* tmr_t5_standard_tick: the 3 Hz menu tick.
* ir_power_down: called after every NFC session. It writes P0 = 0x18, i.e. the transceiver
  shut down and the LED off.

ir_anim_pause_a / A34 / A62 / A9C (F14) are not IR: each plays an animation and then pauses
k x 0.62 s, or 0.77 s for A9C.

## 6. Comment fixes made

* F7b: ir_session_run and ir_exchange (comm_is_initiator sense, kind 0x70, the stub transfer),
  the IR test and gift drivers, ir_session_receive_only.
* F7: ir_menu_hidden_session (how it can be reached), ir_session5_unused, ir_session3_unused, ir_point_gift,
  ir_menu_point_gift, D_25380B0, ir_session.
* F14: ir_anim_pause_a / A34 / A62 / A9C were described as "IR bursts"; they are animation +
  delay.
* U_2504xxx / U_2505xxx: the hand-written routines ir_detect_start, ir_decode_bytes,
  ir_send_packet / ir_send_ping / tmr_t5_pause / tmr_t5_pause_restore, ir_carrier_burst, tmr_t5_cmpa_vector /
  tmr_t5_tick_handler, btn_wait_press_or_timeout.
* Pins and timings in ir_power_up, ir_power_down, ir_rx_start, sys_clock_osc3,
  ir_recv_packet, btn_enable (it is a 62 ms pause, not a sound), ir_port_setup and
  tmr_t5_standard_tick (duplicate comment merged).
* The ir_rx_packet / ir_tx_packet structs in globals.h.

## 7. Still open

* Whether a 4U+ board has any IR part on P03/P04/P05 (a board photo would settle it), and
  which module the earlier boards used.
* The real message kinds of the iD / P's session layer, the code behind ir_transfer. The
  application code in ir_exchange shows only which kinds it expects.
* The meaning of the 6-bit code n in byte 0/2 (56 sent by the gift and test paths, 60 by
  ir_test_hello, 60 also tested in TAMAGO records).
* The pRTCINTMODE 6/7 question: other code also leaves it at 6 (RTCIEN = 0), and how the 1 s
  tick is still delivered then is not checked here.
