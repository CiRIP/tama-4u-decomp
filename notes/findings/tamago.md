# TAMAGO files: header layout, downloaded programs and their API (OPEN.md items 2, 13, 14)

Material: `docs/4u-download-pack/` (owner's, not in the repository): 917 `.jpg` files in 16
folders. Every file but one (`characters/peppertchi/peppertchi-banner.jpg`, a plain picture)
is a JPEG preview with the TAMAGO data appended after the JPEG end marker: 916 TAMAGO
files, 863 distinct. 270 files carry 2 extra bytes after the TAMAGO data (`00 00`, once
`33 33`), outside its size field.

Tool: `tools/tamago.py` (one line per file with the checks, `-v` every field, `--table`
the non-zero header bytes per type/category, `--extract DIR` the payloads). It reads the
signature tables from `FW_4UPlusJP.bin` and embeds no file contents.

"Verified" below = checked on all 916 files and/or read in the code; "inferred" = no
reader found in the firmware, meaning from the data only.

## 1. Header layout (item 2)

All multi-byte fields of the files are BIG-endian.

| Off | Size | Content | Status / consumer |
|---|---|---|---|
| 0x00 | 7 | `"TAMAGO\0"` | verified. Free-slot tests read `x000_h == 0x4154` ("TA") |
| 0x06 | 0x2A | file name, UTF-16BE, NUL-terminated or filling the field (182 files): `DL_T4US_<name>.jpg` items, `DL_T4UA_<name>.jpg` programs and characters; converted content `DL_iDNS_`/`DL_iDLS_`/`DL_iDNd_` (116), `DL_TPDS_` (12), `DL_MDP_` (3, two end in `.did`) | verified UTF-16BE at 0x06 (Fable's UTF-16LE at 0x07 was off by one). No reader (only the 6-byte magic is compared) |
| 0x30 | 4 | u32: size of the whole `.jpg` container (JPEG preview + TAMAGO data) | inferred: equals preview + data for 261 files, +1 for 227, the other 428 have a different preview in the pack. Not a checksum (Fable). No reader found |
| 0x34 | 20 | ASCII id `t4u_<kind><id>_<ver>`: `gh` meal, `oy` snack, `ac` accessory, `fk` clothes and characters, `as` goods, `bg`/`lv` rooms, `rec` bingo/gashapon, `gm` games, `dlode` outings; `id2_`/`idn_` for converted iD L content. 10 ids run past 0x44 | inferred. No reader found |
| 0x48 | 4 | total size (header + body + 2-byte checksum), BIG-endian | verified 916/916 = data length. comm_build_section_item reads it big-endian |
| 0x4C, 0x4D | 1+1 | `01 01` in every file | constant; in exchange records 0x4C is the session mode |
| 0x4E | 1 | type: 0x81 item, 0x94 program / character (1 = legacy format, only the ROM sample) | verified, item_store_in_bank / item_add_to_inventory / dl_install_program |
| 0x4F, 0x51 | 1+1 | category, sub-kind (see section 2) | verified |
| 0x50 | 1 | kind byte, fixed per category: 02 meal, 0D snack, 34 accessory, 33 clothes, 2A goods, 1F room, 29 bingo/gashapon, 5B game, 47 outing, 48 character; 01 on the four bingo/gashapon bonus "fridge" foods | verified values. Read by item_bank_next_slot (browser filter, 0x35 also matches 0x34), item_bank_free_slot (per-kind slot limit, snacks 13), item_same_identity (same-item test), comm_check_app_food (kind-1 food refused in session 8) |
| 0x52 | 8 | signature, key byte 0x55 (fable/sigcheck.py scheme) | valid in 916/916, but see section 3: the firmware never checks it on a file |
| 0x5A | 2 | item id (BE), 1001..9999; the last three digits classify coupons / decorations / bingo / gashapon (item_special_class) | verified; equals the digits of the ASCII id except for characters (1111..1183 vs `fk016xx`) |
| 0x5C | 2 | zero (one converted file: `26 3D`) | |
| 0x5E | 0x14 | display name in the game charset (`04 xx`), up to 10 codes, `00 00` end | verified at 0x5E (not 0x60): item_draw_name draws 9 codes, the town menu 8, F30 copies 0x14 bytes |
| 0x72 | 2 | `00 02` in every food file, else 0 | no reader found |
| 0x74 | 4 | price in Gotchi points (BE u32; 0 = free) | verified: item_price_field, item_price (sale discount) |
| 0x78 | 1 | goods only, equal to 0x79 in 82 of 156 | no reader found |
| 0x79 | 1 | use-animation id (< 0x4A, item_use_anims / item_liked_anims), goods only | item_use_anim_id |
| 0x7A | 1 | hunger hearts: meals 2, snacks 0..3 | item_hunger_effect |
| 0x7B | 1 | happiness hearts: snacks 2..3, meals 0 | item_happiness_effect |
| 0x7C..0x93 | | zero (stray bytes in 2 files) | |
| 0x94..0x9B | 8 | 32 x 2-bit liking per character class c = chara_class(id): bits 2*(c&3) of byte 0x94 + c/4; 1 likes, 2 dislikes | verified: item_pet_preference_raw / item_chara_preference / item_partner_preference index `0x7C + (c + 0x60) / 4` |
| 0x9C, 0x9D | 1+1 | part of the item identity (item_same_identity compares 0x5A, 0x5B, 0x9C, 0x9D) | zero in all files |
| 0x9E..0xA3 | | bingo / gashapon only: bingo `A2 = 64`; gashapon `9E 9F = 28 14`, `A2 A3 = 1E 0A` | OPEN: no reader found |
| 0xAE | 4 | program entry offset (BE): 0x204, 0x25E, 0x2AC, 0x2C6, 0x3EC, 0x43E | verified (town_menu, game_menu); 0 for characters (not code) |
| 0xB2 | 1 | weight change, food 1..6 | item_apply_effects |
| 0xB8..0xBC | 5 | trait increments 0..3 | item_apply_attributes -> pet_add_traits; game_menu applies 0x20000B8 after a rank-5 downloaded game |
| 0xC0..0xE3 | 0x24 | 18 BE character ids (bit 15 dislike, 1/2 = all of kind 0/1), consulted only when 0xFF bit 1 is set | all zero, bit 1 clear in every file |
| 0xF4..0xF7 | 4 | date window start month/day, end month/day (decorations, e.g. 02/01..02/03 setsubun) | item_seasonal_decoration (bank 15) |
| 0xFF | 1 | flags: 0x10 in all 916 (bit 4 = item file, required by comm_check_item_flag for the file in section 3; bit 0 = the exchange record's "complete" bit and bit 1 = use the 0xC0 list are clear) | verified |
| 0x100..0x1FF | | zero in all files | |
| 0x200.. | | body: u16 count n, then n x {u16 length, data} (item_load_pictures); accessories have a {u16 0x3C0, 0x3C0 bytes} block first. Meals/snacks 3 pictures, accessories 4, clothes 28, goods 1..8, rooms 1, bingo/gashapon 2 pictures followed by the prize records (item_prize_record), programs ONE blob = the program, characters one 0x8FC blob + sprite data | verified: the walk ends exactly at the checksum for every food/accessory/clothes/goods/room/program file |
| end - 2 | 2 | 16-bit additive sum of all preceding bytes, BIG-endian | verified 916/916 |

The firmware's own built-in records (ROM 0x2340000..0x23DFFFF, item_builtin_records) have the same
layout: big-endian size, valid big-endian sum, 0xFF = 0x10.

### Files vs exchange records: two byte orders (F70b claim checked)

The Tama-to-Tama / phone exchange records built in RAM at comm_tx_record (F70b) are a
different, little-endian container with the same magic:
- size: `*(u32 *)(comm_tx_record + 0x48) += n` (comm_build_phase1, comm_build_phase2, comm_build_phase3) is a
  native store, i.e. little-endian, and flash_program_bytes copies the header to flash unchanged:
  F70b's "little-endian here" is RIGHT.
- checksum: written by flash_fill / util_fill16 as a native halfword, i.e.
  LITTLE-endian. comm_validate_record reads the two bytes reversed and then byte-swaps them with
  util_read_be16 (disassembly 0x2529554..0x2529564), which is a little-endian read again. The
  comments of F70b ("checksum ... big-endian") and of comm_validate_record ("the big-endian u16")
  were wrong and are fixed.
- comm_build_section_item (section 3) reads the attached file's own size big-endian, as stored.

## 2. Categories, banks and consumers

Received items are filed by item_store_in_bank into the "download banks" item_bank_tbl[k] (the shop
stock of F36: the downloaded items are listed in the shops before the built-in stock and
bought for the price at 0x74). Sector k (1-based) is at 0x2000000 + flash_sector_offsets[k - 1]
(flash_sector_addr; sectors >= 9 are 64 KB at (k - 8) * 0x10000). Items given by an outing go
straight into the inventory instead (item_add_to_inventory, sectors 0x65 / 0x66 / 0x6B / 0x70 /
0x7C / 0x7D).

Files = count over the whole pack (all folders).

| Main pack folder | 4E/4F/51 | 0x50 | Files | Bank (item_bank_tbl) | Flash | Consumer |
|---|---|---|---|---|---|---|
| meals | 81/01/01 | 02 | 101 | 0: 0x800 x 20 | 0x25B0000 | feeding, F18 item_hunger_effect / item_happiness_effect |
| snacks | 81/01/02 | 0D | 160 | 1: 0x800 x 20 | 0x25C0000 | same |
| clothes | 81/03/00 | 33 | 65 | 2: 0x4000 x 5 | 0x2610000 | dressing (clothes on a mannequin in the item card) |
| accessories | 81/02/00 | 34 | 103 | 3: 0x2000 x 10 | 0x2660000 | |
| toys, promotions | 81/04/01 | 2A | 144 | 4: 0x2000 x 15 | 0x26C0000 | use: item_use (0x79 animation) |
| coupons | 81/04/01, id x601..x749 | 2A | 5 | 16 (class 0x61): 0x2000 x 1 | 0x2740000 | coupon picture 0x219A1E0 |
| decorations | 81/04/01, id x801..x949 | 2A | 7 | 15 (class 0x62): 0x2000 x 4 | 0x25A0000 | item_seasonal_decoration (date window 0xF4) |
| livings (rooms) | 81/07/00 | 1F | 215 | 6: 0x2000 x 10 | 0x26F0000 | F32 room browsers |
| bingogames | 81/08/00, id x301..x308 | 29 | 8 | 17 (class 0x64): 0x8000 x 1 | 0x2750000 | Bingo (F45/F49) |
| gashapon | 81/08/00, id x601..x620 | 29 | 7 | 18 (class 0x65): 0x8000 x 1 | 0x2760000 | Gashapon (spot_gashapon_draw, item_prize_record) |
| minigames, special | 94/02/01 | 5B | 24 | 7: 0x8000 x 2 | 0x2710000 | game corner game_menu |
| outings | 94/02/02 | 47 | 9 | 8: 0x8000 x 4 | 0x2720000 | town menu town_menu (entries 8/9/12/13) |
| characters | 94/02/03 | 48 | 68 | dl_chara_store, 8 x 0x8000 | 0x2780000 | downloadable characters |

`cards/` and `characters/` are bundles (a character with its items, room, game or outing).
No file in the pack uses bank 5 (81/04/02), bank 9 (81/06/01) or 81/06/03.

## 3. What the firmware checks on a file

- comm_validate_record (magic, size, sum, signature, 0xFF bit 0, version at 0x84) is the ONLY user
  of the signature tables D_25623D8 / D_2562458 and is called only by comm_exchange_initiator /
  comm_exchange_target on received EXCHANGE records. A bare TAMAGO file would fail it (its
  big-endian size read little-endian is huge, 0xFF bit 0 is clear).
- The file inside section 3 is checked only for 0xFF bit 4 (comm_check_item_flag) and, in session
  8, for kind-1 food (comm_check_app_food); then its type/category bytes decide the bank. Its own
  big-endian sum and its signature are never verified by the firmware (all 916 files have
  valid ones: the issuer computes them; whether the phone app checks them is unknown).

## 4. Downloaded programs and the API (item 13)

Programs: 33 files, 29 distinct (20 games incl. `special/EZ_Gotchi_9999_4U`, 9 outings).
The 67 distinct character files (94/02/03) are not code (entry 0).

How they run (verified in code and in the binaries):
- dl_install_program installs the file at 0x2000000: 0x204 + be16(0x202) bytes = the whole file
  minus the checksum (the body's single blob is the program), boot sectors 1..k+1, skipped
  when the installed copy has the same 0x51 / 0x5A / 0x5B.
- town_menu (bank 8, outings) and game_menu (bank 7, games) then call
  `((int (*)(void))(0x2000000 + be32(item + 0xAE)))()`: no arguments.
- The programs are linked for 0x2000000 INCLUDING the header: code starts at 0x2000204,
  their own data is addressed absolutely (touch-game `xld.w %r5,0x2002148`), firmware
  calls are pc-relative `ext/ext/call` to fixed addresses.
- Globals: RAM 0x84740..0x847BF, a block no firmware code addresses (the firmware's own
  RAM constants in that region stop at 0x845B8). They run on the firmware's stack.
- Return value: games 0 lost, 1..5 rank, > 5 raw Gotchi points (`EZ_Gotchi` is
  `xld.w %r4,0x270f; ret`: a 6-byte cheat that returns 9999 points); outings non-zero =
  "got it" animation, dl_chara_affection_update(0x2000000), save.
- 7 of the 9 outings carry complete TAMAGO item files (their prizes, e.g. `chubu` 4 items)
  inside the program image and hand them to api_store_item -> item_add_to_inventory (inventory).

Every firmware address the 28 calling programs use is in 0x2400AD0..0x2401D92: nothing
outside the block (their `call` targets and `xld.w` code pointers were collected over the
whole program blobs). Count = distinct programs (of 28) using the entry; FW = direct
callers in the firmware.

| Address | Forwards to | Meaning | Programs | FW |
|---|---|---|---|---|
| 2400AD0 / AFA / B20 / B2A | (libgcc) | __udivsi3 / __divsi3 / __umodsi3 / __modsi3 | 11 / 9 / 14 / 3 | many |
| 2400B50 / B5A / B64 | btn_a_presses/1/2 | read button A / B / C press count | 28 / 28 / 19 | 4-5 |
| 2400B6E / B78 / B82 | btn_a_presses/1/2 | set button A / B / C press count | 28 | 5 |
| 2400B8C | sys_idle_housekeeping | idle housekeeping | 16 | 3 |
| 2400BA8 | util_rand_except | random number | 22 | 2 |
| 2400BB4 | util_rand_shuffle | shuffle bytes | 5 | 3 |
| 2400C5E | tmr_delay_ms | delay ms | 17 | 2 |
| 2400C68 | tmr_delay_ms_t5 | delay variant | 1 | 1 |
| 2400C72 / C7C | tmr_t3_start_timeout / tmr_t5_start_timeout | start timeout T16 ch.3 / ch.5 | 6 / 12 | 1 / 4 |
| 2400C86 / C92 | tmr_flags | timeout ch.3 / ch.5 elapsed? | 6 / 12 | 1 / 4 |
| 2400C9E / CA6 | tmr_flags | clear timeout ch.3 / ch.5 | 6 / 12 | 1 / 4 |
| 2400CAE | snd_play | play sound | 19 | 5 |
| 2400CC0 | ui_new_screen | new screen with the icon bars | 16 | 4 |
| 2400CC6 | func_00009A6 | flush to the LCD | 16 | 4 |
| 2400CCE | chara_draw_pet | draw the pet | 1 | 2 |
| 2400CE8 | anim_busy | "screen owned" flag | 11 | 2 |
| 2400D12 | gfx_object_set_image | set image of display object | 16 | 2 |
| 2400D36 | gfx_object_move | move object | 3 | 0 |
| 2400D42 | gfx_object_move_dirty | move object (old + new area dirty) | 14 | 2 |
| 2400D4E | gfx_object_set_image_attr | change image pointer / attribute | 3 | 0 |
| 2400D74 | gfx_object_create | create display object | 16 | 2 |
| 2400DAE | anim_draw_pic | show still image in an animation slot | 14 | 0 (19 pointer uses) |
| 2400DC2 | anim_queue_sound | queue sound for the next frame | 12 | 18 |
| 2400DCA | anim_frame | end of keyframe (wait a ms, skip mask) | 14 | 14 |
| 2400E4A | (table) | picture number -> ROM picture address | 12 | 4 |
| 2401BAC | anim_scene_room | animation scene set-up (room) | 10 | 19 |
| 2401BC6 | anim_scene_window2 | scene set-up, animated background 2 | 6 | 0 |
| 2401C16 | anim_draw_pet | pet pose in an animation slot | 9 | 5 |
| 2401C6E | anim_draw_text | text line in a slot | 9 | 2 |
| 2401CF8 | anim_reaction0.. | one of nine reaction animations | 5 | 0 |
| 2401D72 / D82 | anim_pic_slots | write / read a shared game word | 9 / 9 | 0 |
| 2401D92 | item_add_to_inventory | put an item record into the inventory | 7 | 0 |

Games use the button / timer / object half (B50..DCA), outings the generated-animation
half (DAE, DCA, 1BAC, 1C16, 1C6E, 1CF8, 1D72/1D82, 1D92). Seven entries are used ONLY by
downloaded programs (D36, D4E, DAE as a call, 1BC6, 1CF8, 1D72, 1D82, 1D92): the block is
the downloadable-program interface (verified). Never used by any program in the pack:
B94 / B9E (button bits), BBA..C54 (pet stats, clock: programs get no direct stat access,
they return a result instead), CB8, CF4, CFA, D1E, D58, D90, D9C, DA6, DD2, DEC, E0A, E28,
E34, 1BB4, 1BBC, 1BCE..1C0E, 1C2C, 1C42, 1C58, 1C82, 1C96, 1C9E, 1CA6, 1CDE, 1CF0, 1D6A,
1DA0.

## 5. sys_apply_patch_list (item 14)

Applies a list of {address, mask, value} u32 triples until address 0: mask < 0xFF byte
and-or, 0xFF byte store, < 0xFFFF halfword and-or, 0xFFFF halfword store, < 0xFFFFFFFF
word and-or, 0xFFFFFFFF word store. No call, no code pointer (`xld.w`) and no data word
0x0250B3EA anywhere in the image; none of the 29 programs calls it (all their calls go to
the API block) and no payload contains the word in either byte order. Dead code as far as
the firmware and every known download go (verified); a program could still call it, since
nothing protects any address.

## 6. Corrections to earlier notes

- Fable section 8: name UTF-16LE at 0x07 -> UTF-16BE at 0x06; display name 0x60 -> 0x5E;
  size u32 LE -> BE; 0x30 "checksum?" -> container size; 0x78/0x79 "icon index" -> 0x79
  use animation; 0x4F 3 = clothes; 0xFF is 0x10 (bit 4), not bit 0.
- Fable / Opus "received files are verified by comm_validate_record": only the exchange records
  are; the files' own sum and signature are not checked.
- comm_validate_record / F70b: the record checksum is little-endian (native), not big-endian.
- Opus "type 8 refuses a few item kinds": comm_check_app_food refuses kind-1 food, which in the
  pack are exactly the bingo / gashapon bonus fridge foods.

## 7. Still open

- Readers of 0x30, 0x34, 0x4C/0x4D, 0x72, 0x78 (probably none: phone-app / tool fields).
- Bingo / gashapon bytes 0x9E..0xA3 and the prize-record layout after their pictures.
- Whether the phone app checks the files' signature and sum before wrapping them in
  exchange records (the app is not in the material).
