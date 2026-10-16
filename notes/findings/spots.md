# Touch Spots, touch cards and the idle-touch counter

OPEN.md items 4 and 7. Sources: src/spot/spot_reader.c (reader, deliveries), src/lib/nfc/nfc_reader.c,
lib/nfc/nfc_reader.c, lib/nfc/nfc_tags.c, lib/nfc/nfc_tags.c, lib/nfc/nfc_tags.c, lib/nfc/nfc_tags.c, lib/nfc/nfc_protocol.c (reader driver),
src/spot/spot_bingo_menu.c (Bingo / Gashapon menu), src/nfc/nfc_session.c, nfc/nfc_session.c, lib/nfc/nfc_dep.c (peer
detection), src/comm/comm_outcome.c (idle event), src/room/room_window.c (house), docs/emu4u-master.

Confidence: **V** = read in the code (C that builds byte-identical, or checked in the asm),
**I** = inferred.

## 1. Three ways a touch is handled

| | toy role | entry | what is accepted | data read |
|---|---|---|---|---|
| A. reader | PCD (polls Type A, then NFC-F) | spot_reader(a) | 48-byte Type 2 tag, ISO-DEP touch-card applet | yes |
| B. "touch any reader" | NFC-DEP target, field detect | spot_wait_reader (Bingo/Gashapon), nfc_idle_touch -> nfc_idle_target_session (idle) | any RF field that is not a Tamagotchi (peer code 0xF0EF) | nothing |
| C. つうしん sessions | NFC-DEP / SNEP | F69/F70 | another 4U, the phone app | session data (not this topic) |

Callers of the reader spot_reader(a) (V): a = 0 bingo spot (spot_bingo_read, from F45 with a
spot bingo card), a = 1 the タッチスポット menu (spot_touch_menu_entry), a = 2 the set-up flow
sys_startup_flow right after the name entry when the P06 test strap was held at boot
(sys_debug_strap_count == 8, via spot_reader_mode2), a = 3/4/5 the test-mode R/C/W screens (test_spot_screen from
test_mode). The old comment "a = 1 Gashapon, 2 generic spot" was wrong: Gashapon never uses
the reader.

## 2. Card types recognised by the reader (V)

nfc_poll_tag polls up to 30 s (x2c = 30, d * 10 ticks of 100 ms; the old comment said 3 s)
alternating nfc_activate_type_a (Type A) and nfc_poll_felica (NFC-F). Type code in x28:

* nfc_activate_type_a: ATQA as received; SAK bit 5 (ISO 14443-4) -> 0x0EEF; ATQA 0x0044 -> probed by
  nfc_identify_type2_tag, which reads the capability container (bytes 12..15 = page 3): BuiltinItemKeyEntry 10, then
  (CC3 << 8 | CC2) * 8 = 48 -> 0x0810, 128 -> 0x0812, 888 -> 0x0816, else 0x0044.
* nfc_poll_felica: FeliCa polling (06 00 FF FF 01 00, system code request) -> IDm into the UID
  buffer, system code into x2a, type code 0x0FFF.

spot_reader accepts only **0x0810** and **0x0EEF**; MIFARE Classic (0x0004/0x0002, which the
driver can read), other Type 2 sizes (0x0044/0x0812/0x0816) and FeliCa (0x0FFF) give the failure
screen. So:

* "ATQA 0x0810" in OPEN.md is not an ATQA: it is the firmware's code for a Type 2 tag with
  ATQA 0x0044 and CC = BuiltinItemKeyEntry 10 06 00, i.e. a 48-byte MIFARE Ultralight / NTAG210 that is
  NDEF-formatted with write access (CC3 must be 0x00, else the code is no longer 0x0810).
* "record at page 0x10" is wrong: nfc_type2_read takes byte offsets (page = b >> 2). The record is
  bytes 0x10..0x3F = pages 4..15 = the whole 48-byte user area. Writes go to pages 4 and 15.
* NFC-F is polled but never accepted (no FeliCa spot path exists in this firmware).

## 3. Type 2 spot record (48 bytes, V)

| bytes | meaning |
|---|---|
| 0..1 | 00 80 (shown as "VER" on the R test screen) |
| 2..3 | remaining uses, big-endian; 0xFFFF = unlimited |
| 4 | kind ("INF" first number) |
| 5 | page byte of the text (high byte of each 2-byte game character, 04; "INF" second) |
| 6 | bingo spot id, must equal byte 0xEE of the installed bingo card ("CLS" first) |
| 7 | flags: bit 0 counted, bit 1 register the name (ISO-DEP only), bit 2 not twice in a row ("CLS" second) |
| 8..9 | parameter: kind 8 points, kinds 9/10 visitor character id, bingo: byte 9 = number 1..9 |
| 0xA..0x2D | 36 text characters (low bytes) |
| 0x2E..0x2F | 16-bit byte sum of 0..0x2D (spot_check_sum) |

The field names VER / INF / CLS come from the test screen (test_spot_show_ver / test_spot_show_inf /
test_spot_show_cls write "VER", "INF", "CLS" in game characters 04 F5 04 E4 04 F1 etc.).

Kinds: from the menu (a = 1/2): 1 random idle animation pet_event_idle_random, 2 visitor phrase
pet_event_phrase, 3 profile phrase spot_profile_talk, 4 card-matching game + prize
game_score_reward(game_slot_screen()), 7 the 36-character message (spot_show_message), 8 Gotchi points
= bytes 8..9 (spot_gift_points -> anim_points_gain animation, pet_add_points), 9 / 10 a visitor (id
bytes 8..9 -> spot_visitor_sprite -> chara_select_other_by_set) saying the message (spot_visitor_message / spot_visitor_scene);
0, 5, 6 fail. From the bingo menu (a = 0): only kind 6. Kinds 1-4 are exactly the events of
the idle reader touch comm_idle_touch_event. A Type 2 tag cannot carry a TAMAGO file: no item ever
comes from this path.

## 4. ISO-DEP touch card (V, matches emu4u)

* card_activate sends `90 1B 00 00 08 AE 51 9F B5 8A 78 71 F4 04`, byte for byte the AUTH APDU
  of emu4u's scripts/dump_card.py (key D_255D378). card_read reads with
  `90 AD 00 <file> 00 00 03 <24-bit address> <16-bit Le>` (extended length, as emu4u's
  `ExtendedLength` applet), card_write writes with 90 8D. The file is P2 = card_file_no, set by
  card_set_params(2 or 3, key, 1): card_set_params's "2 = SELECT by AID, 3 = SELECT file" reading is
  wrong (it is the file number). The toy uses files 2 and 3; file 1 (present in emu4u's dump
  script) is never read by the toy. APDU-level details are for the APDU report.
* File 2 (0x3C bytes): 00 80, 32-bit uses at 4..7, two 0x18-byte name slots at 8 and 0x20
  (name owner_name, 8 characters, + birth month owner_bday_month + birth day owner_bday_day as 2-digit
  numbers; 0xFF = free), sum at 0x38..0x39.
* File 3: 0x30-byte header [0..3] length, [4] kind, [5] content type 0x65 / 0x66, [6..7] spot
  id / flags, [8..9] parameter, [0xA..0xB] 00 80; then the content. From the menu: kind 0 or
  0xFF, length 1..0xF000 -> the whole file 3 (header included) goes straight into flash
  0x2050000, checked by the sum in its last 2 bytes. 0x65 -> spot_content_browser (characters first,
  then TAMAGO items: food, accessories, backgrounds, games/apps, goods/coupons, bingo cards,
  Gashapon tickets); 0x66 -> spot_bingo_card_screen (a bingo card for sector 0x7D). From the bingo menu:
  type 0x66 with kind 0 or 6 acts as a bingo spot (same id check, number in byte 9).
* The 4U download pack's `cards/` folders (one character + 3-4 items per card: e.g. rolatchi =
  character, plush, glasses, dress, "touch living") fit content type 0x65.

## 5. Spot or card: what the firmware distinguishes

There is no "spot" or "card" flag (V). The firmware distinguishes only

1. the technology: 48-byte Type 2 tag vs ISO-DEP with the applet, and
2. the record: kind / content type and the flag bits of byte 7.

What makes the toy behave "like a card": flag bit 1 on an ISO-DEP record -> the name +
birthday registration in file 2, refused with the two-names screen (state 4, picture 0x211310C)
when both slots belong to other toys; registered cards are not counted (V). That is the
"at most 2 tamas" rule.

What looks like a spot: flag bit 0 (a use counter stored on the tag itself, so shared by all
toys: "ここからは もう もらえないよ!" at 0) and flag bit 2 ("れんぞくで おなじスポットに
タッチできないよ!": refused when the UID equals the previous touch's, spot_last_uid). Both
messages say スポット and both work on either technology (V).

Conclusions:

* The ISO-DEP path is the touch card (V: key, CLA/INS, addressing identical to emu4u; I: the
  registration flag is what the sold cards set).
* A separate path that fits a cheap store spot exists: the 48-byte Type 2 tag (V that it exists;
  I that stores used it). It gives events, messages, Gotchi points, visitors and bingo numbers,
  never items, and the toy writes its counter back, so it must be a writable tag (a powered
  device emulating Type 2 would also have to accept the writes and the read-back check).
* Spots that delivered TAMAGO items must have been ISO-DEP devices speaking the card applet
  protocol (I), e.g. an applet card or a reader in card emulation inside the spot, without the
  registration flag. Nothing in the code tells such a spot from a card.
* Bingo spots are verified: picture 0x2111EF4 "このビンゴせんようスポットに タッチしてね" (touch
  this bingo's own spot) is shown when the spot id (byte 6) is not the card's byte 0xEE.

Per-spot identity (V): the bingo spot id (byte 6 vs card byte 0xEE) and the UID, kept only as
"the last one" in RAM spot_last_uid (10 bytes, outside the saved blocks 0x119C..0x13E3 /
0xE9C..0x13C3). No store id, no date or time check, no once-per-day limit in the reader. The
only daily limit is the touch bingo (3 plays a day, spot_bingo_plays_today, cleared at midnight in F30).

## 6. Bingo and Gashapon (V)

Bingo card ids (spot_bingo_card_kind, item at 0x2750000): x001-x299 = **spot bingo**: F45 ->
spot_bingo_read -> spot_reader(0); the number is byte 9 of the spot (> 9 -> 1), marked by
spot_bingo_screen(k); no daily limit. x301-x599 = **touch bingo**: spot_touch_any_reader(0) ->
spot_wait_reader (touch any reader, nothing read) -> spot_bingo_screen(0) = a random number, at most 3
plays a day. The earlier comments ("1 active card, 0 used/completed") were wrong.
Gashapon (ticket x601-x999 at 0x2760000): spot_touch_any_reader(1), touch any reader, then the capsule.
The card's 3 x 3 grid of numbers 1..9 (tbl in spot_bingo_mark) matches the bingo pictures in the
download pack (bingogames/*.jpg).

## 7. Idle touch counter (item 7)

* Trigger (V): nfc_idle_touch -> nfc_idle_session -> nfc_idle_target_session. When the PN512 reports an RF
  field, nfc_detect_initiator listens 2 s as NFC-DEP target; an ATR_REQ with NFCID3 F0 40 = another
  Tamagotchi, anything else (3 other frames, or no frame at all before the timeout, which
  returns 0 with 0xF0EF) = "reader" -> comm_idle_touch_event (event by day of month) -> comm_count_and_save.
* "Different readers" are not told apart (V): nothing about the reader is read or stored. No
  reader id list exists anywhere (the "tag id list" of Fable's notes was already refuted). Each
  touch counts; the only per-tag memory is spot_last_uid in the reader mode, and that only blocks
  an immediate repeat on flag bit 2 records.
* comm_count is not an idle-touch counter but the general communication count (V): +1 in
  comm_count_and_save (idle touch comm_idle_touch_event; Tama-to-Tama / app sessions nfc_session_initiator,
  nfc_session_target, nfc_idle_target_session) and comm_count_add (Touch Spot reader spot_reader, touch Bingo /
  Gashapon spot_touch_any_reader); capped at 1001; saved (inside 0x119C..0x13E3).
* Thresholds (V):

| count | effect | code |
|---|---|---|
| 5 | house level 1 (small house) | comm_house_level, pet_house_level |
| 5, 10 | "こそだてちょうを てにいれたよ!" animations; two character badges lit on the "このたまごっちが ついかされたよ" page (> 4, > 9); growth candidates 1006 (> 4) and 21 (> 9) in evo_pick_adult_girl / evo_pick_adult_boy | comm_notebook_unlock_check, dl_chara_added_screen |
| 20 | house level 2 (two-storey house); town entry 4 たまのうえん opens; entry 5 であいストリート appears as "???" | comm_house_level, town_farm_state_cb, town_street_state_cb |
| 50 | であいストリート opens | town_street_state_cb |
| 100 | house level 3 (mansion) | comm_house_level |
| 1000 | house level 4 (palace) | comm_house_level |

  The house pictures (0x218A91C cabin, 0x218BB44, 0x218CD6C, 0x218DF94, 0x218F1BC palace,
  128 x 72, 16 colours) are drawn in the outdoor scene by room_window_draw (the old comment called
  them a "trophy"). A level change is announced by the mascot 0x49 (texts 100/0xB3..0xB6) when
  the menu key is handled. That the badges show the characters 1006 / 21 is I (same
  thresholds, picture content not decoded).

## 8. Remaining unknowns

* Which technology the real store spots used cannot be decided from the firmware alone; a dump
  of a spot (UID / ATQA / SAK and its 48 bytes, or its file 3 header) would settle it.
* Whether a Type 2 spot was a sticker tag or a powered device emulating one.
* Whether an RF field that stays on retriggers the idle touch after nfc_wait_field_on re-arms the
  field detector (PN512 behaviour, PN512 report).
* The texts 100/0xB3..0xB6 of the house level-up were not decoded.
* Picture 0x2083ADC / 0x20828D0 (test C screen) not decoded (1-bit format differs).
