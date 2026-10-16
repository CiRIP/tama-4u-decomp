# Touch cards and flash sector 13

OPEN.md items 3 (card protocol vs emu4u) and 6 (flash 0x2050000). Sources: the firmware
(`src/`, the built image), emu4u (`docs/emu4u-master`: `Emu4U.java`, `File.java`,
`Constants.java`, `build.xml`, `scripts/dump_card.py`) and the sample payloads in
`docs/4u-download-pack/cards/` (JPEG previews with a TAMAGO file appended).

Labels: **V** = verified in code / data, **I** = inferred, with confidence.

## 1. Card protocol

### Transport (V)

The card is the ISO-DEP target found by the reader-mode poll (ATQA replaced by 0x0EEF when
SAK bit 5 is set). Every READ or WRITE done by spot_reader is a complete session:
nfc_tag_read (read) / nfc_tag_write (write) open the PN512, re-select the card
(nfc_activate_type_a), then card_activate:

1. RATS `E0 50` (FSD 64, CID 0).
2. The ATS must hold `"TNPS064"` at bytes 5..11 (the historical bytes after TL T0 TA TB TC),
   else -2: only this card product is accepted. **An emu4u card must present these
   historical bytes**, which emu4u's source does not set (it is card configuration).
3. PPS `D0 11 0A` (CID 0, PPS1 = DSI 2 / DRI 2 = 424 kbit/s both ways), must be echoed
   `D0`; the PN512 is then switched to 424 kbit/s.
4. If card_auth_enable (always 1 here): AUTHENTICATE (below).
5. The READ / WRITE APDU (card_read / card_write), split in two APDUs when longer
   than 0x7FFE bytes.
6. S(DESELECT) `C2` (card_deselect).

I-blocks use PCB 0A/0B with CID 0 (block number card_block_number); frames are at most 64 bytes
with CRC, which is why WRITE puts 0x32 data bytes in the first block and 0x3C in the
next ones. Chained answers are acknowledged with R(ACK) AA/AB.

**There is no ISO 7816 SELECT anywhere** (no `00 A4` is built by any reader-mode
function). The earlier "SELECT by AID with the key D_255D378" (comments, OPEN.md) was wrong:
D_255D378 is the AUTHENTICATE data. The applet must therefore be the card's default
selected application (for emu4u: install it with the implicit-selection / default-selected
privilege; its own AID is 45 6D 75 34 55 01, which the toy never sends).

### APDUs (V)

All have CLA 0x90, as emu4u's `Constants.CLA`. P1P2 = card_file_no = the file number set by
card_set_params (P1 = 00; emu4u reads P2 as the file index, `resolveFile`).

| Name (emu4u) | Firmware | Command | Answer |
|---|---|---|---|
| AUTHENTICATE (`INS_AUTHENTICATE` 1B, `handleAuthenticate`) | card_activate | `90 1B 00 00 08 <AE 51 9F B5 8A 78 71 F4> 04` | 4 data bytes + SW. Data go to card_auth_answer, SW to card_status_word; neither is checked (the 4 bytes are fetched by card_last_result into a local of spot_reader that is never read: checked in asm/func/func_2514854.s). Only a status-only answer (no data) fails (-2). emu4u accepts any key and answers `0A 0B 0C 0D 90 00`. |
| READ (`INS_READ` AD, `handleRead`) | card_read | `90 AD 00 0F 00 00 03 o2 o1 o0 Lh Ll` (extended Lc = 3: the 24-bit byte offset; 2-byte Le = length) | data in chained I-blocks + SW |
| WRITE (`INS_WRITE` 8D, `handleWrite`) | card_write | `90 8D 00 0F 00 Ch Cl o2 o1 o0 data...` (extended Lc = 3 + length, no Le) | SW |

(0F = the file number, 02 or 03.) Status words, both directions: 9000 -> 0;
6700 (wrong length), 6982 (security status not satisfied, i.e. not authenticated),
6A80 (wrong data: emu4u when the offset/length is beyond the file), 6A82 (file not found:
emu4u `resolveFile`) -> -2; any other SW or a timeout -> -4 (and card_status_word is set to 9000,
so the s8 result has to be checked as well). emu4u never sends 6982, so a real card
presumably requires AUTHENTICATE first (I, medium).

emu4u matches the firmware's encoding: it implements `ExtendedLength`, requires Lc = 3 for
READ, takes the 3 data bytes as a 24-bit offset, and loops `receiveBytes` for long WRITEs.
emu4u's `dump_card.py` sends the same AUTHENTICATE bytes and uses short APDUs with P2 = 1..3.

Two more APDUs exist in the firmware, only behind uncalled functions (no call, no pointer
to card_apdu_f5_session / card_apdu_c4_session in the image), and emu4u does not implement them:

* `90 F5 P1 P2 00` (card_apdu_f5 via card_apdu_f5_session): expects 12 data bytes, returned as
  a 24-bit value, three bytes and two 24-bit values. Probably "get file info" (I, low).
* `90 C4 00 00 0C <8 bytes> <4 bytes>` (card_apdu_c4 via card_apdu_c4_session, after an
  AUTHENTICATE with a caller-supplied key): status only. A key change or the second half
  of a challenge-response (I, low).

### Card files (V for what the firmware uses)

The firmware uses files 2 and 3 (seven card_set_params calls in spot_reader, all with 2 or
3, always with key D_255D378). `dump_card.py` dumps P2 = 1..3, so real cards also have a
file 1, never accessed by the toy (its use is unknown).

**File 2, registration record, 0x3C bytes** (buf68 in spot_reader):

| Offset | Content |
|---|---|
| 0x00..0x01 | magic 00 80 |
| 0x02..0x03 | ? (not used) |
| 0x04..0x07 | use counter, u32 BE (0xFFFFFFFF = unlimited) |
| 0x08..0x1F | owner slot 1 (0x18 bytes, first byte 0xFF = free) |
| 0x20..0x37 | owner slot 2 |
| 0x38..0x39 | u16 BE byte sum of 0x00..0x37 (spot_check_sum(p, 0x3A), spot_sum) |
| 0x3A..0x3B | ? (rewritten unchanged with the checksum: the checksum WRITE is 4 bytes at 0x38) |

An owner slot is the owner name owner_name (0x10 bytes, 8 glyphs of the game charset) +
the owner's birthday as glyphs `04 6D+d`: month owner_bday_month as two digits, day owner_bday_day[0]
as two digits (the same values the profile page and the session record use for the
owner's birthday).

**File 3, content**, read in two steps: first the 0x30-byte header (buf38), then, for
a != 0, the whole file (length from the header) straight into flash 0x2050000.

| Offset | Content |
|---|---|
| 0x00..0x03 | total length v, u32 BE; accepted when 0 < v <= 0xF000 |
| 0x04 | kind: 0 or 0xFF for a download (else error); 0 or 6 for the bingo case (a = 0) |
| 0x05 | content type: 0x65 = block of characters / items (spot_content_browser), 0x66 = Bingo card (spot_bingo_card_screen); for a = 0 it must be 0x66 |
| 0x06 | bingo spot number (checked by spot_bingo_id_matches against the Bingo card, a = 0) |
| 0x07 | flags: bit 0 = counted, bit 1 = owner registration, bit 2 = not twice in a row from the same card UID (spot_last_uid) |
| 0x08..0x09 | parameter (byte 9 -> spot_bingo_number, values above 9 become 1) |
| 0x0A..0x0B | magic 00 80 |
| 0x22..0x23 | u16 BE: extra bytes between the 0x80-byte header and the characters (spot_content_header_field(1)) |
| 0x24..0x27 | u32 BE: size of the character area (spot_content_header_field(2)) |
| 0x28..0x29 | u16 BE: number of characters m (spot_content_entry_count(0)) |
| 0x2A..0x2D | u32 BE, not read by any code |
| 0x2E..0x41 | ten u16 BE counts; m + these = number of entries n (spot_content_entry_count(-1)) |
| 0x80 + [0x22] | characters: u16 length + character file, stride 0x4800, at most 2 (spot_content_character) |
| 0x80 + [0x22] + [0x24] | file list: u16 BE length + TAMAGO file, repeated (spot_content_file) |
| v-2..v-1 | u16 BE byte sum of bytes 0..v-3 (spot_check_sum(0x2050000, v)) |

The header fields 0x22.. are only read from flash after the download, so this layout is V
for the code; that real cards carry it is I (high: it is the only content path).

### Session logic in spot_reader (V)

`a` is the caller: 0 = the Bingo app (spot_bingo_gashapon_menu via spot_bingo_read), 1 = the
タッチスポット entry of the communication menu (spot_touch_menu_entry), 2 = the P06-low test start
at boot (sys_startup_flow -> spot_reader_mode2), 3/4/5 = engineering read / check / write
(test_spot_screen).

On each poll the function first erases flash sector 13, then polls 3 s. For an ISO-DEP
card:

1. AUTH + READ file 2 (0x3C bytes); magic, checksum and SW 9000 must be right.
2. AUTH + READ file 3 header (0x30 bytes); magic 00 80 at 0x0A.
3. a = 0 and type 0x66: Bingo spot check (spot_bingo_card_kind on the Bingo card at 0x2750000,
   spot_bingo_id_matches on the spot number) -> res 1. a != 0: AUTH + READ the whole file 3 into
   flash 0x2050000, checksum over v bytes -> res 3.
4. res 3, a = 1 and flag bit 1: **owner registration** (below). Else, (a = 1 or 5) and
   flag bit 0: **use counter**: 0 -> state 5 "ここからはもうもらえないよ"; else counter - 1
   is written (AUTH + WRITE file 2 offset 4, 4 bytes; AUTH + WRITE the checksum at 0x38;
   AUTH + READ back 0x3C bytes and compare; a = 5 writes -1 then +1 as a write test). Else
   flag bit 2 with the same UID as the previous touch -> state 3.
   res 1 (bingo, a = 0) goes through the same registration / counter logic (case 0/1).
5. Delivery: type 0x65 -> spot_content_browser(a), 0x66 -> spot_bingo_card_screen; then comm_count_add and
   the save save_game.

**Registration (max 2 owners)**: the 0x18-byte owner slot (name + birthday) is compared
with both slots of file 2. A match passes. If neither matches and both slots are used
(first byte != 0xFF), state 4 shows picture 0x211310C with the two names (bytes 0x18 and
0x30 of the buffer are zeroed to cut the birthday digits) and nothing is delivered. If a
slot is free: AUTH + WRITE the slot (offset 8 or 0x20), AUTH + WRITE the new checksum at
0x38, AUTH + READ back the 0x3C bytes and compare, then "あなたのなまえが
とうろくされたよ" (D_255D3E4 / D_255D3F8) and the delivery. So the same owner (name +
birthday) can use the card any number of times, a third owner is refused. A card uses
either the counter or the registration, not both (if / else-if in the code).

The write sequence is not atomic (slot, then checksum); a card torn away between the two
fails the checksum on the next touch (I, the code has no repair path).

### What the card delivers (V for the code, I for the real contents)

spot_content_browser pages through the entries: characters first (installed with dl_chara_store into
the 8 character slots at 0x2780000, 0x47FE bytes; dl_chara_name_protected refuses two protected
names), then the files of the list, classified by spot_content_file_kind (TAMAGO type/category at
0x4E/0x4F/0x51) and stored with item_receive (items 0x81 / 1-4, 7; games and outings
0x94 / 2 / 1-2), spot_install_bingo_card (Bingo card 0x81 / 8 -> 0x2750000) or spot_install_gashapon_ticket (Gashapon
ticket -> 0x2760000). Type 0x66 (spot_bingo_card_screen) installs file 0 as the Bingo card.

The sample payloads in `docs/4u-download-pack/cards/` fit this exactly: each card set is a
character (TAMAGO type 0x94 / category 2 / sub 3, 0x47DE bytes, i.e. one 0x4800 slot with
its length) plus 2-4 items: food 0x81/1, goods 0x81/2-4, room / pattern 0x81/7, outings
0x94/2/2, mini-games 0x94/2/1 (e.g. rolatchi: Rolatchi + moka plush 0x81/4 + heart
glasses 0x81/2 + purple dress 0x81/3 + touch living 0x81/7). All sets stay below 0xF000
with the 0x80 header and the length prefixes.

## 2. Flash 0x2050000 (sector 13)

Sector 13 = 0x2050000..0x205FFFF (sector table flash_sector_offsets: sectors 1..8 are 8 KB, 9.. are
64 KB; func_0000A54(base, first, last) is 1-based). Every code reference to 0x204F000..
0x205FFFF in the disassembled image (xld/xadd constants) and every func_0000A54 call with
13 or a range containing it was checked. The item banks (item_bank_tbl: sectors 0x63..0x78) and
the friend portrait sectors (9, 10) never reach it. The sector is erased in the dump
(FW_4UPlusJP.bin).

| User | When | Writes | Reads | Layout |
|---|---|---|---|---|
| spot_reader (F51) | Touch Spot / card reader (a = 0..5) | erases at every poll; programs card file 3 (<= 0xF000 bytes) via card_read + func_0000B52 | spot_check_sum over the block; spot_content_browser, spot_bingo_card_screen, spot_content_entry_count, spot_content_header_field, spot_content_character, spot_content_file, spot_content_file_length, spot_content_file_record, spot_content_file_picture, spot_content_file_name, spot_content_file_kind (all only reached from spot_reader) | the card content block (section 1) |
| friend_book_update (F66) | after a greeting session (type 2) | erase; 0xE00 bytes of the friend's portrait (friend_portrait) at 0x2050000 + the 0x80-byte profile at 0x2050E00 | friend_portrait_store(friend, 0x2050000 - 0x118) copies the 0xE80-byte portrait record into the portrait sectors; nfc_session.friend_data = 0x2050000 | 0xE00 picture + 0x80 profile |
| comm_store_pending_friend (via func_25039AA) | friend book after a full-book session: deleting a friend (friend_delete) with friend_book_state.new_friend_waiting set | - | the record at nfc_session.friend_data (still 0x2050000 after a greeting) -> friend slot 29 | same |
| comm_make_random_friend | never (no caller, no pointer: a debug helper making a random friend) | same as friend_book_update | same | same |
| friend_page (F53) | friend page, entries 1 (write a message) and 2 (message history) | erase; chara_render_pose_set(0, 0x2050000) renders our character's pose set 0 | ptr = 0x2050006 + be16(0x2050002) = picture 1, given to the message editor mail_card | picture set: u16 count, then per picture u16 length + picture |
| test_flash_erase_all (F56) | test mode flash reset | erases sectors 0xC..0xF (table test_flash_areas, entry 3: 0x2040000, 0x20000 halfwords), fills and verifies a pattern, erases again | - | - |

**Conclusion (V for the list, I high for the intent): sector 13 is a shared scratch
sector with no persistent content.** Each user erases it and consumes the data in the
same flow: the card block is processed inside spot_reader; the message-editor pictures
are used inside friend_page (local `loaded` flag); the friend portrait staging is copied
to the portrait sectors in friend_book_update itself, or a little later by comm_store_pending_friend, but
only through the friend book opened by comm_result_wait_book right after the session, where B
deletes friends (friend_delete) instead of opening friend_page, so nothing rewrites the
sector in between.

Earlier readings corrected:

* Fable "character creation record (comm_make_random_friend), 11 u16 counters at 0x2050028
  (spot_content_entry_count), NFC tag id list at 0x2050000+6+len (friend_page)": wrong. comm_make_random_friend
  is an uncalled random-friend debug helper, spot_content_entry_count reads the entry counts of the card
  content header, and friend_page renders our own pictures there.
* Opus "temporary TAMAGO / friend record and Touch Spot download target": right; plus
  the message-editor pictures.
* OPEN.md / comments "SELECT by AID with the key D_255D378": there is no SELECT;
  D_255D378 is the AUTHENTICATE data.

## 3. Open points

* File 1 of the card: present on real cards (dump_card.py reads it), never used by the
  toy. Contents unknown without a dump.
* The unused APDUs 90 F5 and 90 C4: their meaning (file info? key change?) needs a real
  card or the official writer software.
* Whether the in-store Touch Spots were the same ISO-DEP product (they could share the
  APDU path: content type 0x66 / kind 6 is the bingo spot case) or the Ultralight tags:
  item 4 of OPEN.md (other agent).
* Seen in passing, not in this topic: the Ultralight record is read at byte offset 0x10
  (nfc_type2_read uses page = b >> 2, so page 4), not "page 0x10" as the spot_reader
  comment says; and the test-mode table of test_flash_erase_all erases sectors 0xC..0xF.
