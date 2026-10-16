# Tamagotchi 4U+ firmware (FW_4UPlusJP.bin) - understanding notes

Confidence levels: HIGH = read the code and the data agree; MED = strong circumstantial
evidence; LOW = guess. Addresses are VMA (file offset + 0x2000000). "F<n>" = source file ids
from config/files.txt. See peripherals.md for the register-level hardware inventory and
symbols.tsv for proposed names.

## 1. Platform and boot (HIGH)

* CPU Epson S1C33E07 (C33 PE core); all peripheral accesses match the E07 manual. 29.49 MHz
  OSC3 (from UART baud reloads), 32 kHz OSC1 for the RTC.
* Memory: 20 KB internal RAM at 0 (0..0xD08 = code copied from 0x2562840, then .bss up to
  0x4AE4, stack top 0x5000), 2 KB DST RAM at 0x84000 (.bss 0x84000..0x84474, .data copied
  from 0x25626FC to 0x84474..0x845B8, more bss after). 8 MB x16 NOR flash at 0x2000000,
  AMD/JEDEC command set, 8 x 8 KB boot sectors then 127 x 64 KB (sector table flash_sector_offsets:
  sector k starts at flash_sector_offsets[k-1]; flash_sector_addr(k) returns its address).
* Reset vector sys_reset (asm/sys/sys_boot.s): CMU setup (OSC3, PLL off), moves the IVRAM
  to area 0 (pLCDC_IRAM=1), clears RAM 0..0x5000, sp=0x5000, sys_bus_setup (SRAMC waits),
  sys_gpio_init (ports), sys_init (system init: libc init func_24001B0/24002B6/240044A =
  malloc heap 0xD08..0x1108?, ITC sys_itc_init, timers tmr_init, 3 s delay
  tmr_t5_start_timeout(3000), copy IRAM code sys_copy_iram_code, copy .data sys_copy_data, zero bss
  sys_clear_bss_ram/24F657E, ADC sys_adc_setup/24F621C, enable interrupts sys_irq_enable (psrset 4)),
  nfc_session_clear (clear the NFC session struct nfc_session), then sys_main = main, which
  never returns.
* Interrupts (vector table asm/vectors.s, full list in peripherals.md): 3 buttons on P00-P02
  (vec 16-18), timer 3 cmp A/B (NFC timeout tick / flag), timer 4 cmp B (sound sequencer),
  timer 5 cmp A via RAM pointer ir_ram_routines (default tmr_t5_tick_handler) and cmp B (flag bit 5 of
  tmr_flags), SIO1 err/rx/tx (NFC UART ring buffers), A/D done, RTC 1 s tick (rtc_isr =
  the game clock), port 12 = PN512 IRQ.
* Low power: sys_sleep_osc1 switches the CPU to OSC1 (32 kHz) and sleeps (slp), sys_sleep_osc3
  returns to OSC3; sys_leave_low_power is "return to full speed and restore timers". The game
  sleeps between events (the big sleep/wake functions 24FE21C, 250DF58, 250BC60... write
  CTL0/2/3/5 = 2 (reset the tone timers) and poll P0D for buttons).

## 2. Display (HIGH for the bus protocol, MED for the engine details)

* 128x128 TFT with an ST7735-class controller on the external bus: command 0x600000, data
  0x600001, 18-bit colour (3 bytes per pixel). lcd_init_ili9163_old = init sequence (SWRESET, packed
  cmd/param table, SLPOUT, GAMMA E0/E1, DISPON), lcd_init_st7735 = second long init (other panel
  revision?), sys_display_off/2409068 = display on/off (2409068(int) is called everywhere to
  wake the screen / blank it), sys_power_down = power down (sleep in), lcd_init = set window
  (2A/2B/2C) + fill.
* Software rendering in internal RAM: a 256-colour palette in three 256-byte tables at
  0x3C10 (R), 0x3D50 (G), 0x3E90 (B) (+0x100 for a second palette set), an 8-bit line/tile
  buffer at 0x1810, dirty rectangle in 0x1805..0x180C (two rectangles: 1805-1808 and
  1809-180C). The IRAM blitters: func_0000000 (grow the dirty rect from a sprite record),
  func_000014A (pixel colour lookup: converts RGB565 to the panel format with several colour
  modes; 0x6000/0xA000/0x4000/0xE4C0/0xE5E0/0xE460 are "key" colours), func_0000312
  (render one sprite row into the line buffer), func_00004C8/func_0000586 (compose the sprite
  list gfx_objects (20 entries of 0x14 bytes, count at 0x1804) at (x,y)), func_00006B8 and
  func_000081A (flush a rectangle to the LCD: 2A/2B/2C then 3 bytes per pixel from the
  palette), func_00009A6 (flush both dirty rectangles and reset them = "present frame").
* Sprite list: 48 slots (0x1444..0x1804 / 0x14 = 48) of struct GfxObject at 0x1444 (count
  at 0x1804): x000 image pointer,
  x004 x, x006 y, x008 w, x009 h, x00a ?, x00c palette offset, x00e flags (0x4000 = second
  rectangle/layer, 0x2000 = colour inversion/modulo, 0x1000 = ..., 0x800 = colour-key,
  0x80 = ..., 0x40 = flip x, 0x20 = flip y, 0x10 = ...), x010 = row stride.
* Engine API (api/api_entries.c / U_2402xxx): gfx_object_create(x, y, layer/flags, flip, ?, image)
  creates a sprite (image = pointer into the graphics assets at 0x2080000.., 0x2340000..;
  image header: [0]=w, [1]=h, [2]=stride?, data from +4) and returns its slot id;
  gfx_object_move_dirty(id, x, y) moves it; gfx_object_set_flags(id, flags) changes flags; gfx_draw_text_line(id,
  x, y, layer, text, ...) / gfx_draw_text_centered draw a text string (game charset, 2 bytes per glyph
  "04 xx" = kana page 4) with the font; txt_draw_centered draws a string; gfx_set_background(colour)
  = background colour (RGB565: 0xFFFF white, 0xF800 red, 0xE7FC ...); gfx_display_reset /
  ui_new_screen = clear the sprite list / screen; anim_draw_pic(slot, image, flags, x, y) and
  anim_draw_text(slot, text, ..) are the animation-frame primitives used by the generated zone
  (slot 0 = background 0x40x0x24); anim_frame(ms, flag) = present and wait (animation
  frame delay); anim_queue_sound(n) = play sound effect n (MED); anim_scene_room(n)/func_2402582(n)
  = start an animation with n sprites (MED).

## 3. The generated zone 0x241D000-0x24F5000 (anim/anim_scripts.c) (HIGH)

Not scripts and not bytecode: hand/tool-generated C animation sequences ("cutscenes").
Each function is a straight line of frame descriptions: set sprite slot k to (image, flags,
x, y) with anim_draw_pic / anim_draw_pet (numbered character sprite) / anim_draw_text (text),
optional anim_queue_sound (sound), then anim_frame(500, 0) = show the frame for 500 ms. They
are called from the game logic (e.g. anim_happy, anim_owner_birthday, pet_event_chara_birthday from main).
There is no interpreter; the "script-like" look comes from the generator.

## 4. Sound (HIGH)

Piezo on the 16-bit timer outputs (TM0..TM3 on P10..P13, PWM). snd_ch0_on(period, duty,
mode) starts tone channel 0 (CLKCTL0 = MCLK/16, CR0B = period, CR0A = duty, CTL0 = 0x34 or
0x24), snd_set_pitch_0 updates its period, snd_ch0_off/25355E6 stop it; snd_ch1_on/2535554/
2535488/25355F4 = channel 1 (TM1), 253535A/2535566/25354B2/2535602 = channel 2 (TM2),
25353B2/2535578/25354DE/2535610 = channel 3 (TM3); snd_preset_timers (re)starts all. The note
period table at 0x24F529C (misdetected as code "func_24F529C": pairs 01 33 then 16-bit
periods descending by 2^(1/12) per entry) and the melodies after it ("02 33" header then
note/length bytes, 0x7F/0xBF/0xFF = rests/ties) are the sound bank; snd_tick_start(v) starts
the sequencer tick on timer 4 (compare B interrupt -> snd_tick_isr -> snd_sequencer_tick = the
sequencer step, which calls back through RAM pointer 0x84440 when a melody ends);
snd_tick_timer_stop stops it. snd_play(n) = play sound/melody n (0x4F = NFC success jingle,
0x19 = tag read OK, 0xC = error beep, 1/2/3 = the three button clicks; MED).

## 5. Buttons and input (HIGH)

P00/P01/P02 active low (A = left/select, B = middle/confirm, C = right/cancel). Port-input
ISRs btn_a_isr/25001E4/25002D6 debounce (poll the pin up to 0x600 times), count the hold
time in btn_a_debounce/1434/1436 (0x600 = long press), set btn_events bits 0/1/2, play the click
via snd_play(1/2/3), wake the display (sys_display_wake(1)). The game loop reads the
"pressed" flags btn_a_presses (A), btn_b_presses (B), btn_c_presses (C) and clears them; ui_timeout is
the menu inactivity countdown in seconds (decremented by the RTC tick; 0 = time out);
tmr_flags collects timer flags (bit 2 timer3 cmpB, bit 5 timer5 cmpB).
sys_idle_tick() = "idle one tick" (wait for an interrupt, service the clock); every menu loop
calls it. btn_mask(3)/2528AE0(7) mask/unmask the button interrupts (pINT_EK01_EP03).

## 6. Clock / RTC (HIGH)

RTC in the E07 (BCD counters). rtc_reset = RTC init, rtc_set_time(h, m, s?, ...) = set,
rtc_read_clock = read into rtc_month.. (year?, month rtc_month, day rtc_day, hour rtc_hour,
minute rtc_minute, second rtc_second), rtc_get_month/554/55C/564/56C = getters (day of week?,
hour, minute, second...). The 1 s ISR rtc_isr is the heartbeat of the game: it counts
sys_awake_seconds (seconds in the current minute, 0x4B0 = 20 min to auto-sleep ...), decrements the
menu timeout ui_timeout, the care timers pet_bath_request_timer/11BE/11C0 (u16 countdowns in seconds that
set pet_bath_expired/3FFB/3FFC "event pending" flags), the character timer pet.toilet_timer, reads
the RTC every ... and raises sys_debug_fast_forward/1425/1426 (minute/hour tick flags), and calls
snd_play/2409068 for alarms.

## 7. Flash data layout (HIGH where listed, from the writers/readers)

| Area | Sector(s) | Content |
|---|---|---|
| 0x2000000-0x2010000 | 0 (8K) .. 7 | boot sectors: sector 0 = this area is used by dl_install_program to install a type 0x94/2 item ("app/game program"?) starting at 0x2000000 (erases 1..k+1) - in this dump erased. 0x200E000 (sector 8?) = save_game writes the character state pet (0x124 halfwords) + the 30 Tama-friend records friend_list (0x294 halfwords) = the main SAVE (written after every important event) |
| 0x2040000-0x2050000 | 0xC | scratch/copy sector used by flash_backup_sector/250C06E (copy a whole item sector here, erase, write back minus one item = "delete item k") and by the mail code (2417EA4: 100 x 100-byte mail/message records) |
| 0x2050000-0x2060000 | 0xD | character creation record: comm_make_random_friend writes 0xE00 bytes (friend_portrait(template)) + 0x80 random stats; 0x2050028.. = 11 u16 counters (spot_content_entry_count); 0x2050000+6+len = NFC tag id list (24180D4) |
| 0x2070000 | 0xF | temp copy used by 2419890 (friend record delete) |
| 0x22D0000 | 0x35 | mail/message records (mail_store) |
| 0x2340000-0x23DC000 | 0x3C.. | preinstalled TAMAGO items (accessories "ac", goods "as"... with DL_T4Ud names) inside the asset area |
| 0x2570000-0x259E000 | 0x5F-0x61 | 24 x 0x2000 friend profiles ("たまとも" records, diary_page_count counts them, 241946C deletes) |
| 0x25B0000.. | 0x63/0x64 | food items (20 x 0x800 each bank) = item_bank_tbl[0..1] |
| 0x2610000 | 0x69 | 4 x 0x4000 (type 0x81/3) |
| 0x2660000 | 0x6E | 8 x 0x2000 (0x81/2 accessories) |
| 0x26C0000 | 0x74 | 14 x 0x2000 (0x81/4 goods "as") |
| 0x26D0000 | 0x75 | 1 x 0x2000 |
| 0x26E0000 | 0x76/0x77 | backgrounds (0x81/7, 10 slots of 0x2000) ("t4u_bg00001" found here) |
| 0x2750000, 0x2760000 | 0x7C/0x7D/0x7E | 0x8000 items of type 0x81/8 (the "Bingo card" / "Gashapon" app data, spot_bingo_card_kind/2516E3A check them) |
| 0x2790000-0x27C0000 | 0x81-0x83 | copy area for the friend profile sectors |
| 0x27A0400, 0x27C0000-0x27E0000 | 0x82-0x86 | character/game save banks: comm_fill_record loads from 0x27D0400 / 0x27D5400 / 0x27A0400 depending on ir_rx_packet.owner_id_hi flags; 0x27C0000 and 0x27D0400 checked for "erased" by comm_erase_session_buffers; test_program_sample_tamago writes the default record test_sample_tamago (a TAMAGO file "t4u_gh230_1" = default food?) to 0x27D0000; 0x27D0800.. per-game stats (2528xxx, 252B..); flash_write_sector_86 writes a buffer to 0x27E0000 (sector 0x86) |

Item slot directory item_bank_tbl (F36 data): 12-byte entries {u16 size, u8 sector, u8 count,
void *idlist, u32 ?}: [0] 0x800 x20 @0x63, [1] 0x800 x20 @0x64, [2] 0x4000 x4 @0x69,
[3] 0x2000 x8 @0x6E, [4] 0x2000 x14 @0x74, [5] 0x2000 x1 @0x75, [6] 0x2000 x10 @0x77 ...
item_bank_slot(cat, i) = address of slot i of category cat. item_add_to_inventory(item) = store a
received/bought item into the right bank (category from x04e/x04f/x051), item_store_in_bank /
item_receive = "use item" dispatch, item_bank_discard_browser = "inventory full, choose one to throw
away" UI, item_find_in_bank/250C0FA = find an item id in a bank.

### 7b. Corrections / additions from F53 (HIGH)

* Flash sector numbers are 1-based: sector k (1..0x87) starts at flash_sector_offsets[k-1]; 1..8 are
  the 8 KB boot sectors (0x2000000..0x200FFFF), 9..0x87 are 64 KB: sector k = 0x2000000 +
  (k-8)*0x10000 (9 -> 0x2010000, 0xC -> 0x2040000, 0x35 -> 0x22D0000, 0x5F -> 0x2570000,
  0x7F -> 0x2770000, 0x84 -> 0x27C0000, 0x85 -> 0x27D0000).
* 0x2570000..0x259E000 is NOT friend profiles: it is the PICTURE DIARY (えにっき /
  こそだてちょう): up to 23 pages of 0x2000 bytes (0x24 bytes text in 6 fields, w/h, the raw
  palettes of the sprites, then a 1-byte-per-pixel bitmap produced by the software
  rasteriser gfx_raster_pixel / diary_rasterize_page). Scratch copy at 0x2790000 (sectors 0x81..0x83).
* Mail (メッセ): 0x22D0000 (sector 0x35), 100 records x 100 bytes {s32 friend index, s32
  kind (0 ours/1 unread/2 read, -1 draft), name[0x14], body[0x40], pad[8]}; rewritten via
  the scratch sector 0xC (0x2040000). Editor/viewer mail_card, store/delete
  mail_store, lookup mail_find.
* Photo album: 6 slots pet.photo_slots[], records of 0x2800 bytes at 0x2770000 (sector
  0x7F). Tama-friends book: 80 records x 0x1000 at 0x22F0000 (sector 0x37), sorted by the
  u32 at +8.
* Friend portraits: 0xE80-byte records, 17 per sector, sectors 9..11 (0x2010000..).

## 8. TAMAGO item file format (HIGH for the header, MED for the body)

Every downloadable thing is a "TAMAGO" file (the Android app sends them as fake .jpg
files, the fixed name prefix "DL_T4Ud" + Japanese item name + ".jpg" is in the header):

| Off | Size | Field |
|---|---|---|
| 0x00 | 7 | "TAMAGO\0" |
| 0x07 | 0x29 | file name, UTF-16LE, "DL_T4Ud<name>.jpg" (checked with strncmp against D_255D47C / D_255F2B4 = "TAMAGO") |
| 0x30 | 4 | u32 LE: checksum? (0x99E, 0x984...) |
| 0x34 | 16 | ASCII id "t4u_gh00001_1" (gh = ごはん meal, oy = おやつ snack, ac = accessory, as = goods/toys, bg = background; _1 = version) |
| 0x44 | 4 | 0 |
| 0x48 | 4 | u32 LE total file size |
| 0x4C | 1 | 01 |
| 0x4D | 1 | 01 |
| 0x4E | 1 | type: 0x81 = item, 0x94 = ? (program/app: 0x94/2 can be installed at flash 0x2000000 by dl_install_program), 0x01 = ? |
| 0x4F | 1 | category: 1 food, 2 accessory, 3 ?, 4 goods, 6 ?, 7 background, 8 app data (bingo/gashapon) |
| 0x50 | 1 | sub-id / kind (0x35, 0x34, 0x2A, 0x1F...) |
| 0x51 | 1 | sub-category: food 1 = meal / 2 = snack; goods 1 = item / 3 = coupon |
| 0x52 | 8 | SIGNATURE (HIGH, verified on all 339 records of the dump with work/sigcheck.py): K = byte 0x55 (index 3 of the field); for i in 0..7, i != 3: byte[0x52 + PERM[K & 15][i]] == VAL[K >> 4][i] with PERM = D_2562458 (16 rows x 8: permutations of 0..7 whose index-3 entry is the row number) and VAL = D_25623D8 (16 rows x 8, index-3 entry = K & 0xF0). The other 7 bytes are a table-driven function of K, so only 256 valid signatures exist; the sender constants D_2562564 / D_2562570 (K = 0x97 / 0xF2) are two of them. Checked by comm_validate_record (F67) on everything received over NFC. Not cryptographic: the two 128-byte tables are all one needs to forge a record |
| 0x5A | 2 | item id, big-endian (used by item_get_id, the 1001..9999 ranges in F51 classify ids) |
| 0x5C | 4 | 0 |
| 0x60 | <=0x10 | display name in the game charset (pairs "04 xx"), 0000-terminated |
| 0x70.. | | per-category attributes: food: 0x73 = 2, 0x7A = ?, 0x94..0x9B hunger/happiness effect bytes; goods: 0x76-0x77 price (u16 BE: 300, 1000, 1500, 3000 Gotchi points), 0x78/0x79 icon index, 0x7B = 1; 0xB2.. flags, 0xFF = 0x10 |
| 0x100.. | | sprite data (header 0x03 0x01 w h ... then 4-bit paletted rows) |

Received files are verified by comm_validate_record: "TAMAGO" prefix, 0x100 <= length (u32 at
0x48) <= 0xF000, 16-bit additive checksum of all but the last two bytes == big-endian u16
in the last two bytes, the 0x52 signature above, bit 0 of byte 0xFF, record version u16 at
0x84 in {0, 1, 3}; then the type/category/kind whitelists (item_kind_whitelist and friends). No
cryptography anywhere.

Curiosities in F7's data (after the debug menu table comm_main_menu_debug_items, about 0x25381D4..): a
37-byte minimal JPEG (SOI, APP0 "JFIF", a DQT fragment, EOI) = the "fake .jpg" wrapper
template, and a legacy sample record "TAMAGO" + UTF-16LE "DL_MDP_ハンバーガー.jpg" (type 1 /
category 1 / kind 1 / sub-kind 1, size at 0x48 BIG-endian 0x4C8, the sender signature
constant 7C 68 48 97 75 A6 4E 99 at 0x52): the type-1 "old format" items that
item_kind_whitelist / ir_item_validate whitelist (MED: a leftover from a previous product's data).

### 8b. The Tamagotchi-to-Tamagotchi exchange record (comm/comm_exchange.c) (HIGH)

Same header layout, built in RAM at comm_tx_record and programmed into the send banks
0x27C0000 (phase 1), 0x27C0800 (phase 2), 0x27C0400 (phase 3); the peer's records land in
0x27D0000 / 0x27D0800 / 0x27D0400 (mode 2 = greeting stays in RAM, comm_rx_record, 0x180
bytes). Header fields: 0x4C session mode, 0x4D 4 (0 in mode 8), 0x52 signature constant,
0x5E message code (phase 1: 0/1, 0xE0/E1 loopback, 0xA0/A1 greeting; phase 2 by mode:
3 0x20/21, 4 0x22/23, 5 0x24/25, 6 0x28/29, 7 0x26/27, 8 0x10/11; phase 3 0x90/91;
initiator even, target odd; the receiver demands "mine +/- 1" and the same mode byte),
0x5F character family, 0x60 owner name (16), 0x72 owner id (owner_id, random u16 from
character creation; id + name identify a friend), 0x74 character id, 0x76 seconds the
target waits before its animation (keeps both screens in step), 0x78..
growth bytes, 0x84 version, 0x88 species group, 0x8C proposal-text half length, 0xB0 game
parameter, 0xB4 wish list (count, pose ids, 0xFF) of the character poses we want the peer
to RENDER for us, 0xC8 given-item descriptor + 0xCF/0xD0 money give/take, 0xD8 our Gotchi
points, 0xE0..0xEC directory (u16 offsets) of the sections of type 0..6, 0xF8 flags (0x10
Tamagotchi, 0x02 mode-8), 0xFF bit 0 valid. Sections from 0x100: {u32 type, u16 ver, u16
len} + data, 4-aligned: 0 = rendered portrait (pose 13, 0x30x0x48) + 0x80-byte profile at
+0xE08, 1 = the poses the peer asked for (0x30x0x30 bitmaps rendered by comm_render_picture_records from
our own sprite set - this is how a 4U+ displays a character it does not have), 2 = profile
(owner_profile_bytes + owner_profile_texts five 16-byte strings), 3 = a whole TAMAGO item file, 4 = a 100-byte
mail record, 5 = proposal text, 6 = more images. Modes (nfc_session.x003): 1 loopback test,
2 greeting (profile card + "あいしょうをうらなう?" compatibility fortune), 3/4 the
communication game (round 1 / round 2, scores in 0xB0 summed -> prize level
game_timing_result_anim), 5 give item (every 5th gift earns 500 points), 6 proposal / marriage
(comm_can_marry refuses: same family byte 0x5F, either side not adult (0x7B <= 2), the
peer's 0x86 != 0, or our species group 0x88 paired with theirs in comm_forbidden_pairs = a character
and its re-skinned twin; on success evo_after_marriage resets the life timers = new generation),
7 photo (album_make_photo composes the record), 8 item send (debug menu). Outcome handlers in
F66 (comm_run_outcome). The target side in the "any mode" state adopts the initiator's mode
but refuses 4, 6, 8 (0x58). Friend list friend_list: 31 records x 0x16 (x000 used, x001/x002
owner id, x003 flag, x004 name[16], x014 met-by bits, x015 portrait slot + 1), record 30 =
the current visitor; portraits are 0xE80-byte records in the sectors from 9 (17 per
sector, friend_portrait_store).

## 9. NFC: hardware and driver (HIGH)

NXP PN512 on UART (SIO ch.1, P04 = SIN1 / P05 = SOUT1), IRQ on P94 (FPT12, vec 88 ->
nfc_field_irq = 1), reset/power on P1 bits 3/4 (nfc_hw_power_on = reset pulse + power on,
nfc_hw_power_off = power off). All PN512 access is "address byte (bit7 = read), data byte" over
the UART, through the TX queue (nfc_uart_send(buf, n): queue n bytes, interrupt driven) and
the RX ring (nfc_uart_recv(buf, n, &err): wait for n bytes, 0x1869F spins timeout). The
command buffers in nfc_cmd_buf are sequences of (register, value) pairs; register 0x09 =
FIFOData, 0x0A = FIFOLevel (0x80 = flush), 0x0C = Control, 0x0D = BitFraming (0x80 =
StartSend, 0x87 = 7 bits = REQA), 0x01 = Command (0x0C transceive, 0x07 no change, 0x37 =
power down), 0x11 Mode, 0x12/0x13 Tx/RxMode (0xA1 = 212 kbps FeliCa framing, 0 = 106 kbps
Type A), 0x14 TxControl, 0x15 TxAuto, 0x18 RxThreshold, 0x19 Demod, 0x26 RFCfg, 0x27 GsN,
0x28 CWGsP, 0x29 ModGsP, 0x2A-0x2D timer, 0x1F SerialSpeed, 0x8A/0x88/0x86 = read
FIFOLevel/Status2/Error (the polling triple used everywhere: wait for FIFO >= n bytes,
modem idle, no error; Error & 0x40 (TempErr) -> return 3).
* nfc_init: full PN512 bring-up (ports, 9600 baud, wait for the chip, read its 10-byte
  NFCID from the FIFO (WrNFCIDtoFIFO) into nfc_id3 and force bytes 0/1 (and 8/9) to
  F0 40 = this toy's NFCID3 prefix, configure RF registers, power down).
* nfc_link_open / nfc_link_open_921k: wake the chip and switch the UART to 460800 / 921600 baud
  (PN512 SerialSpeedReg 0x3A / 0x1C); nfc_link_close: shut down (power down, 9600 baud);
  nfc_rearm_field_detect: wake + configure as listener and power down (nfc_sleep_until_field(k) = "arm the
  PN512 for card detection with RfOn/RfOff interrupt", nfc_wake_on_field_off chooses RfOff/RfOn).
* Timer 3 (nfc_tick_start start, nfc_tick_stop stop) gives the 10 ms tick nfc_ticks used for
  all NFC timeouts (timeouts are in 1/10 s: "d * 10"); nfc_abort = abort flag (set by the
  C button long press via nfc_user_abort -> nfc_request_abort, or by the ISR).
* Return codes (s8): 0 ok, 2 wrong card type, 3 overheat, 4 aborted by user, -2 protocol
  error, -4 timeout, -7 peer released, -15 chip did not answer, 0x39 too big, 0x3A flash
  write error.

## 10. NFC: protocols (HIGH)

Two personalities, selected by nfc_session.peer_kind (the peer type found by nfc_detect_initiator):
* 0xF040 = another Tamagotchi 4U: raw NFC-DEP (the toy is the target: nfc_detect_initiator answers
  ATR_REQ (D4 00) whose NFCID3 starts with F0 40 with ATR_RES (D5 01 + own NFCID3) at
  212 kbps FeliCa framing; then nfc_dep_receive receives DEP_REQ (D4 06) frames whose first
  frame carries a 32-bit LE total length and whose payload is written straight into flash
  (or RAM) at the destination, acknowledging each with DEP_RES (D5 07 PFB), handling DSL_REQ
  (D4 08 -> D5 09) and RLS; nfc_dep_send is the sender (initiator, D4 06 with the length
  prefix, 0x38/0x3C data bytes per frame, retries on NACK, D4 08 release on abort).
  This is the Tama-to-Tama "つうしん" transfer (friend profiles, gifts, games).
* 0xF0EF = anything else, i.e. a phone: the toy is the NFC-DEP INITIATOR at 106 kbps Type A
  (nfc_poll_type_a = REQA/anticollision/select of the phone as a Type A target, nfc_send_atr_req =
  ATR_REQ with the LLCP magic 46 66 6D and LLCP params VERSION 1.1, WKS 0x0013, LTO 0x96),
  then LLCP over DEP frames: PDU type = ((b0 & 3) << 2) | (b1 >> 6), DSAP = b0 >> 2,
  SSAP = b1 & 0x3F. nfc_snep_server = RECEIVE a SNEP PUT from the phone (phone CONNECTs to SAP 4
  = SNEP server: nfc_llcp_send_cc sends CC; I PDUs with the SNEP request: version/0x02 PUT,
  32-bit length; nfc_snep_store_fragment strips the NDEF record header (TNF/type/id, SR or long
  payload length) and programs the NDEF payload (= the TAMAGO file) into flash at the
  destination address (or copies to RAM); nfc_llcp_send_rr sends RR; nfc_snep_send_response sends the
  SNEP response 0x80 Continue / 0x81 Success; nfc_llcp_send_symm_req/25340DE = SYMM keepalives;
  nfc_llcp_send_disc_server = DM (reject a CONNECT to SAP 1/SDP)). nfc_snep_client = SEND to the phone
  (CONNECT to the phone's SNEP server DSAP 4 from SSAP 0x20, nfc_snep_send_put sends I PDUs
  with the data, 2534650 = DISC, 253474C = DM). So: Android Beam, SNEP PUT of an NDEF
  message whose single record payload is a TAMAGO file (the app names it DL_T4Ud...jpg).
* Reader mode for NFC tags/cards (nfc_activate_type_a = Type A poll + anticollision + select,
  ATQA in *b, 0xEEF substituted when SAK & 0x20 = ISO-DEP): MIFARE Classic (ATQA 0x0004/
  0x0002: nfc_mifare_read read / 25320AE write with key auth card_activate "TNPS064" ...),
  MIFARE Ultralight/NTAG (ATQA 0x0044/0x0810/0x0812/0x0816: nfc_type2_read / 2532650),
  ISO-DEP Type 4 (RATS card_activate, APDU read card_read / write 253284A in 0x7FFE
  chunks, deselect 25332B6, SELECT AID card_set_params(2/3, key "D_255D378")). Used by
  F51 (spot_reader: the "touch spot" / Bingo / Gashapon tag reader: reads a 0x30-byte
  record at page 0x10 of an Ultralight or a 0x3C-byte file on a Type 4 card, verifies a
  16-bit additive checksum (spot_check_sum), decrements a use counter and writes it back,
  registers the player's name on the tag...). test_spot_screen is the engineering tag R/C/W
  test screen ("TAG WRITE OK/NG").
* Application layer nfc/nfc_session.c: nfc_show_session_screen(n) = NFC status screens ("NFC INITIALIZE
  OK/NG"), nfc_find_peer_target = wait for a peer (target mode), nfc_power_up = init the chip,
  nfc_send(buf, len, timeout) = send (Tama or phone), nfc_receive(dst, &len, max, timeout,
  toflash) = receive (6 attempts for a phone, 0x28 x nfc_short_delay waits in between),
  nfc_session_target/252A690(mode) = the "つうしん" (communication) menu state machine (modes 1-7
  set nfc_session.x003 = 2,3,5,7,6,8 and x002 = 1 initiator / 2 target), nfc_session_initiator = the
  phone/"Tama 4U app" session, test_nfc_loopback = factory NFC loopback test (sends 0x400 bytes,
  receives 0x400 bytes forever).

## 11. Code execution from payloads: YES, by design (HIGH)

The "4Uアプリ" downloadable apps are NATIVE S1C33 CODE. Chain (all read in the source):

1. Menu 4Uアプリ -> comm_menu_app -> nfc_menu_session(6) = session mode 8 as target. In mode 8
   nfc_find_peer_target does not wait for NFC-DEP but reports the pseudo peer 0xF0F0 ("the app"),
   so the F70b exchange (comm_exchange -> comm_exchange_target) runs with the LLCP/SNEP transfer
   functions: nfc_receive = SNEP server (nfc_snep_server, the toy polls the phone as a
   Type A initiator, ATR_REQ with the LLCP magic, CONNECT to SAP 4, PUT) receives the
   phase-1 header record, nfc_send (nfc_snep_client SNEP PUT) answers with ours, then
   the phase-2 record whose section 3 is the TAMAGO file (nfc_snep_store_fragment strips the NDEF
   record header and programs the payload into the 0x27D0800 receive bank); after the
   checks (comm_validate_record on every record, comm_check_peer_phase / comm_check_item_flag / comm_check_app_food) the
   outcome handler comm_run_outcome -> comm_outcome_app -> item_receive -> item_store_in_bank (F34)
   files the item by type: type 0x94 / category 2 / sub-kind 2 goes to item bank 8 = four 0x8000-byte "app" slots in sectors 0x7A-0x7B
   (0x2720000..0x273FFFF; sub-kind 1 -> bank 7, sub-kind 3 -> dl_chara_store = a
   DOWNLOADABLE CHARACTER, stored by name in one of 8 slots of 0x8000 bytes at 0x2780000
   (sectors 0x80..0x83) and read back through dl_chara_record / dl_chara_file; such a record
   holds the character's table entry, name, 15 text groups of 0x96 bytes at +0x42.. and
   its sprites). The only checks are comm_validate_record
   (magic "TAMAGO", 0x100 <= size <= 0xF000, additive checksum, the table-driven 8-byte
   signature at 0x52 keyed by byte 0x55, bit 0 of 0xFF, version in {0,1,3}) and the
   type/category whitelist. No cryptography.
2. The town menu (F28 town_menu, table town_places entries 8 / 9 / 12 / 13 = app slots
   0..3, shown with the item's name at +0x5E) on "いく?" -> go:
       fn = item_bank_slot(8, slot);                  the app's TAMAGO record
       dl_install_program(fn);                            copy it to the boot sectors at 0x2000000
       entry = 0x2000000 + BE u32 at fn[0xAE..0xB1];
       r = ((int (*)(void))entry)();                 RUN IT
       if (r) { got-it animation; dl_chara_affection_update(0x2000000); save; }
   dl_install_program erases 8 KB sectors 1..k+1 and programs the record from 0x2000000 (size =
   BE u16 at +0x202 + 0x204), so the app binary executes in place from flash at
   0x2000000 + (its offset in the file); it runs with the firmware's full privileges
   (no MMU, interrupts live) and can call any ROM routine.
2b. A SECOND launch site: the Tama Depa game corner (game/game_menu.c game_menu) lists
   the items of bank 7 (type 0x94 / category 2 / sub-kind 1 = downloadable mini games;
   two 0x8000-byte slots in sector 0x79 = 0x2710000..0x271FFFF, also erased in this
   dump) after the built-in games and runs them the same way (dl_install_program + call of
   0x2000000 + BE u32 at item + 0xAE); the game's return value is its score / rank and
   pays Gotchi points, and the app's bytes at +0xB8 are applied as a stat vector
   (pet_add_traits). The idle-touch path (nfc_idle_touch) does not receive files.
3. Consequence: anyone who reproduces the signature tables (D_2562458 / D_25623D8, both
   in ROM) can build a TAMAGO 0x94/2/2 file with arbitrary code and get it executed by
   tapping a phone (or another Tamagotchi: the Tama-to-Tama exchange carries TAMAGO files
   in section 3 and stores them with the same item_store_in_bank). In this dump bank 8 is
   erased: no app was ever installed on this unit, so no app binary is available to
   study the expected entry conventions (return value != 0 = "the app granted
   something"; flash_base + 0x200.. holds a sub-record table read by comm_fill_record).

Other sinks checked and found benign: the RAM ISR vector ir_ram_routines (only ROM code is
copied there by sys_copy_ram_routines), the sound callback pointer 0x84440 (game code only), the
IRAM copy at boot. The 0x8000-byte type 0x81 / category 8 items (Bingo card, Gashapon
ticket: spot_app_record parses three lists of length-prefixed blobs then 0x68-byte records)
are data for the built-in mini apps.

## 12. Main program (room/room_main.c, sys_main) (MED)

Sets up the two sprite-list buffers (0x40/0x50 bytes at sp, diary_draft/E94), clears all state
flags (lcd_palette_set.., pet_toilet_expired..3FFC, pet_cleaning_reminder/C, save_request, room_idle, sys_battery_empty,
sys_display_state, sys_debug_fast_forward), draws the boot screen (lcd_init), wakes the display, restores
the saved game (snd_init = sound system init: tempo + note table for the clock and the
end-of-melody callback snd_end_callback_fn at 0x84440; then sys_idle_housekeeping x10 = ten idle ticks), decides the start state from pet_life_stage (character life stage 0..5) and pet.status
(status flags: bit 3/4 = sleeping/sick?), then enters the big event loop: a state variable
in r0/[sp+0x2a], the per-state screens (room_scene_egg/D_253EB3C/... = tables of (image, x, y)
for the room decoration, room_scene_current = current room table), the main menu (spot_bingo_gashapon_menu,
pet_habit_learned, pet_sitter_return, anim_christmas_play...), the hourly events (sys_debug_fast_forward/1425/1426 flags
from the RTC tick), sleep (pet_gone_screen) and so on. 3584 instructions; not fully traced.

## 13. Communication menus (comm/comm_menu.c) (HIGH for the tree, MED for the semantics)

Menus are tables of {label, handler} driven by ui_menu_run_plain(table, &index) (0 done / 1
chosen / 2 cancelled / 3 timeout) and the yes/no box ui_yes_no. Tree (labels from the
game text):

* comm_main_menu = the communication main menu (NFC icon): あいさツーしん (greeting, session
  kind 1), たまごっち4U (Tama-to-Tama -> comm_menu_tama4u), 4Uアプリ (smartphone download = session mode 8 -> comm_menu_app; nfc_idle_touch is instead the idle-touch handler called from main: a phone touching the idle toy gets the random "present" event comm_idle_touch_event
  = the SNEP app-link session), タッチスポット (touch spot tag, F51). Table comm_main_menu_items; when
  P06 reads low at entry the variant comm_main_menu_debug_items adds ╬デバックセット (debug set) and アイテムSEND.
  P06 is therefore a hidden test input (also counted by sys_sample_debug_strap into sys_debug_strap_count; == 8 at
  boot selects a special start path in main).
* comm_menu_tama4u "たまごっち4U" sub-menu comm_tama4u_menu_items: ツーしんゲーム (communication games, stakes
  100/500/1000$ via ir_point_gift/863E/86E2), アイテムこうかん (item exchange ->
  comm_menu_item_exchange -> あげる sub-menu comm_give_shop_item: リビング/ミニゲーム/おでかけさき/ごはん/おやつ
  banks, and もちもの comm_give_inventory_item: れいぞうこ/ようふく/アクセサリー/おどうぐ), きねんしゃしん
  (photo, comm_menu_photo -> mode 4), プロフこうかん (profile exchange, comm_menu_greeting -> mode 1),
  プロポーズ (comm_menu_proposal, needs life stage >= 3; modes 1/2), けっこん (comm_ask_marry).
* Every handler: nfc_menu_session(mode) runs the NFC session (F69), ir_power_down restores the
  ports, snd_wait_end waits for the sound, sys_battery_low battery check, then comm_result_wait_book
  (mail) when the partner slot comm_partner_slot == 30.
* ir_session(game mode)/func_24F94A0 (asm) = the exchange driver; result interpreted by
  ir_exchange.

## 14. Tama-to-Tama exchange (ir_exchange) (MED)

Both sides swap a 20-byte header (ours ir_tx_packet, theirs ir_rx_packet); x001 = message type in
request/answer pairs: 2/3 greeting, 8/9 gift (item record follows), 10/11 play together
(ir_rx_packet.owner_id_hi bits 6-7 = has the stamp card, x00e/x00f result or item index, bit 7 =
reject), 12/13 commemorative photo (x003 & 0x3F pose, x00c side), 14/15 proposal, 5 =
marriage (x00b = agreed game), 0x70 = refusal. Payload records land in the save banks
0x27D5400 / 0x27A5400 (profile, images at +0x14, partner header at +0x200) and 0x27C5428 /
0x27CB814 (items / portrait). Low-level transfer ir_transfer: status 0x20 ok, 0xAA user
abort, 0x10 retry. comm_is_initiator == 0 = we initiated. Outcomes: store items (item_add_to_inventory /
comm_item_exchange_result), add friend (friend_add_after_exchange + comm_load_visitor_pictures(1)), unlock mini games
(pet_add_happiness -> pet_clover_leaves bits), partner affection (visit_partner_add_traits: 5 values visit_partner[5..9]
capped by [0xB..0xF], favourite x004), save (save_game). Visitor timer visit_tick:
270 s then the visitor leaves (visit_end). Received record validation ir_validate_record:
big-endian size at 0x48 (<= 0x27D8 items, <= 0xD3D8 profiles) + additive 16-bit checksum
(ir_record_sum) over all but the last two bytes.

## 15. Bit-banged serial / IR: dead code from the Tamagotchi P's (MED)

The group ir_rx_start..func_2505112 (U_2504xxx.c, U_2505xxx.c) is a pulse-width serial
receiver/transmitter on P0 (ir_port_ptr = P0 data register): ir_rx_start arms it (ports to
GPIO ir_power_up, port-input-4 interrupt, timer 5 sampling 0x73 counts / prescaler 3 via
tmr_t5_start_tick), ir_recv_packet receives a 20-byte packet (start pulse ir_detect_start, bit decode
by pulse width ir_decode_bytes, additive checksum ir_checksum, errors 0x10/11/12 in
ir_result), ir_carrier_burst (copied to RAM 0xD58 by sys_copy_ram_routines) is the transmit loop toggling
the port with 0xE0 on each timer-5 tick (ISR count tmr_t5_ticks, ISR tmr_t5_tick_handler copied to
RAM 0xD94 = the vector-51 indirection). The edge-counting ISR it relies on (ir_edge_isr,
a port-4 handler) is NOT in this firmware's vector table (vector 68 = the halt stub), so the
receive path cannot work: it is the IR link of the Tamagotchi P's kept in the code base.
The 4U+ menus still call ir_power_down (idle the line) after every NFC session and
comm_prepare calls sys_copy_ram_routines at menu entry. The RAM ISR vector is only ever loaded with
ROM code.

## 16. Timers and power (HIGH)

| timer | role |
|---|---|
| 0,1,2,3 (TM0-TM3) | piezo tone channels (PWM); timer 2 doubles as the backlight PWM (TM2/P12) via sys_apply_backlight: period 0x44 (OSC3) / 0xF4 (OSC1), duty k*(9-level)/10, level 0..8 from sys_backlight_request (requested) vs sys_backlight_applied (current); sys_backlight_level = user brightness; sys_set_backlight(level) requests |
| 3 | NFC 10 ms tick (compare A, nfc_tick_isr) and the ms delay tmr_t3_start_timeout (compare B -> tmr_flags bit 2) |
| 4 | sound sequencer tick (compare B -> snd_tick_isr) |
| 5 | menu coarse tick: tmr_t5_standard_tick = period 0x989 at MCLK/4096 (about 0.34 s, compare B -> tmr_flags bit 5); one-shot ms delay tmr_t5_start_timeout(ms) (compare B = ms*30000>>12); serial sampling (compare A through the RAM vector) |

comm_exclusive_timing_on / comm_exclusive_timing_off bracket an NFC session: timers 3/5 reset and RTC interrupt off
/ on again + buttons re-armed (btn_rearm). sys_battery_low = battery check (10 AIN0 conversions,
low when > 4 readings <= 0x2E3/0x3FF) -> the caller shuts down with sys_power_down.
sys_irq_save_disable/2505026 and snd_irq_save_disable/25356D6 = PSR save-clear / restore critical sections.
sys_idle_housekeeping = per-tick service after each halt (P06 counter, backlight apply, care icon).

## 17. Character life logic (pet/pet_core.c) (MED)

Character record pet (0x248 bytes): x000 id, x002 table index (chara_table, one
entry per built-in character; downloaded ones through dl_chara_record), x005 hunger hearts
0..5 (6 overfed), x00a attention-call bits (1 hungry, 2 unhappy, 4 poop, 8 sleepy, 0x10 /
0x40 birthday calls, 0x80 sick/evolve), x00c happiness hearts (cap 30 / 100), x00f care
counter, x010 poop count, x011 status (1 poop on screen, 2 neglected, 4 collapsed /
asleep, 8, 0x10 "gone", 0x40/0x80 sick), x012 / x006 sick / starving timers (0x870 s =
36 min ladder), x014 age in seconds, x018 Gotchi points (max 99999), x01c growth step
with a care grade per step at +0x37, x06a room, x06c/x06d worn accessory / clothes, x0ac
= 0x4E achievement bits (0x1D.. raised species, 0x3D.. decorations, 0x45.. outings),
x0b6[6] photo album, x11A.. stats. owner_bday_day+3 = 2-bit ownership counts of 0x61 item
slots. pet_weight mood 0..99 with a per-character floor. Life tick pet_minute_tick once a
second (egg hatches, baby poops at 10/45 s and evolves at 60 s, child at 49 min; hunger
and happiness decay at table rates x012 / x013; sick ladder), pet_clock_events once a minute
(7:00 daily reset, midnight age, birthdays, Christmas), pet_care_mistake care-miss / sickness
roll, pet_sleep_state sleep schedule from the table bedtime/wake hours. The raised-characters
book (sector 0x37 records, album_register) stores a snapshot of each adult reached.
Communication games unlock through received items (pet_fill_clover_leaf, pet_clover_leaves bits).

## 18. The town (town/town_places.c) (MED)

Town menu table town_places (16 x {name, availability, pre, handler, post}): たまデパ (buy
through the bank browser / sell: shop_tama_depa, floors ゲームコーナー / リフォーム / アイテム
ショップ / たまファッション), こうえん (ひろば square encounters by hour and day: town_park_square_event
+ handlers D_0084498; うんどうじょう training game game_training with 20..200 point prizes),
がっこう (five clubs with per-character talent limits D_25424F8, counters at pet +
0x208), おみあいじょ (matchmaking: three candidate species, wedding, new generation),
たまのうえん, であいストリート (pets: items 44..48, adoption into pet_companion), and the four
app slots. Part-time jobs (job: 250 points per hour, 30 shifts, reward item), the
lottery (flags 0x14/0x15/0x16 = prizes incl. the eco car used in travel scenes),
transformations (chara_transform_target: item-gated alternate ids 0x30..0x3C), outing stories
(outing 2..6 -> diary pages 4000..4004). The generic 4-per-page menu driver is
ui_place_menu_run.

## 19. Coverage and open questions

Annotated (comments in src/*.c, 16 commits on branch worktree-agent-a71014bdbfb9dd236):
hardware init and ISRs (F4x/U_24F6xxx, U_25000xx, timers, backlight, sound, RTC, battery),
main (F21), display/animation primitives (F40, F8a, F8b, F0 header), the NFC stack
(PN512 UART driver U_252Cxxx/252Dxxx, NFC-DEP lib/nfc/nfc_dep/252E5D0/252ECE0, LLCP/SNEP F70d,
reader mode lib/nfc/nfc_tags / MIFARE / ISO-DEP, app layer F69, exchange F70b, outcomes F66,
record checks F67), TAMAGO storage (F34, F36), communication menus (F7), touch spots
(F51), Bingo/Gashapon (F45, F49, F63), character logic (F30, F4, F5, F9, F18, F24,
F28, F29, F25, F44, F47, F46, F48), notebooks / mail / diary (F53, F52, F55, F16),
downloadable characters (F20b), menus (F23), mini games (F65, F62, game/game_menu),
renderers (chara/chara_render_flash, chara/chara_render_flash, chara/chara_render_flash). Not read in detail: the 3400 generated
animation functions (F0), the remaining asm-only functions (main sys_main beyond its
header, ir_session_run serial exchange driver, F65/F62 game loops, comm_render_pose_list/BD2,
item_special_menu, the F21 room tables), F43 (farm/street handlers 2508FEE..), F42 events, and
the small U_ files that still carry only the reconstruction header.

Open questions:
* The TAMAGO header u32 at 0x30 (0x99E, 0x984 ...: a CRC of the body? not checked by
  the firmware as far as seen) and the exact semantics of the per-category attribute
  bytes at 0x70..0xFF (only the fields the code touches are named).
* No 0x94 program item exists in this dump, so the app binary conventions (what the
  entry receives, how it draws, how it returns its score) are inferred from the
  launch sites only; an actual app from the smartphone application would settle it.
* Many D_0001xxx bytes of the character record are named only by their effect
  (see NOTES 17); the diary / exchange "variant" codes and several text-group ids are
  described by position, not meaning.
* The old serial protocol (ir_session_run / ir_transfer, ir_rx_packet/DD0 headers) is only
  summarised; it may be unreachable on the 4U+ (no serial peer exists) and is probably
  inherited code like the IR group.
